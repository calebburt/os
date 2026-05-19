#include "libc/stdio.h"

int _start(char *argv[], int argn) {
    if (argn < 1) {
        puts("Usage: rm <file> [<file>...]");
        return 1;
    }

    int rc = 0;
    for (int i = 0; i < argn; i++) {
        if (sys_unlink(argv[i]) < 0) {
            print("rm: cannot remove '");
            print(argv[i]);
            puts("'");
            rc = 1;
        }
    }
    return rc;
}
