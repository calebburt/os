#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: cat <file>");
        return 1;
    }

    long handle = sys_open(argv[1], 0);
    if (handle < 0) {
        puts("Failed to open file");
        return 1;
    }

    char buf[1024];
    long n = sys_read(handle, buf, sizeof(buf) - 1);
    if (n < 0) {
        puts("Failed to read file");
        return 1;
    }
    buf[n] = '\0';

    puts(buf);

    return 0;
}
