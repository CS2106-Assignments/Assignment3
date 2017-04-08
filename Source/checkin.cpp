#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 3)
	{
		printf("\nUsage: %s <file to check in> <password>\n\n", av[0]);
		return -1;
	}

	return 0;
/*
	FILE *fp = fopen(av[1], "r");

        if(fp == NULL)
        {
                printf("\nUnable to open source file %s\n\n", av[1]);
                exit(-1);
        }
*/

	FILE *fp = openFile(av[1], "r");

	if (fp == null){
		printf("\nUnable to opensource file %s\n\n", av[1]);
		exit(-1);
	}

        // Load the file system
        //mountFS("part.dsk", av[2]);
	initFS("part.dsk", av[2]);

        // Get the FS metadata
        //TFileSystemStruct *fs = getFSInfo();
	TFileSystemStruct *fs = getFSInfo();

        //char *buffer;
	char *buffer;

        // Allocate the buffer for reading
        //buffer = makeDataBuffer();
	buffer = makeDataBuffer();

        // Read the file
        //unsigned long len = fread(buffer, sizeof(char), fs->blockSize, fp);
	int len = readFile(buffer, sizeof(char), fs->blockSize, fp);

        // Write the directory entry
        //unsigned int dirNdx = makeDirectoryEntry(av[1], 0x0, len);
	unsigned int dirNdx = makeDirectoryEntry(av[1], 0x0, len);

        // Find a free block
        //unsigned long freeBlock = findFreeBlock();
	unsigned long freeBlock = findFreeBlock();

        // Mark the free block now as busy
        //markBlockBusy(freeBlock);
	markBlockBusy(freeBlock);

        // Create the inode buffer
        //unsigned long *inode = makeInodeBuffer();
	unsigned long *inode = makeInodeBuffer();

        // Load the
        //loadInode(inode, dirNdx);
	loadInode(inode, dirNdx);

        // Set the first entry of the inode to the free block
        //inode[0]=freeBlock;
	inode[0]=freeBlock;

        // Write the data to the block
        //writeBlock(buffer, freeBlock);
	writeBlock(buffer, freeBlock);

        // Write the inode
	//saveInode(inode, dirNdx);
	saveInode(inode, dirNdx);

        // Write the free list
        //updateFreeList();
	updateFreeList();

        // Write the diretory
        //updateDirectory();
	updateDirectory();

        // Unmount
        //unmountFS();
	unmountFS();

        // Free data and inode buffer
        free(buffer);
        free(inode);
        return 0;

}
