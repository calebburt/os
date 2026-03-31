#include <stdint.h>
#include <string.h>
#include "mem.h"
#include "page.h"
#include "isr.h"
#include "stdio.h"

uint64_t hhdm_offset = 0;

static uint64_t *kernel_pml4 = NULL;

// Read current CR3 value
static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

// Write CR3 to flush TLB
static inline void write_cr3(uint64_t cr3) {
    asm volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

// Invalidate a single TLB entry
static inline void invlpg(uint64_t vaddr) {
    asm volatile ("invlpg (%0)" :: "r"(vaddr) : "memory");
}

// Read CR2 (faulting address)
static inline uint64_t read_cr2(void) {
    uint64_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

// Get or create a page table entry, returning a virtual pointer to the next-level table
static uint64_t *get_or_create_table(uint64_t *table, int index) {
    if (!(table[index] & PTE_PRESENT)) {
        uint64_t phys = alloc_phys_page();
        if (!phys) return NULL;
        // Page is already zeroed by alloc_phys_page
        table[index] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    // Extract physical address from entry and convert to virtual
    uint64_t phys = table[index] & 0x000FFFFFFFFFF000ULL;
    return (uint64_t *)phys_to_virt(phys);
}

void map_page(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    int pml4_i = (vaddr >> 39) & 0x1FF;
    int pdpt_i = (vaddr >> 30) & 0x1FF;
    int pd_i   = (vaddr >> 21) & 0x1FF;
    int pt_i   = (vaddr >> 12) & 0x1FF;

    uint64_t *pdpt = get_or_create_table(pml4, pml4_i);
    if (!pdpt) return;
    uint64_t *pd = get_or_create_table(pdpt, pdpt_i);
    if (!pd) return;
    uint64_t *pt = get_or_create_table(pd, pd_i);
    if (!pt) return;

    pt[pt_i] = (paddr & 0x000FFFFFFFFFF000ULL) | flags | PTE_PRESENT;
    invlpg(vaddr);
}

void map_pages(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    uint64_t end = vaddr + size;
    while (vaddr < end) {
        map_page(pml4, vaddr, paddr, flags);
        vaddr += PAGE_SIZE;
        paddr += PAGE_SIZE;
    }
}

uint64_t *get_kernel_pml4(void) {
    return kernel_pml4;
}

// Page fault handler (vector 14)
static void page_fault_handler(struct interrupt_frame *frame) {
    uint64_t fault_addr = read_cr2();
    uint64_t error = frame->error_code;

    printf("\n!!! PAGE FAULT at vaddr=%d\n", fault_addr);
    printf("    Error: %s %s %s\n",
           (error & 1) ? "protection-violation" : "not-present",
           (error & 2) ? "write" : "read",
           (error & 4) ? "user-mode" : "kernel-mode");
    printf("    RIP=0x%lx\n", frame->rip);

    // Halt — in a more advanced kernel you'd kill the faulting process
    for (;;) asm volatile("hlt");
}

void paging_init(struct limine_hhdm_response *hhdm, struct limine_memmap_response *memmap) {
    // Store the HHDM offset (used everywhere for phys<->virt)
    hhdm_offset = hhdm->offset;

    // Initialize the physical memory manager
    pmm_init(memmap);

    // Read the current PML4 that Limine set up for us
    uint64_t cr3 = read_cr3();
    kernel_pml4 = (uint64_t *)phys_to_virt(cr3 & 0x000FFFFFFFFFF000ULL);

    // Register page fault handler
    isr_register_handler(14, page_fault_handler);
}
