#include "libefs.h"

int main(int ac, char **av)
{
    if(ac != 3)
    {
        printf("\nUsage: %s <file to check in> <password>\n\n", av[0]);
        return -1;
    }

    FILE *fp = fopen(av[1], "r");

    if (fp == NULL) {
        printf("\nUnable to open source file %s\n\n", av[1]);
        exit(-1);
    }

    // Mount the file system
    mountFS("part.dsk", av[2]);
    int status = openFile(av[1], MODE_CREATE);

    if (status == FS_DUPLICATE_FILE) {
        printf("\nDuplicate files! %s\n\n", av[1]);
        exit(-1);
    }

    fseek(fp, 0L, SEEK_END);
    unsigned int sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    char *buffer;
    fread( buffer, sizeof(char), sz, fp);
    writeFile(status, buffer, sizeof(char), sz);
    fclose(fp);
    closeFile(status);
    closeFS();
    return 0;
}
