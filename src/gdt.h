#ifndef GDT_H
#define GDT_H

#include <stdint.h>

#define KERNEL_CODE_SEG 0x08
#define KERNEL_DATA_SEG 0x10

void gdt_init(void);

#endif
