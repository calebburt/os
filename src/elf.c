#include "elf.h"
#include <stdlib.h>

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

    // Read Header
    if (buf[0] != 0x7F || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') {
        printf("Not a valid ELF file\n");
        free(buf);
        return -1;
    }

    if (buf[4] != 2) {
        printf("Only 64-bit ELF files are supported\n");
        if (buf[4] == 3) {
            printf("This is a 32-bit ELF file, but only 64-bit is supported\n");
        } else {
            printf("Unknown ELF class: %d\n", buf[4]);
        }
        printf("\n");
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

    if (buf[17] != 0x3E) {
        printf("Only x86-64 ELF files are supported\n");
        free(buf);
        return -1;
    }

    int program_header_offset = *(uint64_t*)(buf + 32);
    int program_header_entry_size = *(uint16_t*)(buf + 54);
    int program_header_entry_count = *(uint16_t*)(buf + 56);

    for (int i = 0; i < program_header_entry_count; i++) {
        char* ph = buf + program_header_offset + i * program_header_entry_size;
        int type = *(uint32_t*)(ph);
        if (type != 1) { // PT_LOAD
            continue;
        }

        uint64_t offset = *(uint64_t*)(ph + 8);
        uint64_t vaddr = *(uint64_t*)(ph + 16);
        uint64_t filesz = *(uint64_t*)(ph + 32);
        uint64_t memsz = *(uint64_t*)(ph + 40);

        // For simplicity, we assume the ELF file is linked to run at its virtual address
        memcpy((void*)vaddr, buf + offset, filesz);
        if (memsz > filesz) {
            memset((void*)(vaddr + filesz), 0, memsz - filesz);
        }
    }
}