#ifndef ISR_H
#define ISR_H

#include <stdint.h>

struct interrupt_frame {
    // Pushed by common stub (in reverse order)
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    // Pushed by stub
    uint64_t vector;
    uint64_t error_code;
    // Pushed by CPU
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef void (*isr_handler_t)(struct interrupt_frame *frame);

void isr_register_handler(uint8_t vector, isr_handler_t handler);
void isr_handler_main(struct interrupt_frame *frame);

#endif
