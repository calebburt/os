#include <stddef.h>
#include <stdint.h>
#include <limine.h>

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if ((uintptr_t)src > (uintptr_t)dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if ((uintptr_t)src < (uintptr_t)dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

// String functions
size_t strlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) {
        n++;
    }
    return n;
}

int strcmp(const char *s1, const char *s2) {
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1) s1 = "";
    if (!s2) s2 = "";
    
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        }
        if (!s1[i]) break;  // End of strings
    }
    return 0;
}

char *strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src) return dest;
    
    for (size_t i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    if (n > 0) {
        dest[n - 1] = '\0';
    }
    return dest;
}

// Simple heap allocator (bump allocator)
#define HEAP_SIZE 262144
static uint8_t heap[HEAP_SIZE];
static size_t heap_offset = 0;

void *malloc(size_t size) {
    if (heap_offset + size > HEAP_SIZE) {
        return NULL;  // Out of memory
    }
    
    void *ptr = &heap[heap_offset];
    heap_offset += size;
    
    // Align to 8 bytes
    heap_offset = (heap_offset + 7) & ~7;
    
    return ptr;
}

// Physical memory manager — simple freelist built from Limine memory map
// We need HHDM offset to access physical memory from higher-half kernel
extern uint64_t hhdm_offset;

#define PAGE_SIZE 4096

// Freelist node stored at the start of each free physical page
struct free_page {
    struct free_page *next;
};

static struct free_page *free_list = NULL;
static uint64_t total_free_pages = 0;

void pmm_init(struct limine_memmap_response *memmap) {
    free_list = NULL;
    total_free_pages = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type != LIMINE_MEMMAP_USABLE) continue;

        uint64_t base = (entry->base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        uint64_t end  = (entry->base + entry->length) & ~(PAGE_SIZE - 1);

        for (uint64_t addr = base; addr < end; addr += PAGE_SIZE) {
            // Skip the first 1MB (legacy BIOS area)
            if (addr < 0x100000) continue;

            struct free_page *page = (struct free_page *)(addr + hhdm_offset);
            page->next = free_list;
            free_list = page;
            total_free_pages++;
        }
    }
}

uint64_t alloc_phys_page(void) {
    if (!free_list) return 0;

    struct free_page *page = free_list;
    free_list = page->next;
    total_free_pages--;

    // Zero the page
    uint64_t phys = (uint64_t)page - hhdm_offset;
    memset(page, 0, PAGE_SIZE);
    return phys;
}

void free_phys_page(uint64_t paddr) {
    struct free_page *page = (struct free_page *)(paddr + hhdm_offset);
    page->next = free_list;
    free_list = page;
    total_free_pages++;
}

void free(void *ptr) {
    // Bump allocator doesn't support freeing
    // This is a no-op, but we keep it for compatibility
    (void)ptr;
}