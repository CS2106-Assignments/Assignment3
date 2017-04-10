#include "libefs.h"
#include <cctype>
int main(int ac, char **av)
{
	if(ac != 2)
	{
		printf("\nUsage: %s <file to check>\n", av[0]);
		printf("Prints: 'R' = Read only, 'W' = Read/Write\n\n");
		return -1;
	}
    initFS("part.dsk", "");
    unsigned int file = openFile(av[1], MODE_NORMAL);
    if(file == -1) {
        return -1;
    }
    else if(file == FS_FILE_NOT_FOUND) {
        printf("FILE NOT FOUND\n");
    } else {
        unsigned int attr = getattr(av[1]);
        printf("file: %d\n",file);
        printf("attr: %d\n",attr);
        if(attr == 0b01) {
            printf("W\n");
        }
        if(attr == 0b10) {
            printf("R\n");
        }
        closeFile(file);
    }
    closeFS();
	return 0;
}
