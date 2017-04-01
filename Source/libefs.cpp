#include "libefs.h"
#define UNASSIGNED_BLOCK 0
#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

// FS Descriptor
TFileSystemStruct *_fs;

// Open File Table
TOpenFile *_oft;

// Open file table counter
int _oftCount=0;

// Mounts a paritition given in fsPartitionName. Must be called before all
// other functions
void initFS(const char *fsPartitionName, const char *fsPassword)
{
    mountFS(fsPartitionName, fsPassword);
    _fs = getFSInfo();
    _oft = new TOpenFile;
    _oft->inodeBuffer = makeInodeBuffer();
    _oft->buffer = makeDataBuffer();
    _oft->blockSize = _fs->blockSize;
    _oft->writePtr = 0;
    _oft->readPtr = 0;
    _oft->filePtr = 0;
}

// Opens a file in the partition. Depending on mode, a new file may be created
// if it doesn't exist, or we may get FS_FILE_NOT_FOUND in _result. See the enum above for valid modes.
// Return -1 if file open fails for some reason. E.g. file not found when mode is MODE_NORMAL, or
// disk is full when mode is MODE_CREATE, etc.

int openFile(const char *filename, unsigned char mode)
{
    _oft->openMode = mode;
    unsigned int inodeId = findFile(filename);
    if (inodeId != FS_FILE_NOT_FOUND) { // File exist we just return
        char *mode = getFileMode(_oft->openMode);
        FILE *fp = fopen(filename,mode);
        if (fp == NULL) {
            return -1;
        }
        _oft->inode = inodeId;
        _oft->writePtr = 0;
        _oft->readPtr = 0;
        _oft->filePtr = 0;       
        loadInode(_oft->inodeBuffer, inodeId); 
        return inodeId;
    } 
    if (mode == MODE_CREATE) {
        // Create a new file
        FILE *fp = fopen(filename, "ab+");
        if (fp == NULL) {
            return -1;
        }
        unsigned int dirIdx = makeDirectoryEntry(filename, 0x0, 0);
        saveInode(_oft->inodeBuffer, dirIdx);
        unsigned long freeBlock = findFreeBlock();
        markBlockBusy(freeBlock);
        _oft->inodeBuffer[0] = freeBlock;
        _oft->inode = dirIdx;
        _oft->writePtr = 0;
        _oft->readPtr = 0;
        _oft->filePtr = 0;
        updateFreeList();
        updateDirectory();
        return dirIdx;
    }
    return -1;
}

// Write data to the file. File must be opened in MODE_NORMAL or MODE_CREATE modes. Does nothing
// if file is opened in MODE_READ_ONLY mode.
void writeFile(int fp, void *buffer, unsigned int dataSize, unsigned int dataCount)
{
    if (_oft->openMode == MODE_READ_ONLY) {
        return;
    }
    _oft->inode = fp;
    char *mode = getFileMode(_oft->openMode);
    FILE *fd = fdopen(fp,mode); 
    loadInode(_oft->inodeBuffer, fp);
    if (_oft->openMode != MODE_READ_APPEND) {
        unsigned int totalData = dataSize * dataCount;
        unsigned numBlocksReq = (totalData / _fs->blockSize);
        if (totalData % _fs->blockSize > 0) {
            numBlocksReq += 1;
        }
        int i;
        for (i = 0; i < numBlocksReq; i++) {
            unsigned long len = fread((char*)_oft->buffer, sizeof(char), _fs->blockSize, fd);
            if (len < _fs->blockSize) {
                continue;
            }
            if (_oft->inodeBuffer[i] == UNASSIGNED_BLOCK) { // Inode without block numbers
                unsigned long freeBlock = findFreeBlock();
                markBlockBusy(freeBlock);
                _oft->inodeBuffer[i] = freeBlock;
                writeBlock(_oft->buffer,freeBlock);
            } else { // Inode with existing block numbers
                writeBlock(_oft->buffer, _oft->inodeBuffer[i]);    
            }
        }
    }
    updateFreeList();
    updateDirectory();
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
    _oft->inode = fp;
    loadInode(_oft->inodeBuffer, fp);
    char *mode = getFileMode(_oft->openMode);
    FILE *fd = fdopen(fp, mode);
    while (_oft->inodeBuffer[_oft->readPtr] != UNASSIGNED_BLOCK) {
        readBlock(_oft->buffer, _oft->inodeBuffer[_oft->readPtr]);
        fwrite(buffer, sizeof(char), _fs->blockSize, fd);
        _oft->readPtr += 1;
    }
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
    char *mode = getFileMode(_oft->openMode);
    FILE *fd = fdopen(fp, mode);
    fclose(fd);
    flushFile(fp);
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
