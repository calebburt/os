#include "idt.h"
#include "gdt.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr idtr;

// Each ISR stub is aligned to 32 bytes in isr.S
extern char isr_stubs_start[];
#define ISR_STUB_SIZE 32

void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr) {
    idt[vector].offset_low  = handler & 0xFFFF;
    idt[vector].selector    = KERNEL_CODE_SEG;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vector].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vector].reserved    = 0;
}

void idt_init(void) {
    uint64_t base = (uint64_t)isr_stubs_start;

    for (int i = 0; i < 256; i++) {
        // 0x8E = present, DPL 0, interrupt gate
        // 0xEE = present, DPL 3, interrupt gate (for int 0x80 syscall)
        uint8_t attr = (i == 0x80) ? 0xEE : 0x8E;
        idt_set_gate(i, base + i * ISR_STUB_SIZE, attr);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;

    asm volatile ("lidt %0" : : "m"(idtr));
}
