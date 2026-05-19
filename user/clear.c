#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    (void)argv;
    (void)argn;
    print("\x1b[2J\x1b[H");
    return 0;
}
