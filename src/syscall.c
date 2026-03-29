#include "syscall.h"
#include "stdio.h"
#include "io.h"

// Syscall convention (matches Linux-style):
//   RAX = syscall number
//   RDI = arg1, RSI = arg2, RDX = arg3
//   Return value in RAX

static void syscall_handler(struct interrupt_frame *frame) {
    switch (frame->rax) {
    case SYS_WRITE: {
        const char *buf = (const char *)frame->rsi;
        uint64_t len = frame->rdx;
        for (uint64_t i = 0; i < len; i++)
            putchar(buf[i]);
        frame->rax = len;
        break;
    }
    case SYS_READ: {
        char *buf = (char *)frame->rsi;
        uint64_t len = frame->rdx;
        for (uint64_t i = 0; i < len; i++)
            buf[i] = getchar();
        frame->rax = len;
        break;
    }
    case SYS_EXIT: {
        shutdown();
        break;
    }
    default:
        printf("[syscall] unknown syscall %lu\n", frame->rax);
        frame->rax = (uint64_t)-1;
        break;
    }
}

void syscall_init(void) {
    isr_register_handler(0x80, syscall_handler);
}
