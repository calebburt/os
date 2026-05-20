#include "fb.h"
#include "flanterm/flanterm.h"
#include <stddef.h>

extern struct flanterm_context *flanterm_ctx;

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

// Color is ignored — flanterm handles colors via ANSI escapes. Callers that
// want red/etc. can emit \x1b[3Xm themselves. These shims exist so the
// pre-stdio init prints in main.c keep working.
void print_char(char c, uint32_t color) {
    (void)color;
    if (!flanterm_ctx) return;
    flanterm_write(flanterm_ctx, &c, 1);
}

void print_string(const char *str, uint32_t color) {
    (void)color;
    if (!flanterm_ctx || !str) return;
    flanterm_write(flanterm_ctx, str, k_strlen(str));
}

void clear_screen(uint32_t color) {
    (void)color;
    if (!flanterm_ctx) return;
    flanterm_write(flanterm_ctx, "\x1b[2J\x1b[H", 7);
}

void cursor_set(int x, int y) {
    if (!flanterm_ctx) return;
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", y + 1, x + 1);
    flanterm_write(flanterm_ctx, buf, len);
}
