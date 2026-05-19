#include "libc/stdio.h"

// NOTE: SYS_READ currently always returns the requested length even past
// end-of-file, so we read a single fixed-size buffer. Files larger than the
// buffer will be truncated, and counts may include trailing garbage.
int _start(char *argv[], int argn) {
    if (argn < 1) {
        puts("Usage: wc <file>");
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

    int lines = 0, words = 0, bytes = 0, in_word = 0;
    for (long i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '\0') break;
        bytes++;
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }

    char out[64];
    snprintf(out, sizeof(out), "%d %d %d %s\n", lines, words, bytes, argv[0]);
    print(out);
    return 0;
}
