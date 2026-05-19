#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

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

// open() flags (a subset of POSIX)
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREAT  0100
#define O_TRUNC  01000
#define O_APPEND 02000

// lseek whence
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// Mode bits (match src/vfs.h)
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
    uint64_t size;
    uint32_t mode;
};

struct dirent {
    char     name[64];
    uint32_t mode;
    uint64_t size;
};

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

static long sys_close(long fd) {
    return syscall(SYS_CLOSE, fd, 0, 0);
}

static long sys_unlink(const char *path) {
    return syscall(SYS_UNLINK, (long)path, 0, 0);
}

static long sys_lseek(long fd, long offset, int whence) {
    return syscall(SYS_LSEEK, fd, offset, whence);
}

static long sys_stat(const char *path, struct stat *out) {
    return syscall(SYS_STAT, (long)path, (long)out, 0);
}

static long sys_readdir(long fd, int index, struct dirent *out) {
    return syscall(SYS_READDIR, fd, index, (long)out);
}

#endif
