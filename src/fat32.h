#ifndef FAT32_H
#define FAT32_H

#include "vfs.h"
#include <stdint.h>

// Mount an existing FAT32 filesystem from the ATA drive.
// Returns a filesystem instance ready to vfs_mount(), or NULL on failure.
struct filesystem *fat32_mount_disk(void);

// Format the ATA drive as FAT32 and return a mounted filesystem instance.
// total_sectors: size of the disk in 512-byte sectors (0 = auto-detect 128MB).
// volume_label: up to 11 characters, or NULL for "NO NAME".
struct filesystem *fat32_format(uint32_t total_sectors, const char *volume_label);

#endif
