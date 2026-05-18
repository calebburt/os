#ifndef PAGE_H
#define PAGE_H

#include <stdint.h>
#include <limine.h>

#define PAGE_SIZE 4096

#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_NX       (1ULL << 63)

// Convert between physical and virtual (HHDM) addresses
extern uint64_t hhdm_offset;

static inline void *phys_to_virt(uint64_t paddr) {
    return (void *)(paddr + hhdm_offset);
}

static inline uint64_t virt_to_phys(void *vaddr) {
    return (uint64_t)vaddr - hhdm_offset;
}

// Initialize paging subsystem: sets up PMM, reads current CR3, registers page fault handler
void paging_init(struct limine_hhdm_response *hhdm, struct limine_memmap_response *memmap);

// Map a single 4K page: vaddr -> paddr with given flags
void map_page(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);

// Map a range of pages (both vaddr and paddr must be page-aligned, size in bytes)
void map_pages(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags);

// Get the kernel's PML4 (virtual address)
uint64_t *get_kernel_pml4(void);

// Create a new PML4 that shares the kernel-half (upper 256 entries) of the
// current kernel PML4, with an empty user-half (lower 256 entries).
// Returns the virtual address of the new PML4, or NULL on failure.
uint64_t *clone_pml4(void);

// Free all user-space page table levels and mapped physical pages
// (lower 256 PML4 entries). Does NOT free the PML4 page itself.
void free_user_pages(uint64_t *pml4);

// Switch CR3 (address space). Takes a physical address.
void switch_address_space(uint64_t cr3_phys);

// Get current CR3 value
uint64_t get_cr3(void);

#endif
