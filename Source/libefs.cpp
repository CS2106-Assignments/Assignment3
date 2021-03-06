#include "libefs.h"
#define UNASSIGNED_BLOCK 0
#define MAX_OFT_ENTRY 1024

// FS Descriptor
TFileSystemStruct *_fs;

// Open File Table
TOpenFile *_oft;
bool *_oftMap;

// Open file table counter
int _oftCount = 0;

int openFileIsFound(unsigned int oftEntry, unsigned int fdIdx, const char *filename, unsigned char mode); 
int openFileCreateNewFile(unsigned int oftEntry, const char *filename, unsigned char mode);
void updateOftEntry(unsigned int oftIdx, unsigned char openMode, unsigned int writePtr, unsigned int readPtr, unsigned int filePtr, const char * filename);
void clearOftEntry(unsigned int oftIdx); 
unsigned int getFreeOftEntry();
void writeFileIntoWriteBuffer(TOpenFile *openFile,void *buffer, unsigned int writeSize);
void fillUpRestOfWriteBuffer(TOpenFile *openFile, void *buffer, unsigned int writeSize);
void writeFinishFileOntoDisk(TOpenFile *openFile, void *buffer, unsigned int writeSize);
void readDataIntoBuffer(TOpenFile *openFile, void *buffer, unsigned int readSize);
void readRestOfReadBuffer(TOpenFile *openFile, void *buffer);
void readRestOfFileFromDisk(TOpenFile *openFile, void *buffer, unsigned int readSize);
void updateFileLen(TOpenFile *openFile, unsigned int writeSize);
void freeAllOft();

// Mounts a paritition given in fsPartitionName. Must be called before all
// other functions
void initFS(const char *fsPartitionName, const char *fsPassword)
{
    mountFS(fsPartitionName, fsPassword);
    _fs = getFSInfo();
    _oft = (TOpenFile *) malloc(sizeof(TOpenFile) * MAX_OFT_ENTRY);
    _oftMap = (bool *) malloc(sizeof(bool) * MAX_OFT_ENTRY);
}

// Opens a file in the partition. Depending on mode, a new file may be created
// if it doesn't exist, or we may get FS_FILE_NOT_FOUND in _result. See the enum above for valid modes.
// Return -1 if file open fails for some reason. E.g. file not found when mode is MODE_NORMAL, or
// disk is full when mode is MODE_CREATE, etc.
int openFile(const char *filename, unsigned char mode)
{
    _oftCount += 1;
    if (_oftCount > MAX_OFT_ENTRY) {
        _oftCount = MAX_OFT_ENTRY-1;
        return OPEN_FILE_TABLE_FULL;
    }
    unsigned int fileIdx = findFile(filename);
    unsigned int freeOftEntry = getFreeOftEntry();
    _oftMap[freeOftEntry] = 1;

    if (_result != FS_FILE_NOT_FOUND) { // If found
        return openFileIsFound(freeOftEntry, fileIdx, filename, mode);
    }

    if (mode == MODE_CREATE) { // Create a file if mode is MODE_CREATE
        return openFileCreateNewFile(freeOftEntry, filename, mode);
    }
    _oftCount -= 1;
    return FS_FILE_NOT_FOUND;
}

// Write data to the file. File must be opened in MODE_NORMAL or MODE_CREATE modes. Does nothing
// if file is opened in MODE_READ_ONLY mode.
void writeFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    if (_oft[fp].openMode == MODE_READ_ONLY || _oftMap[fp] == 0) {
        return;
    }
    TOpenFile *openFile = &_oft[fp];
    unsigned int totalSize = dataSize * dataCount;
    unsigned int fileIdx = findFile(openFile->filename);
    unsigned long freeBlock = -1;
    unsigned int bytesLeftInBuffer = _fs->blockSize - openFile->writePtr;
    unsigned int writeSize = 0;
    loadInode(openFile->inodeBuffer, fileIdx);
    // If the buffer is still not full
    if (totalSize < bytesLeftInBuffer) {
        writeFileIntoWriteBuffer(openFile, buffer, totalSize);
    } else { 
        // Once buffer is full
        fillUpRestOfWriteBuffer(openFile, buffer, bytesLeftInBuffer);
        writeFinishFileOntoDisk(openFile, buffer, totalSize - bytesLeftInBuffer);
        updateFileLen(openFile, totalSize);
    }
    saveInode(openFile->inodeBuffer, fileIdx);

    updateFreeList();
    updateDirectory();
}

