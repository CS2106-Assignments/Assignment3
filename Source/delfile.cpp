#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 2)
	{
		printf("\nUsage: %s <file to delete>\n\n", av[0]);
		return -1;
	}
    initFS("part.dsk", "");
    int file = openFile(av[1], 0);
    if(file == FS_FILE_NOT_FOUND) {
        printf("FILE NOT FOUND\n");
    }
    if(file == -1) {
        return -1;
    }
    delFile(av[1]);
    closeFile(file);
    closeFS();

	return 0;
}
