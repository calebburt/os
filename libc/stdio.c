#include "stdio.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ── Internal string helpers (mirrors kernel's stdio_printf.c) ──────────────

static size_t u_strlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void u_reverse(char *s, size_t len) {
    size_t i = 0, j = len - 1;
    while (i < j) {
        char tmp = s[i]; s[i] = s[j]; s[j] = tmp;
        i++; j--;
    }
}

static void u_utoa(uint64_t value, unsigned base, bool upper,
                   char *buf, size_t *out_len) {
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
    u_reverse(buf, pos);
    *out_len = pos;
}

// ── Syscall-backed I/O callbacks ───────────────────────────────────────────

static int stdout_write(struct FILE *stream, const char *buf, size_t len) {
    (void)stream;
    return (int)sys_write(1, buf, (long)len);
}

static int stderr_write(struct FILE *stream, const char *buf, size_t len) {
    (void)stream;
    return (int)sys_write(2, buf, (long)len);
}

static int stdin_read(struct FILE *stream, char *buf, size_t len) {
    (void)stream;
    return (int)sys_read(0, buf, (long)len);
}

static int file_write(struct FILE *stream, const char *buf, size_t len) {
    long fd = (long)(uintptr_t)stream->fs_data;
    return (int)sys_write(fd, buf, (long)len);
}

static int file_read(struct FILE *stream, char *buf, size_t len) {
    long fd = (long)(uintptr_t)stream->fs_data;
    return (int)sys_read(fd, buf, (long)len);
}

static int file_seek(struct FILE *stream, long offset, int whence) {
    long fd = (long)(uintptr_t)stream->fs_data;
    return (int)sys_lseek(fd, offset, whence);
}

static int file_close(struct FILE *stream) {
    long fd = (long)(uintptr_t)stream->fs_data;
    return (int)sys_close(fd);
}

// ── Standard stream FILE structs ───────────────────────────────────────────

static FILE stdout_file = {
    .write    = stdout_write,
    .read     = NULL,
    .seek     = NULL,
    .close    = NULL,
    .flags    = O_WRONLY,
    .fs_data  = NULL,
    .position = 0,
    .size     = 0,
    .type     = FILE_TYPE_TTY,
};

static FILE stderr_file = {
    .write    = stderr_write,
    .read     = NULL,
    .seek     = NULL,
    .close    = NULL,
    .flags    = O_WRONLY,
    .fs_data  = NULL,
    .position = 0,
    .size     = 0,
    .type     = FILE_TYPE_TTY,
};

static FILE stdin_file = {
    .write    = NULL,
    .read     = stdin_read,
    .seek     = NULL,
    .close    = NULL,
    .flags    = O_RDONLY,
    .fs_data  = NULL,
    .position = 0,
    .size     = 0,
    .type     = FILE_TYPE_TTY,
};

FILE *stdout = &stdout_file;
FILE *stderr = &stderr_file;
FILE *stdin  = &stdin_file;

// ── Output functions ───────────────────────────────────────────────────────

int fputc(int c, FILE *stream) {
    if (!stream || !stream->write) return -1;
    char ch = (char)c;
    if (stream->write(stream, &ch, 1) < 0) return -1;
    return c;
}

int fputs(const char *str, FILE *stream) {
    if (!stream || !str || !stream->write) return -1;
    size_t len = u_strlen(str);
    if (stream->write(stream, str, len) < 0) return -1;
    return (int)len;
}

int putchar(int c) {
    return fputc(c, stdout);
}

int puts(const char *str) {
    if (fputs(str, stdout) < 0) return -1;
    return fputc('\n', stdout);
}

// ── Input functions ────────────────────────────────────────────────────────

int fgetc(FILE *stream) {
    if (!stream || !stream->read) return -1;
    char ch;
    if (stream->read(stream, &ch, 1) <= 0) return -1;
    return (int)(unsigned char)ch;
}

int getchar(void) {
    return fgetc(stdin);
    // The kernel's SYS_READ handler echoes via its own getchar(); no re-echo needed.
}

char *fgets(char *str, int n, FILE *stream) {
    if (!stream || !str || n <= 0 || !stream->read) return NULL;

    int i = 0;
    while (i < n - 1) {
        int c = fgetc(stream);
        if (c == -1) {
            if (i == 0) return NULL;
            break;
        }
        str[i++] = (char)c;
        if (c == '\b') {
            // Kernel SYS_READ already echoed the backspace; adjust the buffer.
            if (i > 1) {
                i -= 2;  // remove the backspace and the char before it
            } else {
                i--;     // nothing before the backspace to remove
            }
        }
        if (c == '\n') break;
    }

    str[i] = '\0';
    return str;
}

