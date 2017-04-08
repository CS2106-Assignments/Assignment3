#include "libefs.h"
#include <cctype>
int main(int ac, char **av)
{
	if(ac != 3)
	{
		printf("\nUsage: %s <file to check set attrbute> <attribute>\n", av[0]);
		printf("Attribute: 'R' = Read only, 'W' = Read/Write\n\n");
		return -1;
	}
    int file = openFile(av[1], '0');
    if(file == FS_FILE_NOT_FOUND) {
        printf("FILE NOT FOUND\n");
        return -1;
    }
    if(file == -1) {
        return -1;
    }
    if(toupper(*av[2]) == 'R') {
        setattr(av[1], '2');
    } else if (toupper(*av[2]) == 'W') {
        setattr(av[1], '1');
    } else {
        printf("Invalid attribute");
    }
    closeFile(file);
	return 0;
}
