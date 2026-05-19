#include "libc/stdio.h"

// Print the first 10 lines of a file. Reads one buffer's worth — see
// note in wc.c about SYS_READ not signalling EOF.
int _start(char *argv[], int argn) {
    if (argn < 1) {
        puts("Usage: head <file>");
        return 1;
    }

    long handle = sys_open(argv[0], 0);
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
    sys_write(stdout, buf, i);
    return 0;
}
