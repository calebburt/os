#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    for (int i = 0; i < argn; i++) {
        print(argv[i]);
        if (i + 1 < argn) putchar(' ');
    }
    putchar('\n');
    return 0;
}
