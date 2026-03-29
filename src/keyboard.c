#include "keyboard.h"
#include "io.h"
#include <stdint.h>

// PS/2 Keyboard support (QEMU window input)
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_OUTPUT_FULL  0x01  // Bit 0: output buffer full (data available)

// Scancode constants
#define PS2_LEFT_SHIFT   0x2A
#define PS2_RIGHT_SHIFT  0x36
#define PS2_BREAK_CODE   0x80  // High bit set = key release

// Unshifted scancode-to-ASCII mapping for US keyboard
static const char scancode_to_ascii_unshifted[] = {
    0,    27,  '1', '2', '3', '4', '5', '6',   // 0x00-0x07
    '7',  '8', '9', '0', '-', '=', '\b', '\t', // 0x08-0x0f
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  // 0x10-0x17
    'o',  'p', '[', ']', '\n', 0, 'a', 's',   // 0x18-0x1f
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',  // 0x20-0x27
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',   // 0x28-0x2f
    'b',  'n', 'm', ',', '.', '/', 0, '*',    // 0x30-0x37
    0,    ' ',  0,   0,   0,   0,   0,   0,     // 0x38-0x3f (0x39 = space)
};

// Shifted scancode-to-ASCII mapping for US keyboard
static const char scancode_to_ascii_shifted[] = {
    0,    27,  '!', '@', '#', '$', '%', '^',   // 0x00-0x07
    '&',  '*', '(', ')', '_', '+', '\b', '\t', // 0x08-0x0f
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',  // 0x10-0x17
    'O',  'P', '{', '}', '\n', 0, 'A', 'S',   // 0x18-0x1f
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',  // 0x20-0x27
    '"',  '~', 0, '|', 'Z', 'X', 'C', 'V',    // 0x28-0x2f
    'B',  'N', 'M', '<', '>', '?', 0, '*',    // 0x30-0x37
    0,    ' ',  0,   0,   0,   0,   0,   0,     // 0x38-0x3f (0x39 = space)
};

// Shift key state
static int shift_pressed = 0;


// Check if keyboard data is available
static int keyboard_has_data(void) {
    return (inb(PS2_STATUS_PORT) & PS2_OUTPUT_FULL);
}

// Read a single character from keyboard (non-blocking, returns 0 if no data)
static char keyboard_getchar(void) {
    if (!keyboard_has_data()) {
        return 0;
    }
    
    uint8_t scancode = inb(PS2_DATA_PORT);
    
    // Handle shift key presses
    if ((scancode & 0x7F) == PS2_LEFT_SHIFT || (scancode & 0x7F) == PS2_RIGHT_SHIFT) {
        if (scancode & PS2_BREAK_CODE) {
            shift_pressed = 0;  // Shift released
        } else {
            shift_pressed = 1;  // Shift pressed
        }
        return 0;  // Don't output shift key
    }
    
    // Handle break codes (key release) - skip them
    if (scancode & PS2_BREAK_CODE) {
        return 0;
    }
    
    // Convert scancode to ASCII using appropriate table
    const char *table = shift_pressed ? scancode_to_ascii_shifted : scancode_to_ascii_unshifted;
    if (scancode < (sizeof(scancode_to_ascii_unshifted) / sizeof(char))) {
        return table[scancode];
    }
    
    return 0;
}

// Read from keyboard into buffer (non-blocking)
int keyboard_read(char *buf, size_t len) {
    if (!buf || len == 0) return 0;
    
    size_t i = 0;
    while (i < len) {
        char c = keyboard_getchar();
        if (c == 0) break;  // No more data available
        buf[i++] = c;
    }
    return i;
}

// Blocking read from keyboard (waits for data)
char keyboard_getchar_blocking(void) {
    char c = 0;
    
    while (c == 0) {
        while (!keyboard_has_data());
        
        uint8_t scancode = inb(PS2_DATA_PORT);
        
        // Handle shift key presses
        if ((scancode & 0x7F) == PS2_LEFT_SHIFT || (scancode & 0x7F) == PS2_RIGHT_SHIFT) {
            if (scancode & PS2_BREAK_CODE) {
                shift_pressed = 0;  // Shift released
            } else {
                shift_pressed = 1;  // Shift pressed
            }
            continue;  // Wait for next key
        }
        
        // Skip break codes (key release)
        if (scancode & PS2_BREAK_CODE) {
            continue;
        }
        
        // Convert scancode to ASCII
        const char *table = shift_pressed ? scancode_to_ascii_shifted : scancode_to_ascii_unshifted;
        if (scancode < (sizeof(scancode_to_ascii_unshifted) / sizeof(char))) {
            c = table[scancode];
        }
    }
    
    return c;
}
