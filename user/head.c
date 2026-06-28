#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: head <file>");
        return 1;
    }

    long handle = sys_open(argv[1], 0);
    if (handle < 0) {
        puts("Failed to open file");
        return 1;
    }

    char buf[4096];
    long n = sys_read(handle, buf, sizeof(buf));
    if (n < 0) {
        puts("Failed to read file");
        return 1;
    }

    int lines = 0;
    long i;
    for (i = 0; i < n && lines < 10; i++) {
        if (buf[i] == '\0') break;
        if (buf[i] == '\n') lines++;
    }
    sys_write(1, buf, i);
    return 0;
}
