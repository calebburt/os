#ifndef SYSCALL_HELPERS_H
#define SYSCALL_HELPERS_H

#include "syscall.h"

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