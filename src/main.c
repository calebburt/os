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
            printf("FAT32 filesystem found, mounting at /disk\n");
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
    stdio_init();
    puts("Mounting filesystem...");
    fs_init();

    clear_screen(0x000000);

    puts("Hello world!");
    puts("Serial output enabled!");
    printf("Testing printf: %d\n", 42);

    // Create and write a test file
    struct inode *file = vfs_open("/1/hello.txt", O_CREAT | O_WRONLY);
    if (file) {
        const char *msg = "Hello from FAT32!";
        vfs_write(file, (const uint8_t *)msg, strlen(msg));
        vfs_close(file);
        puts("Wrote /1/hello.txt");
    }

    // Read it back
    file = vfs_open("/1/hello.txt", O_RDONLY);
    if (file) {
        uint8_t buf[128];
        int n = vfs_read(file, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("Read back: %s\n", (char *)buf);
        }
        vfs_close(file);
    }

    printf("\nEnter characters (echo mode): ");
    char c = ' ';
    while (c != '\n') {
        c = getchar();
        putchar(c);
    }

    puts("Press any key to exit:");
    getchar();

    shutdown();

    // // We're done, just hang...
    // hcf();
}