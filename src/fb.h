#ifndef FB_H
#define FB_H

#include <stdint.h>
#include <limine.h>

void fb_init(struct limine_framebuffer *framebuffer);
void print_char(char c, uint32_t color);
void print_string(const char *str, uint32_t color);
void clear_screen(uint32_t color);
void cursor_set(int x, int y);

#endif
