#include "serial.h"
#include "io.h"
#include <stdint.h>

// Serial port configuration
#define SERIAL_PORT 0x3F8  // COM1
#define SERIAL_DATA 0
#define SERIAL_STATUS 5
#define SERIAL_TRANSMIT_EMPTY 0x20

// Initialize serial port (8n1 @ 115200 baud)
void serial_init(void) {
    uint16_t port = SERIAL_PORT;
    
    // Disable interrupts
    outb(port + 1, 0x00);
    
    // Set baud rate to 115200 (divisor = 1)
    outb(port + 3, 0x80);  // Enable DLAB
    outb(port + 0, 0x01);  // Divisor low byte
    outb(port + 1, 0x00);  // Divisor high byte
    
    // Set 8n1 (8 data bits, no parity, 1 stop bit)
    outb(port + 3, 0x03);
    
    // Enable FIFO
    outb(port + 2, 0xC7);
    
    // Set DTR and RTS
    outb(port + 4, 0x0B);
    
    // Test serial chip
    outb(port + 4, 0x1E);
    if (inb(port + 0) != 0xAE) {
        return;  // Serial port not working
    }
    
    // Re-enable normal operation
    outb(port + 4, 0x0F);
}

// Output a single byte to serial
void serial_putchar(char c) {
    uint16_t port = SERIAL_PORT;
    
    // Wait for transmit buffer to be empty
    while ((inb(port + SERIAL_STATUS) & SERIAL_TRANSMIT_EMPTY) == 0);
    
    // Send character
    outb(port + SERIAL_DATA, (uint8_t)c);
}
