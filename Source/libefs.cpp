#include "libefs.h"
#define UNASSIGNED_BLOCK 0
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
#define ROOT_DIR "/"

// FS Descriptor
TFileSystemStruct *_fs;

// Open File Table
TOpenFile *_oft;

// Open file table counter
int _oftCount=0;

void updateOft(int oftIdx, unsigned char openMode, unsigned int blockSize, unsigned long inode,unsigned long *inodeBuffer, char *buffer, unsigned int writePtr, unsigned int readPtr, unsigned filePtr, const char* filename);
char *getFileMode(unsigned int openMode); 

// Mounts a paritition given in fsPartitionName. Must be called before all
// other functions
void initFS(const char *fsPartitionName, const char *fsPassword)
{
    mountFS(fsPartitionName, fsPassword);
    _fs = getFSInfo();
    _oft = (TOpenFile *) malloc(sizeof(TOpenFile*) * _fs-> maxFiles);
}

// Opens a file in the partition. Depending on mode, a new file may be created
// if it doesn't exist, or we may get FS_FILE_NOT_FOUND in _result. See the enum above for valid modes.
// Return -1 if file open fails for some reason. E.g. file not found when mode is MODE_NORMAL, or
// disk is full when mode is MODE_CREATE, etc.
int openFile(const char *filename, unsigned char mode)
{
    _oftCount += 1;
    int fileIdx = findFile(filename);
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
    while (writeSize >= _fs->blockSize) { // Buffer overflow, flush to file to disk
        unsigned long freeBlock = findFreeBlock();
        markBlockBusy(freeBlock);
        unsigned int bytesLeft = _fs->blockSize - _oft[fp].writePtr;
        memcpy(_oft[fp].buffer + _oft[fp].writePtr, buffer, bytesLeft);
        writeBlock(_oft[fp].buffer, freeBlock);
        loadInode(_oft[fp].inodeBuffer,_oft[fp].inode);
        _oft[fp].inodePtr += 1;
        _oft[fp].inodeBuffer[_oft[fp].inodePtr] = freeBlock;
        saveInode(_oft[fp].inodeBuffer, fp);
        writeSize = writeSize - _fs->blockSize;
        // Update file length
        int curFileLen = getFileLength(_oft[fp].filename);
        updateDirectoryFileLength(_oft[fp].filename, curFileLen + _fs->blockSize);
    }
    _oft[fp].writePtr = writeSize;
}

int calcNumBlocksRequired(unsigned int blockSize, unsigned dataSize, unsigned int dataCount) {
    int totalSize = dataSize * dataCount;
    return totalSize/blockSize;
}

// Flush the file data to the disk. Writes all data buffers, updates directory,
// free list and inode for this file.
void flushFile(int fp)
{
    loadInode(_oft->inodeBuffer, fp);
    unsigned long freeBlock = findFreeBlock();
    markBlockBusy(freeBlock);
    writeBlock(_oft->buffer, freeBlock);

    updateFreeList();
    updateDirectory();
}

// Read data from the file.
void readFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{

}

// Delete the file. Read-only flag (bit 2 of the attr field) in directory listing must not be set. 
// See TDirectory structure.
void delFile(const char *filename) {
    unsigned int inodeId = findFile(filename);
    if (inodeId == FS_FILE_NOT_FOUND) {
        return;
    }

    _oft->inode = inodeId;
    loadInode(_oft->inodeBuffer, inodeId);
    unsigned int attr = getattr(filename);
    if (CHECK_BIT(attr,2)) {  
        return;
    }
    int i = 0;
    while (_oft->inodeBuffer[i] != UNASSIGNED_BLOCK) {
        markBlockFree(_oft->inodeBuffer[i]);
    }
    delDirectoryEntry(filename);
    updateFreeList();
    updateDirectory();
}

// Close a file. Flushes all data buffers, updates inode, directory, etc.
void closeFile(int fp) {
}

// Unmount file system.
void closeFS() {
    unmountFS();
    free(_oft->buffer);
    free(_oft->inodeBuffer);
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
