#ifndef FB_H
#define FB_H

#include <stdint.h>
#include <limine.h>

// Global framebuffer
extern struct limine_framebuffer *fb_global;

void fb_init(struct limine_framebuffer *framebuffer);
void unset_pixel(int x, int y);
void put_pixel(int x, int y, uint32_t color);
void put_char(int x, int y, char c, uint32_t color);
void put_string(int x, int y, const char *str, uint32_t color);
void print_char(char c, uint32_t color);
void print_string(const char *str, uint32_t color);
void clear_screen(uint32_t color);
void cursor_set(int x, int y);

#endif
