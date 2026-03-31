#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers
#define SYS_WRITE  0
#define SYS_READ   1
#define SYS_EXIT   2

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

// Helper functions for syscalls
static long sys_write(const char *buf, long len) {
    return syscall(SYS_WRITE, (long)buf, len, 0);
}

static long sys_read(char *buf, long len) {
    return syscall(SYS_READ, (long)buf, len, 0);
}

static void sys_exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
}

#endif