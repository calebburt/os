#ifndef STDIO_H
#define STDIO_H

#include "syscall.h"
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

struct FILE {
    int (*write)(struct FILE *stream, const char *buf, size_t len);
    int (*read)(struct FILE *stream, char *buf, size_t len);
    int (*seek)(struct FILE *stream, long offset, int whence);
    int (*close)(struct FILE *stream);

    uint32_t flags;     // O_RDONLY, O_WRONLY, etc.
    void    *fs_data;   // fd stored as (void*)(uintptr_t)fd for regular files
    size_t   position;
    size_t   size;
    uint8_t  type;
};

typedef struct FILE FILE;

#define FILE_TYPE_TTY       0x01
#define FILE_TYPE_REGULAR   0x02
#define FILE_TYPE_DIR       0x04
#define FILE_TYPE_DEVICE    0x08

extern FILE *stdout;
extern FILE *stderr;
extern FILE *stdin;

// Standard I/O
int    fputc(int c, FILE *stream);
int    fputs(const char *str, FILE *stream);
int    putchar(int c);
int    puts(const char *str);
int    fgetc(FILE *stream);
int    getchar(void);
char  *fgets(char *str, int n, FILE *stream);
int    fprintf(FILE *stream, const char *format, ...);
int    vfprintf(FILE *stream, const char *format, va_list args);
int    printf(const char *format, ...);
int    vprintf(const char *format, va_list args);
int    snprintf(char *buf, int size, const char *format, ...);
int    vsnprintf(char *buf, int size, const char *format, va_list args);
void   stdio_init(void);

// File operations
FILE  *fopen(const char *path, const char *mode);
size_t fread(void *buf, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buf, size_t size, size_t count, FILE *stream);
int    fseek(FILE *stream, long offset, int whence);
long   ftell(FILE *stream);
void   rewind(FILE *stream);
int    fclose(FILE *stream);

// Backward-compat helpers (used by existing user programs)
// Guard strlen so it doesn't conflict when <string.h> is also included.
#ifndef _STRING_H
static inline size_t strlen(const char *str) {
    size_t len = 0;
    while (*str++) len++;
    return len;
}
#endif

static inline void print(const char *str) {
    fputs(str, stdout);
}

static inline long gets(char *buf, int len) {
    return fgets(buf, len, stdin) ? (long)strlen(buf) : 0;
}

#endif
