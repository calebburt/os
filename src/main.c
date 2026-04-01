#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <limine.h>

#include "fb.h"
#include "stdio.h"
#include "vfs.h"
#include "memfs.h"
#include "ata.h"
#include "fat32.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "isr.h"
#include "syscall.h"
#include "syscall_helpers.h"
#include "elf.h"
#include "page.h"

// Set the base revision to 5, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(5);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
        asm ("hlt");
    }
}

void init_fb(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Fetch the first framebuffer.
    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];

    // Initialize the framebuffer module
    fb_init(framebuffer);
}

void fs_init(void) {
    // Initialize ATA and FAT32
    printf("\nInitializing ATA driver...\n");
    if (ata_init() == 0) {
        printf("ATA drive detected.\n");

        // Try to mount an existing FAT32 filesystem
        struct filesystem *fat = fat32_mount_disk();
        if (fat) {
            printf("FAT32 filesystem found, mounting at /1\n");
        } else {
            printf("No FAT32 filesystem found, formatting...\n");
            fat = fat32_format(0, "MYOS");
        }

        if (fat) {
            vfs_mount("/1", fat);
        }
    } else {
        printf("No ATA drive detected.\n");
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void) {
    init_fb();
    clear_screen(0);

    // Can't use printf until we initialize stdio, so we'll just print directly to the framebuffer for now
    print_string("Initializing stdio...\n", 0xFFFFFF);

    stdio_init();

    puts("Initializing paging...");

    // Initialize paging subsystem (needs HHDM + memmap from Limine)
    if (!hhdm_request.response || !memmap_request.response) {
        print_string("FATAL: Limine HHDM or memmap response missing\n", 0xFF0000);
        hcf();
    }
    paging_init(hhdm_request.response, memmap_request.response);

    // Initialize interrupt infrastructure
    printf("Initializing GDT, IDT, PIC, and syscalls...\n");
    gdt_init();
    idt_init();
    pic_init();
    syscall_init();

    // Unmask keyboard IRQ (IRQ1) and timer (IRQ0)
    pic_clear_mask(0);
    pic_clear_mask(1);

    // Enable interrupts
    asm volatile ("sti");

    puts("Interrupts enabled!");

    puts("Mounting filesystem...");
    fs_init();

    puts("");

    // // Create and write a test file
    // char buffer[32] = "";
    // printf("Type a filename to open: ");
    // fgets(buffer, sizeof(buffer), stdin);
    // // sys_read(buffer, sizeof(buffer) - 1);
    // // buffer[sizeof(buffer) - 1] = '\0';  // Ensure null-termination

    // struct inode *file = vfs_open(buffer, O_CREAT | O_WRONLY);
    // if (file->size == 0) {
    //     const char msg[50] = "";
    //     printf("Type a message to write to the file '%s': ", buffer);
    //     fgets((char *)msg, sizeof(msg), stdin);
    //     vfs_write(file, (const uint8_t *)msg, strlen(msg));
    //     vfs_close(file);
    //     puts("Wrote to file");
    // } else {
    //     puts("File already exists!");
    // }

    // // Read it back
    // file = vfs_open(buffer, O_RDONLY);
    // if (file) {
    //     uint8_t buf[128];
    //     int n = vfs_read(file, buf, sizeof(buf) - 1);
    //     if (n > 0) {
    //         buf[n] = '\0';
    //         printf("Read back: %s\n", (char *)buf);
    //     }
    //     vfs_close(file);
    // }

    char buffer_2[32] = "";
    printf("Type a filename to execute: ");
    fgets(buffer_2, sizeof(buffer_2), stdin);
    run(*vfs_open(buffer_2, O_RDONLY));

    printf("\nEnter characters for fun: ");
    char c = ' ';
    while (c != '\n') {
        c = getchar();
    }

    puts("Press any key to exit:");
    getchar();

    sys_exit();
}