// Flush the file data to the disk. Writes all data buffers, updates directory,
// free list and inode for this file.
void flushFile(int fp)
{
    TOpenFile *openFile = &_oft[fp];
    if (openFile->writePtr > 0) { // If there is some data to write to disk
        unsigned int fileIdx = findFile(openFile->filename);
        unsigned long fileLen = getFileLength(openFile->filename);
        unsigned int needNewBlock = fileLen % _fs->blockSize;
        if (!needNewBlock) { // If a new block of memory is needed
            unsigned long freeBlock = findFreeBlock();
            markBlockBusy(freeBlock);
            writeBlock(openFile->buffer, freeBlock);
            loadInode(openFile->inodeBuffer, fileIdx);
            unsigned int curFileLen = fileLen + openFile->writePtr;
            // Find the inode to put in
            setBlockNumInInode(openFile->inodeBuffer, curFileLen, freeBlock);
            saveInode(openFile->inodeBuffer, fileIdx);
            // Update file length
            updateDirectoryFileLength(openFile->filename, curFileLen);
        } else { 
            unsigned long freeBlock = returnBlockNumFromInode(openFile->inodeBuffer, fileLen);
            char *buffer = makeDataBuffer();
            unsigned int writeItr = fileLen/ _fs->blockSize;
            unsigned int offsetWritePtr = fileLen - writeItr * _fs->blockSize;
            loadInode(openFile->inodeBuffer, fileIdx);
            readBlock(buffer, freeBlock);
            memcpy(buffer + offsetWritePtr, openFile->buffer, openFile->writePtr);
            writeBlock(buffer, freeBlock);
            openFile->inodeBuffer[writeItr] = freeBlock;
            saveInode(openFile->inodeBuffer, fileIdx);
            free(buffer);
            unsigned int curFileLen = fileLen + openFile->writePtr;
            updateDirectoryFileLength(openFile->filename, curFileLen);
        }
    }
    _oft[fp].writePtr = 0;
    _oft[fp].readPtr = 0;
    _oft[fp].filePtr = 0;

    updateFreeList();
    updateDirectory();
}

// Read data from the file.
void readFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    TOpenFile *openFile = &_oft[fp];
    unsigned int totalReadSize = dataSize * dataCount;
    unsigned int dirIdx = findFile(openFile->filename);
    unsigned long blockNumber = -1;
    loadInode(openFile->inodeBuffer, dirIdx);
    unsigned int readFileLen = 0;
    unsigned int filePos = 0;
    unsigned int bytesLeftInBuffer = _fs->blockSize - openFile->readPtr;
    if (totalReadSize < bytesLeftInBuffer) {
        readDataIntoBuffer(openFile, buffer, totalReadSize);
    } else {
        readRestOfReadBuffer(openFile, buffer);
        readRestOfFileFromDisk(openFile, buffer, totalReadSize - bytesLeftInBuffer);
    }
    updateFreeList();
    updateDirectory();
}

// Delete the file. Read-only flag (bit 2 of the attr field) in directory listing must not be set. 
// See TDirectory structure.
void delFile(const char *filename) {
    // Check if the file has a bit 2 in attr field
    unsigned int attr = getattr(filename);

    // Read-only flag is true (bit 2 of attr field is set to 1)
    if ((attr>>1) & 1) {
        return;
    }
    unsigned int dirIdx = findFile(filename);
    // Calc the total of inode that we will have
    unsigned int fileLen = getFileLength(filename);
    unsigned int numFullBlocks = fileLen / _fs->blockSize;
    // Check if there is left over bytes that are in the blocks
    unsigned int isMoreBytes = fileLen - (numFullBlocks * _fs->blockSize);
    unsigned int totalInodeCount = numFullBlocks;
    if (isMoreBytes) {
        totalInodeCount += 1;
    }
    unsigned long *inodeBuffer = makeInodeBuffer();
    loadInode(inodeBuffer, dirIdx);

    int i;
    for (int i = 0; i < totalInodeCount; i++) {
        unsigned long blockNum = inodeBuffer[i];
        markBlockFree(blockNum);
        inodeBuffer[i] = UNASSIGNED_BLOCK;
    }
    delDirectoryEntry(filename);

    updateFreeList();
    updateDirectory();
}

