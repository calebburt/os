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
#include "elf.h"
#include "page.h"
#include "mem.h"
#include "flanterm/flanterm.h"
#include "flanterm/flanterm_backends/fb.h"

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

__attribute__((used, section(".limine_requests")))
static volatile struct limine_flanterm_fb_init_params_request flanterm_request = {
    .id = LIMINE_FLANTERM_FB_INIT_PARAMS_REQUEST_ID,
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

// Used by fb.c and stdio_core.c to route text output through flanterm.
struct flanterm_context *flanterm_ctx = NULL;

// flanterm wants a free() that takes (ptr, size); the kernel's free() takes
// only ptr. Wrap to match the expected signature.
static void flanterm_free(void *ptr, size_t size) {
    (void)size;
    free(ptr);
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

    // Initialize flanterm with the framebuffer geometry from Limine.
    // Pass NULL for the optional fields so flanterm uses its built-in
    // defaults (font, palette, white-on-black) — safer than trusting the
    // optional Limine response, which can be zeroed.
    flanterm_ctx = flanterm_fb_init(
        malloc, flanterm_free,
        framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch,
        framebuffer->red_mask_size, framebuffer->red_mask_shift,
        framebuffer->green_mask_size, framebuffer->green_mask_shift,
        framebuffer->blue_mask_size, framebuffer->blue_mask_shift,
        /* canvas              */ NULL,
        /* ansi_colours        */ NULL, /* ansi_bright_colours */ NULL,
        /* default_bg          */ NULL, /* default_fg          */ NULL,
        /* default_bg_bright   */ NULL, /* default_fg_bright   */ NULL,
        /* font                */ NULL,
        /* font_width/height/spacing */ 0, 0, 0,
        /* font_scale_x/y      */ 0, 0,
        /* margin              */ 0,
        /* rotation            */ FLANTERM_FB_ROTATE_0
    );
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
            printf("No FAT32 filesystem found, would you like to format? [yn] ");
            if (getchar() == 'y')
                fat = fat32_format(0, "MYOS");
            else
                return;
        }

        if (fat) {
            vfs_mount("/1", fat);
        }
    } else {
        printf("No ATA drive detected.\n");
    }
}

static void cpu_enable_sse(void) {
    uint64_t cr0, cr4;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);  // clear EM: no FPU emulation
    cr0 |=  (1ULL << 1);  // set MP: monitor coprocessor
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);   // OSFXSR: enable FXSAVE/FXRSTOR and SSE
    cr4 |= (1ULL << 10);  // OSXMMEXCPT: SSE exceptions → #XF not #UD
    asm volatile("mov %0, %%cr4" :: "r"(cr4));
}

// kernel entry point
void kmain(void) {
    cpu_enable_sse();
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

    char *path = "/1/sh";
    struct inode *file = vfs_open(path, O_RDONLY);
    if (file == NULL) {
        puts("Shell not found.");
        hcf();
    } else {
        clear_screen(0);
        printf("\n");
        run(*file, NULL, 0);
    }

    shutdown();
}