// ── printf family ──────────────────────────────────────────────────────────

// Convert a finite, non-negative double to a string.
// fmt: 'f'/'F', 'e'/'E', 'g'/'G'.  prec: precision (decimal places for f/e,
// significant digits for g).  Returns number of characters written.
static int format_double(char *buf, int bufsz, double v, char fmt, int prec) {
    int pos = 0;
    if (bufsz <= 1) { if (bufsz == 1) buf[0] = '\0'; return 0; }

#define FD_EMIT(c) do { if (pos < bufsz - 1) buf[pos++] = (char)(c); } while (0)

    // Inspect bits for NaN/Inf detection
    uint64_t bits;
    __builtin_memcpy(&bits, &v, sizeof bits);
    bool neg = (bits >> 63) != 0;
    int biased = (int)((bits >> 52) & 0x7FF);

    if (biased == 0x7FF) {
        uint64_t mant = bits & 0x000FFFFFFFFFFFFFULL;
        if (neg) FD_EMIT('-');
        if (mant) { FD_EMIT('n'); FD_EMIT('a'); FD_EMIT('n'); }
        else      { FD_EMIT('i'); FD_EMIT('n'); FD_EMIT('f'); }
        buf[pos] = '\0'; return pos;
    }

    if (neg) { FD_EMIT('-'); v = -v; }

    int actual_prec = (prec < 0) ? 6 : prec;
    if ((fmt == 'g' || fmt == 'G') && actual_prec == 0) actual_prec = 1;

    if (v == 0.0) {
        FD_EMIT('0');
        if (fmt == 'f' || fmt == 'F') {
            if (actual_prec > 0) { FD_EMIT('.'); for (int i = 0; i < actual_prec; i++) FD_EMIT('0'); }
        } else if (fmt == 'e' || fmt == 'E') {
            FD_EMIT('.'); for (int i = 0; i < actual_prec; i++) FD_EMIT('0');
            FD_EMIT(fmt); FD_EMIT('+'); FD_EMIT('0'); FD_EMIT('0');
        }
        buf[pos] = '\0'; return pos;
    }

    // Scale v to [1, 10) iteratively, tracking the base-10 exponent
    double s = v;
    int exp10 = 0;
    if (s >= 10.0) { while (s >= 10.0) { s /= 10.0; exp10++; } }
    else           { while (s <  1.0)  { s *= 10.0; exp10--; } }

    // Choose scientific vs fixed notation
    bool use_sci = (fmt == 'e' || fmt == 'E') ||
                   ((fmt == 'g' || fmt == 'G') && (exp10 < -4 || exp10 >= actual_prec));

    // How many significant digits to extract
    int nsig;
    if (fmt == 'f' || fmt == 'F') {
        nsig = exp10 + 1 + actual_prec;
        if (nsig < 1) nsig = 1;
    } else { // e/E/g/G
        nsig = actual_prec + (use_sci ? 1 : 0);
        if (nsig < 1) nsig = 1;
    }
    if (nsig > 55) nsig = 55;

    // Extract digits one at a time from the scaled value
    char digs[64];
    double cur = s;
    for (int i = 0; i < nsig; i++) {
        int d = (int)cur; if (d < 0) d = 0; if (d > 9) d = 9;
        digs[i] = (char)('0' + d);
        cur = (cur - (double)d) * 10.0;
    }
    // Round based on the next digit
    if ((int)cur >= 5) {
        int carry = 1;
        for (int i = nsig - 1; i >= 0 && carry; i--) {
            int nd = (digs[i] - '0') + carry;
            digs[i] = (char)('0' + nd % 10);
            carry = nd / 10;
        }
        if (carry) { // e.g. 9.999 -> 10.00
            for (int i = nsig - 1; i > 0; i--) digs[i] = digs[i - 1];
            digs[0] = '1'; for (int i = 1; i < nsig; i++) digs[i] = '0';
            exp10++;
            if (fmt == 'g' || fmt == 'G')
                use_sci = (exp10 < -4 || exp10 >= actual_prec);
        }
    }

    // For %g/%G: strip trailing zeros from fractional part
    int last = nsig - 1;
    if (fmt == 'g' || fmt == 'G') {
        while (last > 0 && digs[last] == '0') last--;
    }

    if (use_sci) {
        FD_EMIT(digs[0]);
        if (last > 0) { FD_EMIT('.'); for (int i = 1; i <= last; i++) FD_EMIT(digs[i]); }
        FD_EMIT((fmt == 'E' || fmt == 'G') ? 'E' : 'e');
        int ae = exp10; if (ae < 0) { FD_EMIT('-'); ae = -ae; } else FD_EMIT('+');
        if (ae >= 100) FD_EMIT('0' + ae / 100);
        FD_EMIT('0' + (ae / 10) % 10);
        FD_EMIT('0' + ae % 10);
    } else if (exp10 >= 0) {
        int iw = exp10 + 1; // integer digit count
        for (int i = 0; i < iw; i++) FD_EMIT(i < nsig ? digs[i] : '0');
        if (fmt == 'f' || fmt == 'F') {
            if (actual_prec > 0) {
                FD_EMIT('.');
                for (int i = 0; i < actual_prec; i++) {
                    int idx = iw + i;
                    FD_EMIT(idx < nsig ? digs[idx] : '0');
                }
            }
        } else if (last >= iw) {
            FD_EMIT('.');
            for (int i = iw; i <= last; i++) FD_EMIT(digs[i]);
        }
    } else { // 0.000...
        FD_EMIT('0'); FD_EMIT('.');
        int leading = -exp10 - 1;
        for (int i = 0; i < leading; i++) FD_EMIT('0');
        if (fmt == 'f' || fmt == 'F') {
            int rem = actual_prec - leading;
            for (int i = 0; i < rem; i++) FD_EMIT(i < nsig ? digs[i] : '0');
        } else {
            for (int i = 0; i <= last; i++) FD_EMIT(digs[i]);
        }
    }

    buf[pos] = '\0'; return pos;
#undef FD_EMIT
}

