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
    if (findFile(av[1]) == FS_FILE_NOT_FOUND) {
        printf("File: %s has been deleted!\n",av[1]);
    } else {
        printf("File: %s cannot be deleted. Attribute is ", av[1]);
        unsigned int attr = getattr(av[1]);
        if (attr == 0b01) {
            printf("W\n");
        }
        if (attr == 0b10) {
            printf("R\n");
        }
    }
    closeFile(file);
    closeFS();

	return 0;
}
