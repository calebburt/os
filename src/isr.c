#include "isr.h"
#include "pic.h"
#include "stdio.h"

static isr_handler_t handlers[256] = {0};

static const char *exception_names[] = {
    "Division Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization Exception", "Control Protection Exception",
};

void isr_register_handler(uint8_t vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

void isr_handler_main(struct interrupt_frame *frame) {
    if (handlers[frame->vector]) {
        handlers[frame->vector](frame);
    } else if (frame->vector < 32) {
        printf("\n!!! EXCEPTION: %s (#%d) error_code=%d\n",
               frame->vector < 22 ? exception_names[frame->vector] : "Unknown",
               frame->vector, frame->error_code);
        printf("    RIP=0x%d CS=0x%d RFLAGS=0x%d\n",
               frame->rip, frame->cs, frame->rflags);
        printf("    RAX=0x%d RBX=0x%d RCX=0x%d RDX=0x%d\n",
               frame->rax, frame->rbx, frame->rcx, frame->rdx);
        // Halt on unhandled exception
        for (;;) asm volatile ("hlt");
    }

    // Send EOI for hardware IRQs (vectors 32-47)
    if (frame->vector >= 32 && frame->vector < 48) {
        pic_send_eoi(frame->vector - 32);
    }
}
