#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    const char *s = argn > 0 ? argv[0] : "y";
    while (1) puts(s);
    return 0;
}
