#ifndef SYSCALL_H
#define SYSCALL_H

// Syscall numbers (must match src/syscall.h)
#define SYS_EXIT       0
#define SYS_WRITE      1
#define SYS_READ       2
#define SYS_OPEN       3
#define SYS_CLOSE      4
#define SYS_UNLINK     5
#define SYS_LSEEK      6
#define SYS_EXEC       7
#define SYS_STAT       8
#define SYS_READDIR    9

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
static long sys_write(long fd, const char *buf, long len) {
    return syscall(SYS_WRITE, fd, (long)buf, len);
}

static long sys_read(long fd, char *buf, long len) {
    return syscall(SYS_READ, fd, (long)buf, len);
}

static void sys_exit(void) {
    syscall(SYS_EXIT, 0, 0, 0);
}

static long sys_open(char *path, int flags) {
    return syscall(SYS_OPEN, (long)path, (long)flags, 0);
}

static int sys_exec(char *path, char *argv[], int argn) {
    return syscall(SYS_EXEC, (long)path, (long)argv, (long)argn);
}

#endif
