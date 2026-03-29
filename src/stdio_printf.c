#include "stdio.h"
#include "stdio_internal.h"
#include <stdint.h>
#include <stdbool.h>

// String utility: get length
size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

// String utility: reverse in place
void k_reverse(char *s, size_t len) {
    size_t i = 0;
    size_t j = len - 1;
    while (i < j) {
        char tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
        i++;
        j--;
    }
}

// Convert unsigned integer to string with configurable base
void k_utoa(uint64_t value, unsigned base, bool upper, char *buf, size_t *out_len) {
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t pos = 0;
    if (value == 0) {
        buf[pos++] = '0';
    } else {
        while (value != 0) {
            buf[pos++] = digits[value % base];
            value /= base;
        }
    }
    buf[pos] = '\0';
    k_reverse(buf, pos);
    *out_len = pos;
}

// Core printf implementation
int vfprintf(struct FILE *stream, const char *format, va_list args) {
    if (!stream || !format) {
        return -1;
    }

    int count = 0;
    char numbuf[32];

    while (*format) {
        if (*format != '%') {
            fputc(*format++, stream);
            count++;
            continue;
        }

        format++; // eat '%'
        if (*format == '%') {
            fputc('%', stream);
            count++;
            format++;
            continue;
        }

        switch (*format) {
            case 'c': {
                int c = va_arg(args, int);
                fputc(c, stream);
                count++;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (s == NULL) {
                    s = "(null)";
                }
                int written = fputs(s, stream);
                if (written < 0) return -1;
                count += written;
                break;
            }
            case 'd':
            case 'i': {
                int value = va_arg(args, int);
                bool neg = false;
                uint64_t uvalue;
                if (value < 0) {
                    neg = true;
                    uvalue = (uint64_t)(-(int64_t)value);
                } else {
                    uvalue = (uint64_t)value;
                }
                size_t len;
                k_utoa(uvalue, 10, false, numbuf, &len);
                if (neg) {
                    fputc('-', stream);
                    count++;
                }
                fputs(numbuf, stream);
                count += (int)len;
                if (neg) count++;
                break;
            }
            case 'u': {
                uint64_t value = va_arg(args, unsigned int);
                size_t len;
                k_utoa(value, 10, false, numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'x':
            case 'X': {
                unsigned int value = va_arg(args, unsigned int);
                size_t len;
                k_utoa(value, 16, (*format == 'X'), numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(args, void *);
                fputs("0x", stream);
                count += 2;
                size_t len;
                k_utoa(ptr, 16, false, numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            default:
                // Unknown format specifier, print literally
                fputc('%', stream);
                fputc(*format, stream);
                count += 2;
                break;
        }
        format++;
    }

    return count;
}

int fprintf(struct FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int vprintf(const char *format, va_list args) {
    return vfprintf(stdout, format, args);
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stdout, format, args);
    va_end(args);
    return result;
}
