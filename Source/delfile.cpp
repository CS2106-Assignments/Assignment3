#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 2)
	{
		printf("\nUsage: %s <file to delete>\n\n", av[0]);
		return -1;
	}
    int file = openFile(av[1], 0);
    if(file == FS_FILE_NOT_FOUND) {
        printf("FILE NOT FOUND");
    }
    if(file == -1) {
        return -1;
    }
    delFile(av[1]);

	return 0;
}
