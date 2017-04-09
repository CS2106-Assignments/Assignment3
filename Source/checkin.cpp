#include "libefs.h"

int main(int ac, char **av)
{
    if(ac != 3)
    {
        printf("\nUsage: %s <file to check in> <password>\n\n", av[0]);
        return -1;
    }
	// Load the file system
	initFS("part.dsk", av[2]);

	int fp = openFile(av[1], MODE_READ_ONLY);

	if (fp != FS_FILE_NOT_FOUND) {
		printf("\nDUPLICATE FILE %s\n\n", av[1]);
		exit(FS_DUPLICATE_FILE);
	}

	fp = openFile(av[1], MODE_CREATE);

    FILE *file = fopen(av[1], "r");

	// obtain file size:
	fseek(file, 0, SEEK_END);
	long lSize = ftell(file);
	rewind(file);

	// allocate memory to contain the whole file:
	char *buffer = (char*)malloc(sizeof(char)*lSize);

	// Read the file
	fread(buffer, sizeof(char), lSize, file);

    printf("%s\n", buffer);

	// Write to file
	writeFile(fp, buffer, sizeof(char), lSize);
    closeFile(fp);
	// Unmount
	closeFS();

	// Free buffer
	free(buffer);

	// Close file
	fclose(file);
    return 0;
}
