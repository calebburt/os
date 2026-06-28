// C runtime startup: adapts the kernel entry ABI (argv, argn) to
// the standard C main(argc, argv) convention.
#include "stdlib.h"

extern int main(int argc, char *argv[]);

int _start(char *argv[], int argn) {
    // Raw SYS_WRITE(1, msg, len) — no library dependency, confirms _start reached.
    const char msg[] = "[crt0] _start ok\n";
    long dummy;
    asm volatile("int $0x80"
        : "=a"(dummy)
        : "a"(1L), "D"(1L), "S"(msg), "d"((long)(sizeof msg - 1))
        : "memory");
    int ret = main(argn, argv);
    return ret;
}
