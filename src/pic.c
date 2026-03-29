#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

void pic_init(void) {
    // Save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    // ICW1: begin initialization sequence
    outb(PIC1_CMD, 0x11); io_wait();
    outb(PIC2_CMD, 0x11); io_wait();

    // ICW2: vector offsets
    outb(PIC1_DATA, PIC1_OFFSET); io_wait();
    outb(PIC2_DATA, PIC2_OFFSET); io_wait();

    // ICW3: master/slave wiring
    outb(PIC1_DATA, 0x04); io_wait(); // slave on IRQ2
    outb(PIC2_DATA, 0x02); io_wait(); // slave identity

    // ICW4: 8086 mode
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();

    // Restore masks (all masked by default)
    (void)mask1; (void)mask2;
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}
