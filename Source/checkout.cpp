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

	if (fileNdx == -1)
	{
		printf("Cannot find encrypted file %s\n", av[1]);
		exit(-1);
	}

	FILE *file = fopen(av[1], "r");

	// Get file length
	fseek(file, 0, SEEK_END);
	int len = ftell(file);
	rewind(file);

	// Allocate the inode and buffer
	char *buffer = (char*)malloc(sizeof(char)*len);

	// Unmount the file system
	closeFS();

	// Write the data
	readFile(fp, buffer, sizeof(char), len);

	// Close the file
	fclose(file);
	closeFile(fp);

	free(buffer);
    return 0;
}
