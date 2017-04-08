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

void updateOft(int oftIdx, unsigned char openMode, unsigned int blockSize, unsigned long inode,unsigned long *inodeBuffer, char *buffer, unsigned int writePtr, unsigned int readPtr, unsigned filePtr, const char* filename);
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
    if (_result != FS_FILE_NOT_FOUND) { // If found
        // Update oft
        updateOft(_oftCount, mode, _fs->blockSize, fileIdx, makeInodeBuffer(), makeDataBuffer(), 0, 0, 0, filename);
        return _oftCount;
    }
    if (mode == MODE_CREATE) { // Create a file if mode is MODE_CREATE
        fileIdx = makeDirectoryEntry(filename, 0x0, 0);
        updateOft(_oftCount, mode, _fs->blockSize, fileIdx, makeInodeBuffer(), makeDataBuffer(), 0, 0, 0, filename);
        unsigned long freeBlock = findFreeBlock();
        if (_result == FS_FULL) { // ERROR when disk is full
            return -1;
        }
        markBlockBusy(freeBlock);
        loadInode(_oft[_oftCount].inodeBuffer, fileIdx);
        _oft[_oftCount].inodeBuffer[0] = freeBlock;
        writeBlock("",freeBlock);
        saveInode(_oft[_oftCount].inodeBuffer, fileIdx);

        updateFreeList();
        updateDirectory();
        return _oftCount;
    }
    return -1;
}

void updateOft(int oftIdx, unsigned char openMode, unsigned int blockSize, unsigned long inode,unsigned long *inodeBuffer, char *buffer, unsigned int writePtr, unsigned int readPtr, unsigned filePtr, const char * filename) {
    // Update oft
    _oft[_oftCount].openMode = openMode;
    _oft[_oftCount].blockSize = blockSize;
    _oft[_oftCount].inode = inode;
    _oft[_oftCount].inodeBuffer = inodeBuffer;
    _oft[_oftCount].buffer = buffer;
    _oft[_oftCount].writePtr = writePtr;
    _oft[_oftCount].readPtr = readPtr;
    _oft[_oftCount].filePtr = filePtr;
    _oft[_oftCount].filename = filename;
}

// Write data to the file. File must be opened in MODE_NORMAL or MODE_CREATE modes. Does nothing
// if file is opened in MODE_READ_ONLY mode.
void writeFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    if (_oft[fp].openMode == MODE_READ_ONLY) {
        return;
    }
    int totalSize = dataSize * dataCount;
    unsigned int writeSize = _oft[fp].writePtr + totalSize;
    // This handles appending
    while (writeSize >= _fs->blockSize) { // Buffer overflow, flush to file to disk
        unsigned long freeBlock = findFreeBlock();
        markBlockBusy(freeBlock);
        unsigned int bytesLeft = _fs->blockSize - _oft[fp].writePtr;
        memcpy(_oft[fp].buffer + _oft[fp].writePtr, buffer, bytesLeft);
        writeBlock(_oft[fp].buffer, freeBlock);
        loadInode(_oft[fp].inodeBuffer,_oft[fp].inode);
        _oft[fp].inodeBuffer[_oft[fp].inodePtr] = freeBlock;
        saveInode(_oft[fp].inodeBuffer, fp);
        writeSize = writeSize - _fs->blockSize;
        // Update file length
        int curFileLen = getFileLength(_oft[fp].filename);
        updateDirectoryFileLength(_oft[fp].filename, curFileLen + _fs->blockSize);
        _oft[fp].inodePtr += 1;
    }
    // TODO need to handle MODE_READ_APPEND
    _oft[fp].writePtr = writeSize;
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
    unsigned long freeBlock = findFreeBlock();
    markBlockBusy(freeBlock);
    writeBlock(_oft[fp].buffer, freeBlock);
    loadInode(_oft[fp].inodeBuffer, fp);
    _oft[fp].inodePtr +=1;
    _oft[fp].inodeBuffer[_oft[fp].inodePtr] = freeBlock;
    saveInode(_oft[fp].inodeBuffer, fp);
    // Update file length
    int curFileLen = getFileLength(_oft[fp].filename);
    updateDirectoryFileLength(_oft[fp].filename, curFileLen + _oft[fp].writePtr);
    _oft[fp].writePtr = 0;
    _oft[fp].readPtr = 0;

    updateFreeList();
    updateDirectory();
}

// Read data from the file.
void readFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    TOpenFile openFile = _oft[fp];
    // Find the total reading size needed
    unsigned int totalReadSize = dataSize * dataCount;
    // Get the current size of the file
    unsigned int fileLen = getFileLength(openFile.filename);
    // The current read ptr of the file
    unsigned int bytesLeftToRead = openFile.readPtr; 
    unsigned int bytesRead = 0;
    unsigned long blockNum;
    unsigned int readItr = 0;
    // Load inode
    // TODO handle for when there is already a read poiter
    loadInode(openFile.inodeBuffer, openFile.inode);
    while (bytesRead < totalReadSize) {
        blockNum = openFile.inodeBuffer[readItr];
        readBlock(openFile.buffer, blockNum);
        memcpy(buffer + bytesRead, openFile.buffer, _fs->blockSize);
        bytesRead += _fs->blockSize;
        readItr += 1;
    }

    openFile.readPtr = totalReadSize - bytesRead; 

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
    unsigned int fileLen = getFileLength(filname);
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
    delDirectorEntry(filename);

    updateFreeList();
    updateDirectory();
}

// Close a file. Flushes all data buffers, updates inode, directory, etc.
void closeFile(int fp) {
    // Flush modified buffers to disk
    TOpenFile openFile = _oft[fp];
    flushFile(fp);
    // Release buffers
    free(openFile.buffer);
    free(openFile.inodeBuffer);
    // Update file discriptors
    // Free oft entry
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
            break;;
        }
    }
    return i;
}
