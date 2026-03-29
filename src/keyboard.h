#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stddef.h>

char keyboard_getchar_blocking(void);
int keyboard_read(char *buf, size_t len);

#endif
