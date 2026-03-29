#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr);

#endif
