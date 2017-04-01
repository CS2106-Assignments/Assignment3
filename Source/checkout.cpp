#include "libefs.h"

int main(int ac, char **av)
{
	if(ac != 3)
	{
		printf("\nUsage: %s <file to check out> <password>\n\n", av[0]);
		return -1;
	}
    initFS("part.dsk", av[2]);
    int status = openFile(av[1], MODE_READ_ONLY);
    
    if (status == FS_FILE_NOT_FOUND) {
   		printf("\nFile %s not found!\n\n", av[1]);
		return -1;
    }
    char *buffer;
    
    unsigned int len = getFileLength(av[1]);
    readFile(status, buffer, sizeof(char), len);
    closeFile(status);
    FILE *fp = fopen(av[1], "w");
    fwrite(buffer, sizeof(char), len, fp);
    fclose(fp);
    closeFS();
	return 0;
}
