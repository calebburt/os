#ifndef STDIO_H
#define STDIO_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "fb.h"

// File open flags
#define O_RDONLY    0x0000  // Read only
#define O_WRONLY    0x0001  // Write only
#define O_RDWR      0x0002  // Read and write
#define O_APPEND    0x0008  // Append mode
#define O_CREAT     0x0100  // Create if doesn't exist
#define O_TRUNC     0x0200  // Truncate if exists

// Seek whence constants
#define SEEK_SET    0       // Beginning of file
#define SEEK_CUR    1       // Current position
#define SEEK_END    2       // End of file

struct FILE {
    // I/O operations
    int (*write)(struct FILE *stream, const char *buf, size_t len);
    int (*read)(struct FILE *stream, char *buf, size_t len);
    int (*seek)(struct FILE *stream, long offset, int whence);
    int (*close)(struct FILE *stream);
    
    // File metadata
    uint32_t flags;         // File mode flags (O_RDONLY, O_WRONLY, O_APPEND, etc.)
    void *fs_data;          // Filesystem-specific data (inode pointer, file handle, etc.)
    
    // Position and size tracking
    size_t position;        // Current file position
    size_t size;            // File size (may be 0 for special files like TTY)
    
    // Type identification
    uint8_t type;           // FILE_TYPE_TTY, FILE_TYPE_REGULAR, FILE_TYPE_DIR, etc.
};

// File type constants
#define FILE_TYPE_TTY       0x01  // Terminal/serial
#define FILE_TYPE_REGULAR   0x02  // Regular file
#define FILE_TYPE_DIR       0x04  // Directory
#define FILE_TYPE_DEVICE    0x08  // Device file

extern struct FILE *stdout;
extern struct FILE *stderr;
extern struct FILE *stdin;

// Standard I/O functions
int fputc(int c, struct FILE *stream);
int fputs(const char *str, struct FILE *stream);
int putchar(int c);
int puts(const char *str);
int fgetc(struct FILE *stream);
int getchar(void);
char *fgets(char *str, int n, struct FILE *stream);
int fprintf(struct FILE *stream, const char *format, ...);
int vfprintf(struct FILE *stream, const char *format, va_list args);
int printf(const char *format, ...);
int vprintf(const char *format, va_list args);
void stdio_init(void);

// Filesystem-ready functions (to be implemented with filesystem drivers)
// struct FILE *fopen(const char *path, const char *mode);
// int fread(char *buf, size_t size, size_t count, struct FILE *stream);
// int fclose(struct FILE *stream);
// int fseek(struct FILE *stream, long offset, int whence);
// long ftell(struct FILE *stream);

#endif