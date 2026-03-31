#include "elf.h"
#include "mem.h"
#include "page.h"

int run(struct inode file) {
    char* buf = (char*)malloc(file.size);
    if (!buf) {
        printf("Failed to allocate memory for ELF file\n");
        return -1;
    }

    int n = vfs_read(&file, (uint8_t*)buf, file.size);
    if (n < 0) {
        printf("Failed to read ELF file\n");
        free(buf);
        return -1;
    }

    // Validate ELF magic
    if (buf[0] != 0x7F || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        printf("Not a valid ELF file\n");
        free(buf);
        return -1;
    }

    if (buf[4] != 2) {
        printf("Only 64-bit ELF files are supported\n");
        free(buf);
        return -1;
    }

    if (buf[5] != 1) {
        printf("Only little-endian ELF files are supported\n");
        free(buf);
        return -1;
    }

    if (buf[16] != 2) {
        printf("Only executable ELF files are supported\n");
        free(buf);
        return -1;
    }

    if (buf[18] != 0x3E) {
        printf("Only x86-64 ELF files are supported\n");
        free(buf);
        return -1;
    }

    uint64_t entry_point = *(uint64_t*)(buf + 24);
    uint64_t program_header_offset = *(uint64_t*)(buf + 32);
    uint16_t program_header_entry_size = *(uint16_t*)(buf + 54);
    uint16_t program_header_entry_count = *(uint16_t*)(buf + 56);

    uint64_t *pml4 = get_kernel_pml4();

    for (int i = 0; i < program_header_entry_count; i++) {
        char* ph = buf + program_header_offset + i * program_header_entry_size;
        uint32_t type = *(uint32_t*)(ph);
        if (type != 1) // PT_LOAD
            continue;

        uint64_t offset = *(uint64_t*)(ph + 8);
        uint64_t vaddr  = *(uint64_t*)(ph + 16);
        uint64_t filesz = *(uint64_t*)(ph + 32);
        uint64_t memsz  = *(uint64_t*)(ph + 40);
        uint32_t flags  = *(uint32_t*)(ph + 4);

        printf("  Loading segment: vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
               vaddr, filesz, memsz);

        // Map pages for this segment
        uint64_t page_start = vaddr & ~(PAGE_SIZE - 1ULL);
        uint64_t page_end   = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1ULL);

        uint64_t pte_flags = PTE_WRITABLE;
        if (flags & 0x4) // PF_R — always readable if present
            pte_flags |= 0;
        // We don't set PTE_USER for now since we run ELFs in ring 0

        for (uint64_t va = page_start; va < page_end; va += PAGE_SIZE) {
            uint64_t phys = alloc_phys_page();
            if (!phys) {
                printf("Out of physical memory for ELF segment\n");
                free(buf);
                return -1;
            }
            map_page(pml4, va, phys, pte_flags);
        }

        // Copy file data into the mapped pages
        memcpy((void*)vaddr, buf + offset, filesz);
        // Zero the BSS portion
        if (memsz > filesz) {
            memset((void*)(vaddr + filesz), 0, memsz - filesz);
        }
    }

    printf("Jumping to ELF entry point 0x%lx\n", entry_point);

    // Jump to entry point (ring 0, same address space)
    void (*entry)(void) = (void (*)(void))entry_point;
    entry();

    return 0;
}
