#ifndef STDIO_INTERNAL_H
#define STDIO_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

// Helper functions for string/number conversion
size_t k_strlen(const char *s);
void k_reverse(char *s, size_t len);
void k_utoa(uint64_t value, unsigned base, bool upper, char *buf, size_t *out_len);

#endif
