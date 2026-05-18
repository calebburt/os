#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    if (argn < 1) {
        puts("Usage: cat <file>");
        return 1;
    }

    long handle = sys_open(argv[0], 0);
    if (handle < 0) {
        puts("Failed to open file");
        return 1;
    }

    char *content = sys_read_file(handle);
    if (!content) {
        puts("Failed to read file");
        return 1;
    }

    puts(content);

    return 0;
}