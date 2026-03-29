#include "stdio.h"
#include "stdio_internal.h"
#include "serial.h"
#include "keyboard.h"
#include "fb.h"
#include "vfs.h"

// Write to framebuffer and serial
static int fb_write(struct FILE *stream, const char *buf, size_t len) {
    if (!stream || !buf) {
        return -1;
    }
    for (size_t i = 0; i < len; i++) {
        print_char(buf[i], 0xFFFFFF);  // White text
        serial_putchar(buf[i]);        // Also output to serial
    }
    return (int)len;
}

// Read from keyboard
static int keyboard_read_func(struct FILE *stream, char *buf, size_t len) {
    if (!stream || !buf || len == 0) {
        return -1;
    }
    
    // For single character reads (like fgetc), block until we get input
    if (len == 1) {
        buf[0] = keyboard_getchar_blocking();
        return 1;
    }
    
    // For multi-character reads (like fgets), use non-blocking
    return keyboard_read(buf, len);
}

// Initialize stdio subsystem
void stdio_init(void) {
    serial_init();
    vfs_init();
}

// FILE structures for standard streams
static struct FILE stdout_file = {
    .write = fb_write,
    .read = NULL,           // stdout/stderr are write-only
    .seek = NULL,
    .close = NULL,
    .flags = O_WRONLY,
    .fs_data = NULL,
    .position = 0,
    .size = 0,
    .type = FILE_TYPE_TTY   // TTY device (framebuffer + serial)
};

static struct FILE stderr_file = {
    .write = fb_write,
    .read = NULL,           // stderr is write-only
    .seek = NULL,
    .close = NULL,
    .flags = O_WRONLY,
    .fs_data = NULL,
    .position = 0,
    .size = 0,
    .type = FILE_TYPE_TTY   // TTY device
};

static struct FILE stdin_file = {
    .write = NULL,          // stdin is read-only
    .read = keyboard_read_func,
    .seek = NULL,
    .close = NULL,
    .flags = O_RDONLY,
    .fs_data = NULL,
    .position = 0,
    .size = 0,
    .type = FILE_TYPE_TTY   // TTY device (keyboard)
};

struct FILE *stdout = &stdout_file;
struct FILE *stderr = &stderr_file;
struct FILE *stdin = &stdin_file;

// Output functions
int fputc(int c, struct FILE *stream) {
    if (!stream) {
        return -1;
    }
    char ch = (char)c;
    if (stream->write(stream, &ch, 1) < 0) {
        return -1;
    }
    return c;
}

int fputs(const char *str, struct FILE *stream) {
    if (!stream || !str) {
        return -1;
    }
    size_t len = k_strlen(str);
    if (stream->write(stream, str, len) < 0) {
        return -1;
    }
    return (int)len;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int puts(const char *str) {
    if (fputs(str, stdout) < 0) {
        return -1;
    }
    return fputc('\n', stdout);
}

// Input functions
int fgetc(struct FILE *stream) {
    if (!stream || !stream->read) {
        return -1;
    }
    char ch;
    if (stream->read(stream, &ch, 1) <= 0) {
        return -1;
    }
    return (int)(unsigned char)ch;
}

int getchar(void) {
    int c = fgetc(stdin);
    putchar(c);  // Echo the character back to the user
    return c;
}

char *fgets(char *str, int n, struct FILE *stream) {
    if (!stream || !str || n <= 0 || !stream->read) {
        return NULL;
    }
    
    int i = 0;
    while (i < n - 1) {  // Leave room for null terminator
        int c = fgetc(stream);
        if (c == -1) {
            if (i == 0) return NULL;  // EOF and no characters read
            break;
        }
        str[i++] = (char)c;
        if (stream == stdin) {
            putchar(c);  // Echo input characters back to the user
        }
        if (c == '\n') break;  // Stop at newline
    }
    
    str[i] = '\0';
    return str;
}
