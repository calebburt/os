#ifndef MEM_H
#define MEM_H

#include <stddef.h>
#include <stdint.h>
#include <limine.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);

void *malloc(size_t size);
void free(void *ptr);

// Physical page allocator (uses Limine memory map)
void pmm_init(struct limine_memmap_response *memmap);
uint64_t alloc_phys_page(void);
void free_phys_page(uint64_t paddr);

#endif