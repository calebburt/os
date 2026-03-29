#ifndef SYSCALL_H
#define SYSCALL_H

#include "isr.h"

// Syscall numbers
#define SYS_WRITE  0
#define SYS_READ   1
#define SYS_EXIT   2

void syscall_init(void);

#endif
