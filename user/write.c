#include "libc/stdio.h"

// Usage: write <file> <text...>
// Creates (or truncates) <file> and writes the joined arguments + newline.
int _start(char *argv[], int argn) {
    if (argn < 2) {
        puts("Usage: write <file> <text...>");
        return 1;
    }

    long fd = sys_open(argv[0], O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        puts("Failed to open file");
        return 1;
    }

    for (int i = 1; i < argn; i++) {
        sys_write(fd, argv[i], strlen(argv[i]));
        if (i + 1 < argn) sys_write(fd, " ", 1);
    }
    sys_write(fd, "\n", 1);

    sys_close(fd);
    return 0;
}
