#ifndef SYSCALL_H
#define SYSCALL_H

#include "isr.h"

// Syscall numbers
#define SYS_EXIT       0
#define SYS_WRITE      1
#define SYS_READ       2
#define SYS_OPEN       3
#define SYS_READ_FILE  4
#define SYS_WRITE_FILE 5
#define SYS_EXEC       6

void syscall_init(void);

static inline long syscall(long num, long arg1, long arg2, long arg3) {
    long ret;
    asm volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

#endif
