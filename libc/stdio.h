#ifndef STDIO_H
#define STDIO_H

#include "syscall.h"
#include <stdarg.h>

#define stdin 0
#define stdout 1
#define stderr 2

static inline int strlen(const char *str) {
    int len = 0;
    while (*str != '\0') {
        len++;
        str++;
    }
    return len;
}

static inline void putchar(char character) {
    sys_write(stdout, &character, 1);
}

static inline void print(const char* string) {
    sys_write(stdout, string, strlen(string));
}

static inline void puts(const char* string) {
    print(string);
    putchar('\n');
}

static inline long gets(char* buf, int len)  { // not actually the right api
    int i = 0;
    while (i < len - 1) {  // Leave room for null terminator
        char c;
        if (sys_read(stdin, &c, 1) <= 0) break;

        buf[i++] = c;
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

static inline int snprintf(char* buf, int size, const char* format, ...) {
    // This is a very minimal snprintf implementation that only supports %s and %d
    // It also does not support field width or precision. It is only intended for
    // simple use cases in this kernel.

    va_list args;
    va_start(args, format);

    int i = 0; // index in format string
    int j = 0; // index in output buffer

    while (format[i] && j < size - 1) {
        if (format[i] == '%') {
            i++;
            if (format[i] == 's') {
                char* str = va_arg(args, char*);
                while (*str && j < size - 1) {
                    buf[j++] = *str++;
                }
            } else if (format[i] == 'd') {
                int num = va_arg(args, int);
                char temp[16];
                int k = 0;
                if (num < 0) {
                    buf[j++] = '-';
                    num = -num;
                }
                do {
                    temp[k++] = '0' + (num % 10);
                    num /= 10;
                } while (num > 0 && k < sizeof(temp));
                for (int l = k - 1; l >= 0 && j < size - 1; l--) {
                    buf[j++] = temp[l];
                }
            } else {
                buf[j++] = format[i]; // Unsupported format specifier, just print it
            }
        } else {
            buf[j++] = format[i];
        }
        i++;
    }

    buf[j] = '\0';
    va_end(args);
    return j;
}

#endif