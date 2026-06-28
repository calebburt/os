#include "stdlib.h"
#include "string.h"
#include "time.h"

// Simple first-fit heap with forward coalescing.
// The static heap lives in BSS — zero-initialised, no cost in the ELF file.
#define HEAP_SIZE (2 * 1024 * 1024)  // 2 MB

typedef struct block {
    size_t        size;   // usable bytes (excluding this header)
    int           free;   // 1 = available
    struct block *next;   // next block in the list (NULL = end)
} block_t;

static unsigned char heap[HEAP_SIZE];
static block_t      *head = NULL;  // first block (NULL until first malloc)

static void heap_init(void) {
    head = (block_t *)heap;
    head->size = HEAP_SIZE - sizeof(block_t);
    head->free = 1;
    head->next = NULL;
}

// Merge adjacent free blocks starting at b.
static void coalesce(block_t *b) {
    while (b->next && b->next->free) {
        b->size += sizeof(block_t) + b->next->size;
        b->next  = b->next->next;
    }
}

void *malloc(size_t size) {
    if (size == 0) return NULL;

    // align to 8 bytes
    size = (size + 7) & ~(size_t)7;

    if (!head) heap_init();

    for (block_t *b = head; b; b = b->next) {
        if (!b->free || b->size < size) continue;

        // Split if there's enough room for a new block header + at least 8 bytes
        if (b->size >= size + sizeof(block_t) + 8) {
            block_t *split = (block_t *)((unsigned char *)(b + 1) + size);
            split->size = b->size - size - sizeof(block_t);
            split->free = 1;
            split->next = b->next;
            b->next = split;
            b->size = size;
        }

        b->free = 0;
        return (void *)(b + 1);
    }

    return NULL;  // out of memory
}

void free(void *ptr) {
    if (!ptr) return;
    block_t *b = (block_t *)ptr - 1;
    b->free = 1;
    coalesce(b);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr)   return malloc(size);
    if (!size)  { free(ptr); return NULL; }

    block_t *b = (block_t *)ptr - 1;

    // Try to expand in place by coalescing with the next block first
    coalesce(b);
    if (b->size >= ((size + 7) & ~(size_t)7)) return ptr;

    void *new_ptr = malloc(size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, b->size < size ? b->size : size);
    free(ptr);
    return new_ptr;
}

double strtod(const char *s, char **endptr) {
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;

    double result = 0.0;
    int neg = 0;
    if (*s == '-')      { neg = 1; s++; }
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9')
        result = result * 10.0 + (*s++ - '0');

    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') {
            result += (*s++ - '0') * frac;
            frac *= 0.1;
        }
    }

    if (*s == 'e' || *s == 'E') {
        s++;
        int eneg = 0;
        if (*s == '-')      { eneg = 1; s++; }
        else if (*s == '+') { s++; }
        int e = 0;
        while (*s >= '0' && *s <= '9') e = e * 10 + (*s++ - '0');
        double p = 1.0;
        for (int i = 0; i < e; i++) p *= 10.0;
        if (eneg) result /= p; else result *= p;
    }

    if (endptr) *endptr = (char *)s;
    return neg ? -result : result;
}

clock_t clock(void) {
    return 0;  // no time syscall yet
}
