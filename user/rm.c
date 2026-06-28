#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        puts("Usage: rm <file> [<file>...]");
        return 1;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (sys_unlink(argv[i]) < 0) {
            print("rm: cannot remove '");
            print(argv[i]);
            puts("'");
            rc = 1;
        }
    }
    return rc;
}