int vfprintf(FILE *stream, const char *format, va_list args) {
    if (!stream || !format) return -1;

    int count = 0;
    char numbuf[64];

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

        // Skip flags: -, +, space, 0, #
        while (*format == '-' || *format == '+' || *format == ' ' ||
               *format == '0' || *format == '#')
            format++;

        // Skip field width (digits or '*')
        if (*format == '*') { va_arg(args, int); format++; }
        else while (*format >= '1' && *format <= '9') format++;

        // Parse precision
        int prec = -1;
        if (*format == '.') {
            format++;
            prec = 0;
            if (*format == '*') { prec = va_arg(args, int); format++; }
            else while (*format >= '0' && *format <= '9') prec = prec * 10 + (*format++ - '0');
        }

        // Parse length modifier: h hh l ll z j t L
        int lmod = 0; // 0=int  1=long  2=long long  3=size_t
        if (*format == 'h') {
            format++; lmod = -1; if (*format == 'h') { format++; lmod = -2; }
        } else if (*format == 'l') {
            format++; lmod = 1; if (*format == 'l') { format++; lmod = 2; }
        } else if (*format == 'z') { format++; lmod = 3; }
        else if (*format == 'j' || *format == 't' || *format == 'L') format++;

        switch (*format) {
            case 'c': {
                int c = va_arg(args, int);
                fputc(c, stream);
                count++;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int written = fputs(s, stream);
                if (written < 0) return -1;
                count += written;
                break;
            }
            case 'd': case 'i': {
                long long val;
                if      (lmod == 2)  val = va_arg(args, long long);
                else if (lmod == 1)  val = va_arg(args, long);
                else if (lmod == 3)  val = (long long)va_arg(args, size_t);
                else                 val = va_arg(args, int);
                bool neg = val < 0;
                uint64_t uval = neg ? (uint64_t)(-(uint64_t)val) : (uint64_t)val;
                size_t len;
                u_utoa(uval, 10, false, numbuf, &len);
                if (neg) { fputc('-', stream); count++; }
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'u': {
                uint64_t uval;
                if      (lmod == 2)  uval = va_arg(args, unsigned long long);
                else if (lmod == 1)  uval = va_arg(args, unsigned long);
                else if (lmod == 3)  uval = (uint64_t)va_arg(args, size_t);
                else                 uval = va_arg(args, unsigned int);
                size_t len;
                u_utoa(uval, 10, false, numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'x': case 'X': {
                uint64_t uval;
                if      (lmod == 2)  uval = va_arg(args, unsigned long long);
                else if (lmod == 1)  uval = va_arg(args, unsigned long);
                else if (lmod == 3)  uval = (uint64_t)va_arg(args, size_t);
                else                 uval = va_arg(args, unsigned int);
                size_t len;
                u_utoa(uval, 16, (*format == 'X'), numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(args, void *);
                fputs("0x", stream);
                count += 2;
                size_t len;
                u_utoa((uint64_t)ptr, 16, false, numbuf, &len);
                fputs(numbuf, stream);
                count += (int)len;
                break;
            }
            case 'f': case 'F':
            case 'e': case 'E':
            case 'g': case 'G': {
                double val = va_arg(args, double);
                int n = format_double(numbuf, (int)sizeof numbuf, val, *format, prec);
                fputs(numbuf, stream);
                count += n;
                break;
            }
            default:
                fputc('%', stream);
                fputc(*format, stream);
                count += 2;
                break;
        }
        format++;
    }

    return count;
}

int fprintf(FILE *stream, const char *format, ...) {
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

// ── snprintf / vsnprintf ───────────────────────────────────────────────────

struct str_buf_state {
    char *buf;
    int   size;
    int   pos;
};

static int str_buf_write(FILE *stream, const char *data, size_t len) {
    struct str_buf_state *sb = (struct str_buf_state *)stream->fs_data;
    for (size_t i = 0; i < len; i++) {
        if (sb->pos < sb->size - 1)
            sb->buf[sb->pos++] = data[i];
    }
    return (int)len;
}

int vsnprintf(char *buf, int size, const char *format, va_list args) {
    if (!buf || size <= 0) return 0;
    struct str_buf_state sb = { buf, size, 0 };
    struct FILE sf = {
        .write    = str_buf_write,
        .read     = NULL,
        .seek     = NULL,
        .close    = NULL,
        .flags    = O_WRONLY,
        .fs_data  = &sb,
        .position = 0,
        .size     = 0,
        .type     = 0,
    };
    int r = vfprintf(&sf, format, args);
    buf[sb.pos] = '\0';
    return r;
}

int snprintf(char *buf, int size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int r = vsnprintf(buf, size, format, args);
    va_end(args);
    return r;
}

// ── File pool for fopen ────────────────────────────────────────────────────

#define FILE_POOL_SIZE 16

static struct FILE file_pool[FILE_POOL_SIZE];
static int         file_pool_used[FILE_POOL_SIZE];

void stdio_init(void) {
    // stdin/stdout/stderr are statically initialised; nothing to do.
}

FILE *fopen(const char *path, const char *mode) {
    if (!path || !mode) return NULL;

    int flags = O_RDONLY;
    if (mode[0] == 'r') {
        flags = (mode[1] == '+') ? O_RDWR : O_RDONLY;
    } else if (mode[0] == 'w') {
        flags = (mode[1] == '+') ? (O_RDWR | O_CREAT | O_TRUNC)
                                 : (O_WRONLY | O_CREAT | O_TRUNC);
    } else if (mode[0] == 'a') {
        flags = (mode[1] == '+') ? (O_RDWR | O_CREAT | O_APPEND)
                                 : (O_WRONLY | O_CREAT | O_APPEND);
    }

    long fd = sys_open((char *)path, flags);
    if (fd < 0) return NULL;

    for (int i = 0; i < FILE_POOL_SIZE; i++) {
        if (!file_pool_used[i]) {
            file_pool_used[i] = 1;
            file_pool[i].write    = file_write;
            file_pool[i].read     = file_read;
            file_pool[i].seek     = file_seek;
            file_pool[i].close    = file_close;
            file_pool[i].flags    = (uint32_t)flags;
            file_pool[i].fs_data  = (void *)(uintptr_t)fd;
            file_pool[i].position = 0;
            file_pool[i].size     = 0;
            file_pool[i].type     = FILE_TYPE_REGULAR;
            return &file_pool[i];
        }
    }

    sys_close(fd);
    return NULL;
}

int fclose(FILE *stream) {
    if (!stream || !stream->close) return -1;
    int r = stream->close(stream);
    for (int i = 0; i < FILE_POOL_SIZE; i++) {
        if (&file_pool[i] == stream) {
            file_pool_used[i] = 0;
            break;
        }
    }
    return r;
}

size_t fread(void *buf, size_t size, size_t count, FILE *stream) {
    if (!stream || !stream->read || !buf || size == 0) return 0;
    size_t total = size * count;
    int n = stream->read(stream, (char *)buf, total);
    if (n <= 0) return 0;
    stream->position += (size_t)n;
    return (size_t)n / size;
}

size_t fwrite(const void *buf, size_t size, size_t count, FILE *stream) {
    if (!stream || !stream->write || !buf || size == 0) return 0;
    size_t total = size * count;
    int n = stream->write(stream, (const char *)buf, total);
    if (n <= 0) return 0;
    stream->position += (size_t)n;
    return (size_t)n / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream || !stream->seek) return -1;
    int r = stream->seek(stream, offset, whence);
    if (r >= 0) stream->position = (size_t)r;
    return r >= 0 ? 0 : -1;
}

long ftell(FILE *stream) {
    if (!stream) return -1;
    return (long)stream->position;
}

void rewind(FILE *stream) {
    fseek(stream, 0L, SEEK_SET);
}
