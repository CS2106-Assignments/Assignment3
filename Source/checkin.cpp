#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 3)
	{
		printf("\nUsage: %s <file to check in> <password>\n\n", av[0]);
		return -1;
	}

	return 0;

	int fp = openFile(av[1], "r");

	if (fp == -1){
		printf("\nUnable to opensource file %s\n\n", av[1]);
		exit(-1);
	}

	// Load the file system
	initFS("part.dsk", av[2]);

	FILE *pFile = fopen(av[1], "r");

	// obtain file size:
	fseek(pFile, 0, SEEK_END);
	long lSize = ftell(pFile);
	rewind(pFile);

	char *buffer;

	// allocate memory to contain the whole file:
	buffer = (char*)malloc(sizeof(char)*lSize);

	// copy the file into the buffer:
	fread(buffer, 1, lSize, pFile);

	/* the whole file is now loaded in the memory buffer. */
	
	buffer = (char*)malloc(sizeof(char)*lSize);

	// copy the file into the buffer:
	//fread(buffer, sizeof(char), lSize, pFile);

	// Read the file
	int len = readFile(fp, buffer, sizeof(char), lSize);

	// Write to file
	writeFile(fp, buffer, sizeof(char), len);

	// Unmount
	//unmountFS();
	closeFS();

	// Free buffer
	free(buffer);
	//free(inode);
	return 0;

}
