#ifndef ATA_H
#define ATA_H

#include <stdint.h>

// Initialize ATA driver. Returns 0 on success, -1 if no drive found.
int ata_init(void);

// Read 'count' sectors starting at LBA into 'buf'.
// buf must be at least count * 512 bytes.
// Returns 0 on success, -1 on error.
int ata_read_sectors(uint32_t lba, uint8_t count, void *buf);

// Write 'count' sectors starting at LBA from 'buf'.
// Returns 0 on success, -1 on error.
int ata_write_sectors(uint32_t lba, uint8_t count, const void *buf);

#endif
