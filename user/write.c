#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        puts("Usage: write <file> <text...>");
        return 1;
    }

    long fd = sys_open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        puts("Failed to open file");
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        sys_write(fd, argv[i], strlen(argv[i]));
        if (i + 1 < argc) sys_write(fd, " ", 1);
    }
    sys_write(fd, "\n", 1);

    sys_close(fd);
    return 0;
}