// Close a file. Flushes all data buffers, updates inode, directory, etc.
void closeFile(int fp) {
    // Flush modified buffers to disk
    TOpenFile openFile = _oft[fp];
    _oftCount -= 1;
    flushFile(fp);
    // Release buffers
    clearOftEntry(fp);
    // Update file discriptors
    // Free oft entry
    _oftMap[fp] = 0;
}

// Unmount file system.
void closeFS() {
    // Go into every oft entry and free (close) everything if possible
    freeAllOft();
    free(_oftMap);
    free(_oft);

    unmountFS();
}

// Obtains the free entry on the open file table
unsigned int getFreeOftEntry() {
    int i;
    for (i = 0; i < MAX_OFT_ENTRY; i++) {
        if (_oftMap[i] == 0) {
            break;
        }
    }
    return i;
}

// Frees all of the oft table entries.
void freeAllOft() {
    int i;
    for(i = 0; i < MAX_OFT_ENTRY; i++) {
        if (_oftMap[i] == 1) {
            _oftMap[i] = 0;
            closeFile(i);
        }
    }
}

// Opens the file is it is found, returns the index of the open file table
int openFileIsFound(unsigned int oftEntry, unsigned int fdIdx, const char *filename, unsigned char mode) {
    updateOftEntry(oftEntry, mode, 0, 0, 0, filename);
    return oftEntry;
}

// Creates the file if it is not found, returns the index on the open file table
int openFileCreateNewFile(unsigned int oftEntry, const char *filename, unsigned char mode) {
    unsigned int fileIdx = makeDirectoryEntry(filename, 0x0, 0);
    updateOftEntry(oftEntry, mode, 0, 0, 0, filename);
    if (_result == FS_FULL) { // ERROR when disk is full
        _oftMap[oftEntry] = 0;
        clearOftEntry(oftEntry);
        _oftCount -= 1;
        return FS_FULL;
    }
    updateFreeList();
    updateDirectory();
    return oftEntry;
}

// Updates entire oft entry. 
void updateOftEntry(unsigned int oftIdx, unsigned char openMode, unsigned int writePtr, unsigned int readPtr, unsigned int filePtr, const char * filename) {
    // Update oft
    _oft[oftIdx].openMode = openMode;
    _oft[oftIdx].blockSize = _fs->blockSize;
    _oft[oftIdx].inodeBuffer = makeInodeBuffer();
    _oft[oftIdx].buffer = makeDataBuffer();
    _oft[oftIdx].writePtr = writePtr;
    _oft[oftIdx].readPtr = readPtr;
    _oft[oftIdx].filePtr = filePtr;
    _oft[oftIdx].filename = filename;
}

// Clears the oft entry
void clearOftEntry(unsigned int oftIdx) {
    _oft[oftIdx].openMode = 0;
    _oft[oftIdx].blockSize = 0;
    _oft[oftIdx].inode = 0;
    free(_oft[oftIdx].inodeBuffer);
    free(_oft[oftIdx].buffer);
    _oft[oftIdx].writePtr = 0;
    _oft[oftIdx].readPtr = 0;
    _oft[oftIdx].filePtr = 0;
    _oft[oftIdx].filename = NULL;
}

// Writes file into the write buffer and updates the write pointer.
void writeFileIntoWriteBuffer(TOpenFile *openFile,void *buffer, unsigned int writeSize) {
    memcpy(openFile->buffer + openFile->writePtr, buffer, writeSize);
    openFile->writePtr += writeSize;
}

