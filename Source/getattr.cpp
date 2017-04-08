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
    
    int file = openFile(av[1], 0);

    if(file == -1) {
        return -1;
    }
    else if(file == FS_FILE_NOT_FOUND) {
        printf("FILE NOT FOUND");
    } else {
        unsigned int attr = getattr(av[1]);
        if(attr == 0) {
            printf("W");
        }
        if(attr == 2) {
            printf("R");
        }
    }
    closeFile(file);
	return 0;
}
