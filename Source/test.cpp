#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 3)
	{
		printf("\nUsage: %s <file to check out> <password>\n\n", av[0]);
		return -1;
	}
	// Mount the file system
	initFS("part.dsk", av[2]);

	// Search the directory for the file
	//unsigned int fileNdx = findFile(av[1]);
	int fileNdx = openFile(av[1], MODE_READ_ONLY);
    int file = openFile(av[1], MODE_CREATE);
	if (fileNdx == FS_FILE_NOT_FOUND)
	{
		printf("Cannot find encrypted file %s\n", av[1]);
		exit(-1);
	}
	// Get file length
    int len = getFileLength(av[1]) + 1;
    // Allocate the inode and buffer
	char *buffer = (char*)malloc(sizeof(char)*len);

	// Write the data
	readFile(fileNdx, buffer, sizeof(char), len - 1);
    buffer[len-1] = '\0';
    
	// Close the file

    FILE *fp = fopen(av[1], "w");

    if (fp == NULL) {
        printf("File cannot be written out\n");
        return -1;
    }
    fwrite(buffer,sizeof(char), len, fp);
    printf("Checkout successfull!\nFile: %s created!\n", av[1]);
	free(buffer);
	// Unmount the file system
	closeFS();
	fclose(fp);
    
    return 0;
}
