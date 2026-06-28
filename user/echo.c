#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        print(argv[i]);
        if (i + 1 < argc) putchar(' ');
    }
    putchar('\n');
    return 0;
}
