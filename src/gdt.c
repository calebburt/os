#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_hi;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr gdtr;

static void gdt_set_entry(int i, uint8_t access, uint8_t flags) {
    gdt[i].limit_low    = 0;
    gdt[i].base_low     = 0;
    gdt[i].base_mid     = 0;
    gdt[i].access       = access;
    gdt[i].flags_limit_hi = flags;
    gdt[i].base_high    = 0;
}

void gdt_init(void) {
    // Null descriptor
    gdt_set_entry(0, 0, 0);
    // Kernel code: present, DPL 0, code, readable, long mode
    gdt_set_entry(1, 0x9A, 0x20);
    // Kernel data: present, DPL 0, data, writable
    gdt_set_entry(2, 0x92, 0x00);

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt;

    asm volatile (
        "lgdt %0\n"
        "pushq $0x08\n"          // push kernel code segment
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"               // far return to reload CS
        "1:\n"
        "movw $0x10, %%ax\n"    // kernel data segment
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "movw %%ax, %%ss\n"
        :
        : "m"(gdtr)
        : "rax", "memory"
    );
}
