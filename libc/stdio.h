#ifndef STDIO_H
#define STDIO_H

#include "syscall.h"

int strlen(const char *str) {
    int len = 0;
    while (*str != '\0') {
        len++;
        str++;
    }
    return len;
}

static inline void putchar(char character) {
    sys_write(character);
}

static inline void print(const char* string) {
    int len = strlen(string);

    for (int i=0;i<len;i++) {
        sys_write(string[i]);
    }
}

static inline void puts(const char* string) {
    print(string);
    
    sys_write('\n');
}

static inline long gets(char* buf, int len)  { // not actually the right api 
    int i = 0;
    while (i < len - 1) {  // Leave room for null terminator
        char c = sys_read();

        buf[i++] = (char)c;
        if (c == '\b') { // handle backspace
            if (i > 1) {
                i -= 2;  // Remove the backspace and the character before it
            } else {
                i--;  // Just remove the backspace if it's the first character
            }
        }
        if (c == '\n') break;  // Stop at newline
    }
    
    buf[i] = '\0';
    return i;
}

#endif