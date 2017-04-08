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
	int fileNdx = openFile(av[1], "r");

	if (fileNdx == -1)
	{
		printf("Cannot find encrypted file %s\n", av[1]);
		exit(-1);
	}

	// Get file length

	// obtain file size:
	fseek(pFile, 0, SEEK_END);
	int len = ftell(pFile);
	rewind(pFile);

	// Allocate the inode and buffer
	char *buffer = (char*)malloc(sizeof(char)*lSize);

	// Unmount the file system
	closeFS();

	// Write the data
	writeFile (fp, buffer, sizeof(char), len);

	// Close the file
	fclose(fp);

	free(buffer);
	return 0;
}
