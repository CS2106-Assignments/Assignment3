#include "libefs.h"
#define UNASSIGNED_BLOCK 0
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define MAX_OFT_ENTRY 1024

// FS Descriptor
TFileSystemStruct *_fs;

// Open File Table
TOpenFile *_oft;
bool *_oftMap;

// Open file table counter
int _oftCount=0;

void updateOftEntry(int oftIdx, unsigned char openMode, unsigned int blockSize, unsigned long inode,unsigned long *inodeBuffer, char *buffer, unsigned int writePtr, unsigned int readPtr, unsigned filePtr, const char* filename);
void clearOftEntry(unsigned int oftIdx);
char *getFileMode(unsigned int openMode); 
// Grabs the next free oft entry possible
unsigned int getFreeOftEntry();

// Mounts a paritition given in fsPartitionName. Must be called before all
// other functions
void initFS(const char *fsPartitionName, const char *fsPassword)
{
    mountFS(fsPartitionName, fsPassword);
    _fs = getFSInfo();
    _oft = (TOpenFile *) malloc(sizeof(TOpenFile*) * MAX_OFT_ENTRY);
    _oftMap = (bool *) malloc(sizeof(bool *) * MAX_OFT_ENTRY);
}

// Opens a file in the partition. Depending on mode, a new file may be created
// if it doesn't exist, or we may get FS_FILE_NOT_FOUND in _result. See the enum above for valid modes.
// Return -1 if file open fails for some reason. E.g. file not found when mode is MODE_NORMAL, or
// disk is full when mode is MODE_CREATE, etc.
int openFile(const char *filename, unsigned char mode)
{
    _oftCount += 1;
    if (_oftCount >= MAX_OFT_ENTRY) {
        _oftCount = MAX_OFT_ENTRY-1;
        return -1;
    }
    unsigned int fileIdx = findFile(filename);
    unsigned int freeOftEntry = getFreeOftEntry();
    _oftMap[freeOftEntry] = 1;
    if (_result != FS_FILE_NOT_FOUND) { // If found
        // Update oft
        updateOftEntry(freeOftEntry, mode, _fs->blockSize, fileIdx, makeInodeBuffer(), makeDataBuffer(), 0, 0, 0, filename);
        return freeOftEntry;
    }
    if (mode == MODE_CREATE) { // Create a file if mode is MODE_CREATE
        fileIdx = makeDirectoryEntry(filename, 0x0, 0);
        updateOftEntry(freeOftEntry, mode, _fs->blockSize, fileIdx, makeInodeBuffer(), makeDataBuffer(), 0, 0, 0, filename);
        unsigned long freeBlock = findFreeBlock();
        if (_result == FS_FULL) { // ERROR when disk is full
            _oftMap[freeOftEntry] = 0;
            clearOftEntry(freeOftEntry);
            _oftCount -= 1;
            return FS_FULL;
        }
        markBlockBusy(freeBlock);
        loadInode(_oft[freeOftEntry].inodeBuffer, fileIdx);
        _oft[freeOftEntry].inodeBuffer[0] = freeBlock;
        writeBlock("", freeBlock);
        saveInode(_oft[freeOftEntry].inodeBuffer, fileIdx);

        updateFreeList();
        updateDirectory();
        return freeOftEntry;
    }
    _oftCount -= 1;
    return FS_FILE_NOT_FOUND;
}

// Updates entire oft entry. 
void updateOft(int oftIdx, unsigned char openMode, unsigned int blockSize, unsigned long inode,unsigned long *inodeBuffer, char *buffer, unsigned int writePtr, unsigned int readPtr, unsigned filePtr, const char * filename) {
    // Update oft
    _oft[oftIdx].openMode = openMode;
    _oft[oftIdx].blockSize = blockSize;
    _oft[oftIdx].inode = inode;
    _oft[oftIdx].inodeBuffer = inodeBuffer;
    _oft[oftIdx].buffer = buffer;
    _oft[oftIdx].writePtr = writePtr;
    _oft[oftIdx].readPtr = readPtr;
    _oft[oftIdx].filePtr = filePtr;
    _oft[oftIdx].filename = filename;
}

