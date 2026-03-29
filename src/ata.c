#include "ata.h"
#include "io.h"

// Primary ATA bus I/O ports
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECT_COUNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_CMD_STATUS  0x1F7

// Commands
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH   0xE7

// Status bits
#define ATA_STATUS_BSY  0x80
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_ERR  0x01

static int ata_detected = 0;

// Wait for BSY to clear. Returns status byte.
static uint8_t ata_wait(void) {
    uint8_t status;
    // 400ns delay: read status port 4 times
    for (int i = 0; i < 4; i++) {
        status = inb(ATA_CMD_STATUS);
    }
    while ((status = inb(ATA_CMD_STATUS)) & ATA_STATUS_BSY)
        ;
    return status;
}

// Wait for DRQ or ERR
static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(ATA_CMD_STATUS);
        if (status & ATA_STATUS_ERR) return -1;
        if (status & ATA_STATUS_DRQ) return 0;
    }
    return -1; // timeout
}

int ata_init(void) {
    // Select drive 0 (master) on primary bus
    outb(ATA_DRIVE_HEAD, 0xA0);
    io_wait();

    // Zero out sector count and LBA registers
    outb(ATA_SECT_COUNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_CMD_STATUS, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_CMD_STATUS);
    if (status == 0) {
        // No drive present
        return -1;
    }

    // Wait for BSY to clear
    ata_wait();

    // Check if it's ATA (not ATAPI) - LBA mid/hi should remain 0
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) {
        return -1; // Not ATA
    }

    // Wait for DRQ
    if (ata_wait_drq() < 0) {
        return -1;
    }

    // Read and discard 256 words of identify data
    for (int i = 0; i < 256; i++) {
        inw(ATA_DATA);
    }

    ata_detected = 1;
    return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t count, void *buf) {
    if (!ata_detected || count == 0) return -1;

    uint16_t *wbuf = (uint16_t *)buf;

    ata_wait();

    // LBA28 mode, drive 0
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECT_COUNT, count);
    outb(ATA_LBA_LO, (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));

    // Send read command
    outb(ATA_CMD_STATUS, ATA_CMD_READ);

    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;

        // Read 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            wbuf[s * 256 + i] = inw(ATA_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t count, const void *buf) {
    if (!ata_detected || count == 0) return -1;

    const uint16_t *wbuf = (const uint16_t *)buf;

    ata_wait();

    // LBA28 mode, drive 0
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECT_COUNT, count);
    outb(ATA_LBA_LO, (uint8_t)(lba));
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));

    // Send write command
    outb(ATA_CMD_STATUS, ATA_CMD_WRITE);

    for (int s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;

        // Write 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            outw(ATA_DATA, wbuf[s * 256 + i]);
        }

        // Flush after each sector
        outb(ATA_CMD_STATUS, ATA_CMD_FLUSH);
        ata_wait();
    }

    return 0;
}
