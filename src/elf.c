#include "elf.h"
#include "mem.h"
#include "page.h"

int run(struct inode file, char *argv[], int argn) {
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

    // Validate ELF magic number
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

    // Create a new address space for this process
    uint64_t *new_pml4 = clone_pml4();
    if (!new_pml4) {
        printf("Failed to allocate page table for process\n");
        free(buf);
        return -1;
    }

    // Save the caller's CR3 and switch to the new address space
    uint64_t old_cr3 = get_cr3();
    uint64_t new_cr3 = virt_to_phys(new_pml4);
    switch_address_space(new_cr3);

    uint64_t *pml4 = new_pml4;

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

        // printf("  Loading segment: vaddr=0x%lx filesz=0x%lx memsz=0x%lx\n",
        //        vaddr, filesz, memsz);

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

    // Dedicated stack for the user process (2 MB in BSS — zero file-size cost).
    // Programs like interpreters allocate hundreds of KB on the C stack
    // (e.g. a 256 KB VM struct), which would overflow the kernel stack if we
    // called entry() directly.
#define USER_STACK_SIZE (2 * 1024 * 1024)
    static uint8_t user_stack[USER_STACK_SIZE];
    uint64_t user_stack_top = ((uint64_t)(user_stack + USER_STACK_SIZE)) & ~15ULL;

    // Determine where the child process's stack starts.
    //
    // If we're already executing on user_stack (i.e. this run() was reached via
    // a syscall from a user process), the kernel call chain occupies the upper
    // part of user_stack.  Resetting RSP to user_stack_top would let the child
    // grow back into those kernel frames and corrupt them (e.g. overwriting
    // old_cr3 with stack data → GPF on cleanup).  Instead, start just below the
    // current RSP so the child grows away from the kernel frames.
    //
    // If we're not on user_stack (first process, called from the kernel's own
    // stack), switch to user_stack_top as before.
    uint64_t cur_rsp;
    asm volatile("mov %%rsp, %0" : "=r"(cur_rsp));
    uint64_t new_rsp;
    if (cur_rsp >= (uint64_t)user_stack && cur_rsp < user_stack_top)
        new_rsp = cur_rsp & ~15ULL;   // nested: stay at current depth
    else
        new_rsp = user_stack_top;     // first process: use full stack

    // Switch rsp to new_rsp, call the entry point, then restore.
    // We save the kernel rsp in a static (RIP-relative address, always valid).
    // Register layout: "a"=rax (entry), "b"=rbx (new rsp), "D"=rdi (argv),
    //                  "S"=rsi (argn).  `call` makes callee see rsp = 16n-8.
    static uint64_t _saved_kernel_rsp;
    printf("[elf] calling entry=0x%lx argc=%d stack=0x%lx\n",
           entry_point, argn, new_rsp);
    asm volatile(
        "mov %%rsp, %[save]\n\t"   /* stash kernel rsp (rip-relative write) */
        "mov %%rbx, %%rsp\n\t"    /* switch to chosen stack position        */
        "call *%%rax\n\t"          /* call _start(argv, argn)                */
        "mov %[save], %%rsp\n\t"  /* restore kernel rsp                     */
        : [save] "+m"(_saved_kernel_rsp)
        : "a"(entry_point), "b"(new_rsp), "D"(argv), "S"((long)argn)
        : "memory", "rcx", "rdx", "r8", "r9", "r10", "r11"
    );

    // Restore the caller's address space, free the process's pages and ELF buffer.
    switch_address_space(old_cr3);
    free_user_pages(new_pml4);
    free_phys_page(new_cr3);
    free(buf);
    return 0;
}
