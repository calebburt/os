#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
#include "syscall.h"

void  *malloc(size_t size);
void  *calloc(size_t nmemb, size_t size);
void  *realloc(void *ptr, size_t size);
void   free(void *ptr);

static inline void exit(int code) {
    (void)code;
    sys_exit();
}

double strtod(const char *s, char **endptr);

// Simple number conversions
static inline int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

#endif
