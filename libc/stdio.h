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

static inline int puts(const char* string) {
    int len = strlen(string);

    return sys_write(string, len);
}

#endif