// Clears the oft entry
unsigned int clearOftEntry(int oftIdx) {
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

// Write data to the file. File must be opened in MODE_NORMAL or MODE_CREATE modes. Does nothing
// if file is opened in MODE_READ_ONLY mode.
void writeFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    if (_oft[fp].openMode == MODE_READ_ONLY) {
        return;
    }
    int totalSize = dataSize * dataCount;
    TOpenFile openFile = _oft[fp];
    loadInode(openFile.inodeBuffer, openFile.inode);

    unsigned long freeBlock = -1;
    unsigned int bytesLeftInBuffer = _fs->blockSize - openFile.writePtr;
    unsigned int writeSize = 0;
    unsigned int currentInodeToLookAt = 0;
    unsigned int updatedLen = getFileLength(openFile.filename);
    // If the buffer is still not full
    if (totalSize < bytesLeftInBuffer) {
        memcpy(openFile.buffer + openFile.writePtr, buffer, totalSize);
        openFile.writePtr += totalSize;
    } else { // Once buffer is full
        // Fill up rest of the buffer
        memcpy(openFile.buffer + openFile.writePtr, buffer, bytesLeftInBuffer);
        if (openFile.openMode == MODE_READ_APPEND) {
            freeBlock = findFreeBlock();
            markBlockBusy(freeBlock);
            writeBlock(openFile.buffer, freeBlock);
        } else {
            freeBlock = openFile.inodeBuffer[currentInodeToLookAt];
            if (freeBlock == UNASSIGNED_BLOCK) {
                freeBlock = findFreeBlock();
                markBlockBusy(freeBlock);
            }
            writeBlock(openFile.buffer, freeBlock);
            currentInodeToLookAt += 1;
        }
        openFile.writePtr = 0;
        unsigned int currentFileLenLeft = totalSize - bytesLeftInBuffer;
        // Continue to read blocks of file continuously
        unsigned int numWriteItr = currentFileLenLeft / _fs->blockSize;
        unsigned int i = 0;
        unsigned int filePos = bytesLeftInBuffer;
        while (i < numWriteItr) {
            memcpy(openFile.buffer, buffer + filePos, _fs->blockSize);
            if (openFile.openMode == MODE_READ_APPEND) {
                freeBlock = findFreeBlock();
                markBlockBusy(freeBlock);
            } else {
                freeBlock = openFile.inodeBuffer[currentInodeToLookAt];
                if (freeBlock == UNASSIGNED_BLOCK) {
                    freeBlock = findFreeBlock();
                    markBlockBusy(freeBlock);
                }
                writeBlock(openFile.buffer, freeBlock);
                currentInodeToLookAt += 1;
            }
            writeBlock(openFile.buffer, freeBlock);
            filePos += _fs->blockSize;
            i += 1;
        }
        openFile.writePtr = currentFileLenLeft - (i * _fs->blockSize);
    
        memcpy(openFile.buffer, buffer + filePos, openFile.writePtr);
        if (openFile.openMode == MODE_READ_APPEND) {
            updatedLen += filePos;
        } else {
            updatedLen = filePos;
        }
    }
    saveInode(openFile.inodeBuffer, fp);

    updateDirectoryFileLength(openFile.filename, updatedLen);
    updateFreeList();
    updateDirectory();
}

int calcNumBlocksRequired(unsigned int blockSize, unsigned dataSize, unsigned int dataCount) {
    int totalSize = dataSize * dataCount;
    return totalSize/blockSize;
}

// Flush the file data to the disk. Writes all data buffers, updates directory,
// free list and inode for this file.
void flushFile(int fp)
{
    TOpenFile *openFile = &_oft[fp];
    unsigned long freeBlock = findFreeBlock();
    markBlockBusy(freeBlock);
    writeBlock(openFile->buffer, freeBlock);
    loadInode(openFile->inodeBuffer, fp);
    openFile->inodePtr +=1;
    unsigned int curFileLen = getFileLength(openFile->filename) + openFile->writePtr;
    // Find the inode to put in
    setBlockNumInInode(openFile->inodeBuffer, curFileLen, freeBlock);
    saveInode(openFile->inodeBuffer, fp);
    // Update file length
    updateDirectoryFileLength(_oft[fp].filename, curFileLen);
    _oft[fp].writePtr = 0;
    _oft[fp].readPtr = 0;

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
    char *readBuffer = (char *) malloc(_fs->blockSize);
    unsigned int readFileLen = 0;
    unsigned int filePos = 0;
    if (openFile->readPtr < totalReadSize) {
        blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, totalReadSize);
        readBlock(readBuffer, blockNumber);
        memcpy(openFile->buffer + openFile->readPtr, readBuffer, totalReadSize);
    } else {
        unsigned int bytesLeftInBuffer = _fs->blockSize - openFile->readPtr;
        blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, bytesLeftInBuffer);
        readBlock(readBuffer, blockNumber);
        memcpy(openFile->buffer + openFile->readPtr, readBuffer, bytesLeftInBuffer);
        memcpy(buffer, openFile->buffer, _fs->blockSize);

        // Number of read iterations
        unsigned int curFileLen = totalReadSize - bytesLeftInBuffer;
        unsigned int readItr = curFileLen / _fs->blockSize;
        unsigned int i=0;
        readFileLen = bytesLeftInBuffer;
        filePos = bytesLeftInBuffer;
        while (i < readItr) {
            readFileLen += _fs->blockSize;
            blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, readFileLen);
            readBlock(readBuffer, blockNumber);
            memcpy(buffer + filePos, readBuffer, _fs->blockSize);
        }
        openFile->readPtr = totalReadSize - readFileLen;

        if (openFile->readPtr) {
            blockNumber = returnBlockNumFromInode(openFile->inodeBuffer, totalReadSize);
            readBlock(readBuffer, blockNumber);
            memcpy(openFile->buffer, readBuffer, openFile->readPtr);
        }
    }

    free(readBuffer);
    updateFreeList();
    updateDirectory();
}

// Delete the file. Read-only flag (bit 2 of the attr field) in directory listing must not be set. 
// See TDirectory structure.
void delFile(const char *filename) {
    // Check if the file has a bit 2 in attr field
    unsigned int attr = getattr(filename);

    // Read-only flag is true (bit 2 of attr field is set to 1)
    if (CHECK_BIT(attr, 2)) {
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
    free(openFile.buffer);
    free(openFile.inodeBuffer);
    // Update file discriptors
    // Free oft entry
    _oftMap[fp] = 0;
}

// Unmount file system.
void closeFS() {
    unmountFS();
    // Go into every oft entry and free (close) everything if possible
    free(_oftMap);
    free(_oft);
}

char *getFileMode(unsigned int openMode) {
    char *mode;
    if (openMode == MODE_READ_ONLY) {
        mode = "r";
    } else if (openMode == MODE_READ_APPEND) {
        mode = "ab+";
    } else {
        mode = "w";
    }
    return mode;
}

unsigned int getFreeOftEntry() {
    int i;
    for (i=0; i < MAX_OFT_ENTRY; i++) {
        if (_oftMap[i] == 0) {
            break;
        }
    }
    return i;
}