// Fills up the rest of the write buffer as much as possible.
void fillUpRestOfWriteBuffer(TOpenFile *openFile, void *buffer, unsigned int writeSize) {
    memcpy(openFile->buffer + openFile->writePtr, buffer, writeSize);
    unsigned long freeBlock = -1;
    if (openFile->openMode == MODE_READ_APPEND) {
        freeBlock = findFreeBlock();
        markBlockBusy(freeBlock);
        writeBlock(openFile->buffer, freeBlock);
    } else {
        freeBlock = openFile->inodeBuffer[0];
        if (freeBlock == UNASSIGNED_BLOCK) {
            freeBlock = findFreeBlock();
            markBlockBusy(freeBlock);
        }
        writeBlock(openFile->buffer, freeBlock);
    }
    // Reset write ptr to 0
    openFile->writePtr = 0;
    openFile->filePtr = writeSize;
}

// Write the rest of the data into the disk.
// Data that is not able to fully fit into the blockSize will remain in the write buffer.
void writeFinishFileOntoDisk(TOpenFile *openFile, void *buffer, unsigned int writeSize) {
    // Continue to read blocks of file continuously
    unsigned int numWriteItr = writeSize / _fs->blockSize;
    unsigned int i = 0;
    unsigned int currentInodeToLookAt = 1;
    unsigned long freeBlock = -1;
    while (i < numWriteItr) {
        memcpy(openFile->buffer, buffer + openFile->filePtr, _fs->blockSize);
        if (openFile->openMode == MODE_READ_APPEND) {
            freeBlock = findFreeBlock();
            markBlockBusy(freeBlock);
        } else {
            freeBlock = openFile->inodeBuffer[currentInodeToLookAt];
            if (freeBlock == UNASSIGNED_BLOCK) {
                freeBlock = findFreeBlock();
                markBlockBusy(freeBlock);
            }
            writeBlock(openFile->buffer, freeBlock);
            currentInodeToLookAt += 1;
        }
        writeBlock(openFile->buffer, freeBlock);
        openFile->filePtr += _fs->blockSize;
        i += 1;
    }
    openFile->writePtr = writeSize - (i * _fs->blockSize);
    memcpy(openFile->buffer, buffer + openFile->filePtr, openFile->writePtr);
}

// Reads the data from the read buffer into the output buffer when the data to read is smaller than the readbuffer.
void readDataIntoBuffer(TOpenFile *openFile, void *buffer, unsigned int readSize) {
    unsigned long blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, readSize);
    if (openFile->readPtr) {
        memcpy(buffer, openFile->buffer, openFile->readPtr);
        openFile->filePtr = openFile->readPtr;
    } 
    readBlock(openFile->buffer, blockNumber);
    memcpy(buffer + openFile->filePtr, openFile->buffer, readSize);
}

// Reads the rest of the data from the read buffer onto the output buffer.
void readRestOfReadBuffer(TOpenFile *openFile, void *buffer) {
    if (openFile->readPtr) {
        memcpy(buffer, openFile->buffer, openFile->readPtr);
        openFile->filePtr = openFile->readPtr;
        openFile->readPtr = 0;
    }
}

// Reads the rest of the data on the disk into the output buffer.
void readRestOfFileFromDisk(TOpenFile *openFile, void *buffer, unsigned int readSize) {
    // Number of read iterations
    unsigned int curFileLen = readSize - openFile->readPtr;
    unsigned int readItr = curFileLen / _fs->blockSize;
    unsigned int i = 0;
    unsigned int readFileLen = openFile->filePtr;
    unsigned int blockNumber = -1;
    while (i < readItr) {
        readFileLen += _fs->blockSize;
        blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, readFileLen);
        readBlock(openFile->buffer, blockNumber);
        memcpy(buffer + openFile->filePtr, openFile->buffer, _fs->blockSize);
        openFile->filePtr += _fs->blockSize;
        i++;
    }
    openFile->readPtr = readSize - readFileLen;

    if (openFile->readPtr) {
        blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, readSize);
        readBlock(openFile->buffer, blockNumber);
        memcpy(buffer+ openFile->filePtr, openFile, openFile->readPtr);
    }
}

// Updates the file length after writing
void updateFileLen(TOpenFile *openFile, unsigned int writeSize) {
    unsigned int updatedLen = getFileLength(openFile->filename);
    if (openFile->openMode == MODE_READ_APPEND) {
        updatedLen += writeSize;
    } else {
        updatedLen = writeSize;
    }
    updateDirectoryFileLength(openFile->filename, updatedLen);
}
