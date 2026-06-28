#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    const char *s = argc > 1 ? argv[1] : "y";
    while (1) puts(s);
    return 0;
}
