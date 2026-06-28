#include "libc/stdio.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    print("\x1b[2J\x1b[H");
    return 0;
}
