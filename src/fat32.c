#include "fat32.h"
#include "ata.h"
#include "stdio.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ── FAT32 on-disk structures ────────────────────────────────────────────────

struct __attribute__((packed)) fat32_bpb {
    uint8_t  jmp[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;    // 0 for FAT32
    uint16_t total_sectors_16;    // 0 for FAT32
    uint8_t  media_type;
    uint16_t fat_size_16;         // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32-specific
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_serial;
    char     volume_label[11];
    char     fs_type[8];
};

struct __attribute__((packed)) fat32_dir_entry {
    char     name[11];            // 8.3 format
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t first_cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
};

// Directory entry attributes
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// FAT special values
#define FAT32_EOC       0x0FFFFFF8  // End of cluster chain (>= this value)
#define FAT32_FREE      0x00000000
#define FAT32_BAD       0x0FFFFFF7

// ── Internal state ──────────────────────────────────────────────────────────

struct fat32_private {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size;            // sectors per FAT
    uint32_t root_cluster;
    uint32_t total_sectors;
    uint32_t data_start_sector;   // first sector of data region
    uint32_t fat_start_sector;    // first sector of FAT
    uint32_t cluster_count;
};

// Per-open-file data
struct fat32_file_data {
    uint32_t first_cluster;
    uint32_t file_size;
    uint32_t dir_cluster;         // cluster of parent directory
    uint32_t dir_offset;          // byte offset of dir entry in parent
    uint8_t  is_dir;
};

// Scratch buffer for sector I/O (avoids heap allocation)
static uint8_t sector_buf[512];

// ── Helpers ─────────────────────────────────────────────────────────────────

static uint32_t cluster_to_lba(struct fat32_private *priv, uint32_t cluster) {
    return priv->data_start_sector + (cluster - 2) * priv->sectors_per_cluster;
}

// Read one FAT entry
static uint32_t fat_read_entry(struct fat32_private *priv, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = priv->fat_start_sector + (fat_offset / 512);
    uint32_t entry_offset = fat_offset % 512;

    if (ata_read_sectors(fat_sector, 1, sector_buf) < 0)
        return FAT32_BAD;

    uint32_t val;
    memcpy(&val, sector_buf + entry_offset, 4);
    return val & 0x0FFFFFFF;
}

// Write one FAT entry (updates all FAT copies)
static int fat_write_entry(struct fat32_private *priv, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector_in_fat = fat_offset / 512;
    uint32_t entry_offset = fat_offset % 512;

    for (uint8_t f = 0; f < priv->num_fats; f++) {
        uint32_t sector = priv->fat_start_sector + f * priv->fat_size + sector_in_fat;

        if (ata_read_sectors(sector, 1, sector_buf) < 0) return -1;

        // Preserve top 4 bits
        uint32_t existing;
        memcpy(&existing, sector_buf + entry_offset, 4);
        value = (existing & 0xF0000000) | (value & 0x0FFFFFFF);
        memcpy(sector_buf + entry_offset, &value, 4);

        if (ata_write_sectors(sector, 1, sector_buf) < 0) return -1;
    }
    return 0;
}

// Allocate a free cluster, mark it as EOC, zero it out. Returns cluster number or 0.
static uint32_t fat_alloc_cluster(struct fat32_private *priv) {
    // Linear scan for a free cluster
    for (uint32_t c = 2; c < priv->cluster_count + 2; c++) {
        if (fat_read_entry(priv, c) == FAT32_FREE) {
            // Mark as end of chain
            if (fat_write_entry(priv, c, 0x0FFFFFFF) < 0) return 0;

            // Zero the cluster
            memset(sector_buf, 0, 512);
            uint32_t lba = cluster_to_lba(priv, c);
            for (uint8_t s = 0; s < priv->sectors_per_cluster; s++) {
                if (ata_write_sectors(lba + s, 1, sector_buf) < 0) return 0;
            }
            return c;
        }
    }
    return 0; // disk full
}

// Convert a filename like "hello.txt" to FAT 8.3 name "HELLO   TXT"
static void to_fat_name(const char *path, char *fat_name) {
    memset(fat_name, ' ', 11);

    // Skip leading slashes
    while (*path == '/') path++;

    int i = 0;
    // Copy name part (up to 8 chars before dot)
    while (*path && *path != '.' && *path != '/' && i < 8) {
        char c = *path++;
        if (c >= 'a' && c <= 'z') c -= 32; // toupper
        fat_name[i++] = c;
    }
    // Skip to dot
    while (*path && *path != '.' && *path != '/') path++;
    // Copy extension
    if (*path == '.') {
        path++;
        int j = 8;
        while (*path && *path != '/' && j < 11) {
            char c = *path++;
            if (c >= 'a' && c <= 'z') c -= 32;
            fat_name[j++] = c;
        }
    }
}

// ── Read a cluster chain into a callback ────────────────────────────────────

// Find a directory entry by 8.3 name within a directory starting at dir_cluster.
// On success: fills out_entry and sets *out_cluster/*out_offset to location of the entry.
static int fat32_find_in_dir(struct fat32_private *priv, uint32_t dir_cluster,
                             const char *fat_name, struct fat32_dir_entry *out_entry,
                             uint32_t *out_cluster, uint32_t *out_offset)
{
    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < FAT32_EOC) {
        uint32_t lba = cluster_to_lba(priv, cluster);
        for (uint8_t s = 0; s < priv->sectors_per_cluster; s++) {
            if (ata_read_sectors(lba + s, 1, sector_buf) < 0) return -1;

            for (int e = 0; e < 512 / 32; e++) {
                struct fat32_dir_entry *de = (struct fat32_dir_entry *)(sector_buf + e * 32);
                if (de->name[0] == 0x00) return -1;  // end of directory
                if ((uint8_t)de->name[0] == 0xE5) continue;  // deleted
                if (de->attr == ATTR_LONG_NAME) continue;

                if (memcmp(de->name, fat_name, 11) == 0) {
                    if (out_entry) *out_entry = *de;
                    if (out_cluster) *out_cluster = cluster;
                    if (out_offset) *out_offset = (uint32_t)(s * 512 + e * 32);
                    return 0;
                }
            }
        }
        cluster = fat_read_entry(priv, cluster);
    }
    return -1; // not found
}

// Find a free slot in a directory. Returns 0 on success.
static int fat32_find_free_dir_slot(struct fat32_private *priv, uint32_t dir_cluster,
                                    uint32_t *out_cluster, uint32_t *out_offset)
{
    uint32_t cluster = dir_cluster;
    uint32_t prev_cluster = 0;
    while (cluster >= 2 && cluster < FAT32_EOC) {
        uint32_t lba = cluster_to_lba(priv, cluster);
        for (uint8_t s = 0; s < priv->sectors_per_cluster; s++) {
            if (ata_read_sectors(lba + s, 1, sector_buf) < 0) return -1;

            for (int e = 0; e < 512 / 32; e++) {
                struct fat32_dir_entry *de = (struct fat32_dir_entry *)(sector_buf + e * 32);
                if (de->name[0] == 0x00 || (uint8_t)de->name[0] == 0xE5) {
                    *out_cluster = cluster;
                    *out_offset = (uint32_t)(s * 512 + e * 32);
                    return 0;
                }
            }
        }
        prev_cluster = cluster;
        cluster = fat_read_entry(priv, cluster);
    }

    // Directory is full — extend it by one cluster
    uint32_t new_cluster = fat_alloc_cluster(priv);
    if (new_cluster == 0) return -1;
    if (fat_write_entry(priv, prev_cluster, new_cluster) < 0) return -1;

    *out_cluster = new_cluster;
    *out_offset = 0;
    return 0;
}

// Write a dir entry at a specific cluster + byte offset within that cluster
static int fat32_write_dir_entry(struct fat32_private *priv, uint32_t cluster,
                                 uint32_t offset, const struct fat32_dir_entry *entry)
{
    uint32_t sector_in_cluster = offset / 512;
    uint32_t offset_in_sector = offset % 512;
    uint32_t lba = cluster_to_lba(priv, cluster) + sector_in_cluster;

    if (ata_read_sectors(lba, 1, sector_buf) < 0) return -1;
    memcpy(sector_buf + offset_in_sector, entry, 32);
    if (ata_write_sectors(lba, 1, sector_buf) < 0) return -1;
    return 0;
}

// ── Path resolution ─────────────────────────────────────────────────────────

// Walk a path like "subdir/file.txt" from root, resolving each component.
// Returns 0 on success and fills *out_entry.
// If the last component is not found but create==1, *parent_cluster is set for creation.
static int fat32_resolve_path(struct fat32_private *priv, const char *path,
                              struct fat32_dir_entry *out_entry,
                              uint32_t *out_dir_cluster, uint32_t *out_dir_offset,
                              int create, uint32_t *parent_cluster)
{
    // Skip leading slashes
    while (*path == '/') path++;
    if (*path == '\0') {
        // Root directory itself
        if (out_entry) {
            memset(out_entry, 0, sizeof(*out_entry));
            out_entry->attr = ATTR_DIRECTORY;
            out_entry->first_cluster_hi = (uint16_t)(priv->root_cluster >> 16);
            out_entry->first_cluster_lo = (uint16_t)(priv->root_cluster);
        }
        if (out_dir_cluster) *out_dir_cluster = priv->root_cluster;
        return 0;
    }

    uint32_t current_cluster = priv->root_cluster;
    char component[13];

    while (*path) {
        // Extract next path component
        int len = 0;
        while (path[len] && path[len] != '/') len++;

        if (len > 12) return -1; // name too long
        memcpy(component, path, len);
        component[len] = '\0';

        path += len;
        while (*path == '/') path++;

        char fat_name[11];
        to_fat_name(component, fat_name);

        struct fat32_dir_entry entry;
        uint32_t found_cluster, found_offset;
        int found = fat32_find_in_dir(priv, current_cluster, fat_name,
                                      &entry, &found_cluster, &found_offset);

        if (found < 0) {
            // Not found
            if (*path == '\0' && create) {
                // Last component — caller wants to create it
                if (parent_cluster) *parent_cluster = current_cluster;
                return -1; // signal "not found but parent is valid"
            }
            return -1;
        }

        if (*path == '\0') {
            // This is the final component
            if (out_entry) *out_entry = entry;
            if (out_dir_cluster) *out_dir_cluster = found_cluster;
            if (out_dir_offset) *out_dir_offset = found_offset;
            return 0;
        }

        // Must be a directory to continue traversal
        if (!(entry.attr & ATTR_DIRECTORY)) return -1;
        current_cluster = ((uint32_t)entry.first_cluster_hi << 16) | entry.first_cluster_lo;
    }

    return -1;
}

// ── VFS inode operations ────────────────────────────────────────────────────

static int fat32_inode_read(struct inode *ino, uint8_t *buf, size_t offset, size_t len) {
    if (!ino || !buf || !ino->fs || !ino->fs_data) return -1;

    struct fat32_private *priv = (struct fat32_private *)ino->fs->fs_data;
    struct fat32_file_data *fd = (struct fat32_file_data *)ino->fs_data;

    if (!fd->is_dir && offset >= fd->file_size) return 0;

    size_t to_read = len;
    if (!fd->is_dir && offset + to_read > fd->file_size)
        to_read = fd->file_size - offset;

    uint32_t bytes_per_cluster = (uint32_t)priv->sectors_per_cluster * 512;
    uint32_t cluster = fd->first_cluster;

    // Skip clusters to reach offset
    uint32_t skip_clusters = (uint32_t)(offset / bytes_per_cluster);
    for (uint32_t i = 0; i < skip_clusters && cluster >= 2 && cluster < FAT32_EOC; i++) {
        cluster = fat_read_entry(priv, cluster);
    }

    size_t cluster_offset = offset % bytes_per_cluster;
    size_t bytes_read = 0;

    while (bytes_read < to_read && cluster >= 2 && cluster < FAT32_EOC) {
        uint32_t lba = cluster_to_lba(priv, cluster);

        // Which sector within the cluster?
        uint32_t sec_in_cluster = (uint32_t)(cluster_offset / 512);
        uint32_t off_in_sector = (uint32_t)(cluster_offset % 512);

        while (sec_in_cluster < priv->sectors_per_cluster && bytes_read < to_read) {
            if (ata_read_sectors(lba + sec_in_cluster, 1, sector_buf) < 0) return -1;

            size_t chunk = 512 - off_in_sector;
            if (chunk > to_read - bytes_read) chunk = to_read - bytes_read;
            memcpy(buf + bytes_read, sector_buf + off_in_sector, chunk);
            bytes_read += chunk;

            off_in_sector = 0;
            sec_in_cluster++;
        }

        cluster_offset = 0;
        cluster = fat_read_entry(priv, cluster);
    }

    return (int)bytes_read;
}

static int fat32_inode_write(struct inode *ino, const uint8_t *buf, size_t offset, size_t len) {
    if (!ino || !buf || !ino->fs || !ino->fs_data) return -1;

    struct fat32_private *priv = (struct fat32_private *)ino->fs->fs_data;
    struct fat32_file_data *fd = (struct fat32_file_data *)ino->fs_data;

    uint32_t bytes_per_cluster = (uint32_t)priv->sectors_per_cluster * 512;

    // Ensure cluster chain is long enough
    uint32_t needed_bytes = (uint32_t)(offset + len);
    uint32_t needed_clusters = (needed_bytes + bytes_per_cluster - 1) / bytes_per_cluster;
    if (needed_clusters == 0) needed_clusters = 1;

    // Allocate first cluster if file is empty
    if (fd->first_cluster < 2) {
        uint32_t c = fat_alloc_cluster(priv);
        if (c == 0) return -1;
        fd->first_cluster = c;
    }

    // Extend chain as needed
    uint32_t cluster = fd->first_cluster;
    uint32_t chain_len = 1;
    while (chain_len < needed_clusters) {
        uint32_t next = fat_read_entry(priv, cluster);
        if (next >= 2 && next < FAT32_EOC) {
            cluster = next;
            chain_len++;
        } else {
            uint32_t nc = fat_alloc_cluster(priv);
            if (nc == 0) return -1;
            fat_write_entry(priv, cluster, nc);
            cluster = nc;
            chain_len++;
        }
    }

    // Now write data
    cluster = fd->first_cluster;
    uint32_t skip_clusters = (uint32_t)(offset / bytes_per_cluster);
    for (uint32_t i = 0; i < skip_clusters; i++) {
        cluster = fat_read_entry(priv, cluster);
    }

    size_t cluster_offset = offset % bytes_per_cluster;
    size_t written = 0;

    while (written < len && cluster >= 2 && cluster < FAT32_EOC) {
        uint32_t lba = cluster_to_lba(priv, cluster);
        uint32_t sec = (uint32_t)(cluster_offset / 512);
        uint32_t off = (uint32_t)(cluster_offset % 512);

        while (sec < priv->sectors_per_cluster && written < len) {
            if (ata_read_sectors(lba + sec, 1, sector_buf) < 0) return -1;

            size_t chunk = 512 - off;
            if (chunk > len - written) chunk = len - written;
            memcpy(sector_buf + off, buf + written, chunk);

            if (ata_write_sectors(lba + sec, 1, sector_buf) < 0) return -1;
            written += chunk;
            off = 0;
            sec++;
        }

        cluster_offset = 0;
        cluster = fat_read_entry(priv, cluster);
    }

    // Update file size
    if (offset + written > fd->file_size) {
        fd->file_size = (uint32_t)(offset + written);
        ino->size = fd->file_size;

        // Update directory entry on disk
        struct fat32_dir_entry de;
        uint32_t de_sector_in_cluster = fd->dir_offset / 512;
        uint32_t de_offset_in_sector = fd->dir_offset % 512;
        uint32_t de_lba = cluster_to_lba(priv, fd->dir_cluster) + de_sector_in_cluster;

        if (ata_read_sectors(de_lba, 1, sector_buf) == 0) {
            memcpy(&de, sector_buf + de_offset_in_sector, 32);
            de.file_size = fd->file_size;
            de.first_cluster_hi = (uint16_t)(fd->first_cluster >> 16);
            de.first_cluster_lo = (uint16_t)(fd->first_cluster);
            memcpy(sector_buf + de_offset_in_sector, &de, 32);
            ata_write_sectors(de_lba, 1, sector_buf);
        }
    }

    return (int)written;
}

static int fat32_inode_seek(struct inode *ino, long offset, int whence) {
    if (!ino) return -1;
    long new_pos = 0;
    switch (whence) {
        case 0: new_pos = offset; break;                        // SEEK_SET
        case 1: new_pos = ino->position + offset; break;       // SEEK_CUR
        case 2: new_pos = (long)ino->size + offset; break;     // SEEK_END
        default: return -1;
    }
    if (new_pos < 0) return -1;
    ino->position = new_pos;
    return 0;
}

static void fat32_inode_close(struct inode *ino) {
    if (ino && ino->fs_data) {
        free(ino->fs_data);
        ino->fs_data = NULL;
    }
}

static struct inode_ops fat32_inode_ops = {
    .read  = fat32_inode_read,
    .write = fat32_inode_write,
    .seek  = fat32_inode_seek,
    .close = fat32_inode_close,
};

// ── VFS filesystem operations ───────────────────────────────────────────────

static int fat32_fs_lookup(struct filesystem *fs, const char *name, struct inode **ino) {
    if (!fs || !name || !ino) return -1;

    struct fat32_private *priv = (struct fat32_private *)fs->fs_data;
    struct fat32_dir_entry entry;
    uint32_t dc, doff;

    if (fat32_resolve_path(priv, name, &entry, &dc, &doff, 0, NULL) < 0)
        return -1;

    struct inode *node = (struct inode *)malloc(sizeof(struct inode));
    if (!node) return -1;

    struct fat32_file_data *fd = (struct fat32_file_data *)malloc(sizeof(struct fat32_file_data));
    if (!fd) { free(node); return -1; }

    fd->first_cluster = ((uint32_t)entry.first_cluster_hi << 16) | entry.first_cluster_lo;
    fd->file_size = entry.file_size;
    fd->dir_cluster = dc;
    fd->dir_offset = doff;
    fd->is_dir = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;

    node->inode_num = fd->first_cluster;
    node->mode = fd->is_dir ? 0755 : 0644;
    node->size = fd->file_size;
    node->ref_count = 1;
    node->fs = fs;
    node->ops = &fat32_inode_ops;
    node->fs_data = fd;
    node->position = 0;

    *ino = node;
    return 0;
}

static int fat32_fs_open(struct filesystem *fs, const char *path, int flags, struct inode **ino) {
    if (!fs || !path || !ino) return -1;

    struct fat32_private *priv = (struct fat32_private *)fs->fs_data;
    struct fat32_dir_entry entry;
    uint32_t dc, doff;
    uint32_t parent_cluster = 0;

    int found = fat32_resolve_path(priv, path, &entry, &dc, &doff, 1, &parent_cluster);

    if (found == 0) {
        // File exists
        if (flags & O_TRUNC) {
            // TODO: free cluster chain and reset size
            entry.file_size = 0;
            fat32_write_dir_entry(priv, dc, doff, &entry);
        }
    } else if (flags & O_CREAT) {
        // Create new file
        if (parent_cluster < 2) parent_cluster = priv->root_cluster;

        uint32_t slot_cluster, slot_offset;
        if (fat32_find_free_dir_slot(priv, parent_cluster, &slot_cluster, &slot_offset) < 0)
            return -1;

        memset(&entry, 0, sizeof(entry));

        // Get the filename component (last part of path)
        const char *filename = path;
        const char *p = path;
        while (*p) { if (*p == '/') filename = p + 1; p++; }
        to_fat_name(filename, entry.name);

        entry.attr = ATTR_ARCHIVE;
        entry.file_size = 0;
        // No cluster allocated yet — will be allocated on first write

        if (fat32_write_dir_entry(priv, slot_cluster, slot_offset, &entry) < 0)
            return -1;

        dc = slot_cluster;
        doff = slot_offset;
    } else {
        return -1; // not found
    }

    // Build inode
    struct inode *node = (struct inode *)malloc(sizeof(struct inode));
    if (!node) return -1;

    struct fat32_file_data *fd = (struct fat32_file_data *)malloc(sizeof(struct fat32_file_data));
    if (!fd) { free(node); return -1; }

    fd->first_cluster = ((uint32_t)entry.first_cluster_hi << 16) | entry.first_cluster_lo;
    fd->file_size = entry.file_size;
    fd->dir_cluster = dc;
    fd->dir_offset = doff;
    fd->is_dir = (entry.attr & ATTR_DIRECTORY) ? 1 : 0;

    node->inode_num = fd->first_cluster;
    node->mode = fd->is_dir ? 0755 : 0644;
    node->size = fd->file_size;
    node->ref_count = 1;
    node->fs = fs;
    node->ops = &fat32_inode_ops;
    node->fs_data = fd;
    node->position = (flags & O_APPEND) ? fd->file_size : 0;

    *ino = node;
    return 0;
}

static int fat32_fs_mkdir(struct filesystem *fs, const char *path) {
    if (!fs || !path) return -1;

    struct fat32_private *priv = (struct fat32_private *)fs->fs_data;

    // Check it doesn't already exist
    struct fat32_dir_entry existing;
    if (fat32_resolve_path(priv, path, &existing, NULL, NULL, 0, NULL) == 0)
        return -1; // already exists

    // Find parent directory
    uint32_t parent_cluster = priv->root_cluster;

    // Walk to parent
    const char *last_slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last_slash = p;
    }

    char parent_path[256];
    const char *dir_name = path;
    if (last_slash && last_slash > path) {
        size_t plen = (size_t)(last_slash - path);
        if (plen >= sizeof(parent_path)) return -1;
        memcpy(parent_path, path, plen);
        parent_path[plen] = '\0';
        dir_name = last_slash + 1;

        struct fat32_dir_entry parent_entry;
        uint32_t pc, po;
        if (fat32_resolve_path(priv, parent_path, &parent_entry, &pc, &po, 0, NULL) < 0)
            return -1;
        if (!(parent_entry.attr & ATTR_DIRECTORY)) return -1;
        parent_cluster = ((uint32_t)parent_entry.first_cluster_hi << 16) | parent_entry.first_cluster_lo;
    } else {
        while (*dir_name == '/') dir_name++;
    }

    // Allocate a cluster for the new directory
    uint32_t new_cluster = fat_alloc_cluster(priv);
    if (new_cluster == 0) return -1;

    // Write . and .. entries
    struct fat32_dir_entry dot;
    memset(&dot, 0, sizeof(dot));
    memset(dot.name, ' ', 11);
    dot.name[0] = '.';
    dot.attr = ATTR_DIRECTORY;
    dot.first_cluster_hi = (uint16_t)(new_cluster >> 16);
    dot.first_cluster_lo = (uint16_t)(new_cluster);

    struct fat32_dir_entry dotdot;
    memset(&dotdot, 0, sizeof(dotdot));
    memset(dotdot.name, ' ', 11);
    dotdot.name[0] = '.';
    dotdot.name[1] = '.';
    dotdot.attr = ATTR_DIRECTORY;
    dotdot.first_cluster_hi = (uint16_t)(parent_cluster >> 16);
    dotdot.first_cluster_lo = (uint16_t)(parent_cluster);

    // Write them to the first sector of the new cluster
    uint32_t dir_lba = cluster_to_lba(priv, new_cluster);
    if (ata_read_sectors(dir_lba, 1, sector_buf) < 0) return -1;
    memcpy(sector_buf + 0, &dot, 32);
    memcpy(sector_buf + 32, &dotdot, 32);
    if (ata_write_sectors(dir_lba, 1, sector_buf) < 0) return -1;

    // Add entry in parent directory
    uint32_t slot_cluster, slot_offset;
    if (fat32_find_free_dir_slot(priv, parent_cluster, &slot_cluster, &slot_offset) < 0)
        return -1;

    struct fat32_dir_entry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    to_fat_name(dir_name, new_entry.name);
    new_entry.attr = ATTR_DIRECTORY;
    new_entry.first_cluster_hi = (uint16_t)(new_cluster >> 16);
    new_entry.first_cluster_lo = (uint16_t)(new_cluster);

    return fat32_write_dir_entry(priv, slot_cluster, slot_offset, &new_entry);
}

static int fat32_fs_rmdir(struct filesystem *fs __attribute__((unused)),
                          const char *path __attribute__((unused))) {
    return -1; // not implemented
}

static void fat32_fs_unmount(struct filesystem *fs) {
    if (fs && fs->fs_data) {
        free(fs->fs_data);
        fs->fs_data = NULL;
    }
}

static struct filesystem_ops fat32_fs_ops = {
    .lookup  = fat32_fs_lookup,
    .open    = fat32_fs_open,
    .mkdir   = fat32_fs_mkdir,
    .rmdir   = fat32_fs_rmdir,
    .unmount = fat32_fs_unmount,
};

// ── Public API ──────────────────────────────────────────────────────────────

struct filesystem *fat32_mount_disk(void) {
    // Read BPB from sector 0
    if (ata_read_sectors(0, 1, sector_buf) < 0) return NULL;

    struct fat32_bpb *bpb = (struct fat32_bpb *)sector_buf;

    // Sanity checks
    if (bpb->bytes_per_sector != 512) return NULL;
    if (bpb->num_fats == 0) return NULL;
    if (bpb->fat_size_32 == 0) return NULL;
    if (bpb->sectors_per_cluster == 0) return NULL;

    // Check for FAT32 signature
    if (memcmp(bpb->fs_type, "FAT32   ", 8) != 0) return NULL;

    struct fat32_private *priv = (struct fat32_private *)malloc(sizeof(struct fat32_private));
    if (!priv) return NULL;

    priv->bytes_per_sector = bpb->bytes_per_sector;
    priv->sectors_per_cluster = bpb->sectors_per_cluster;
    priv->reserved_sectors = bpb->reserved_sectors;
    priv->num_fats = bpb->num_fats;
    priv->fat_size = bpb->fat_size_32;
    priv->root_cluster = bpb->root_cluster;
    priv->total_sectors = bpb->total_sectors_32;
    priv->fat_start_sector = priv->reserved_sectors;
    priv->data_start_sector = priv->reserved_sectors + priv->num_fats * priv->fat_size;
    priv->cluster_count = (priv->total_sectors - priv->data_start_sector) / priv->sectors_per_cluster;

    struct filesystem *fs = (struct filesystem *)malloc(sizeof(struct filesystem));
    if (!fs) { free(priv); return NULL; }

    fs->ops = &fat32_fs_ops;
    fs->fs_data = priv;
    fs->root = NULL;

    return fs;
}

struct filesystem *fat32_format(uint32_t total_sectors, const char *volume_label) {
    if (total_sectors == 0) total_sectors = 262144; // 128MB default

    // FAT32 geometry
    uint16_t reserved_sectors = 32;
    uint8_t  num_fats = 2;
    uint8_t  sectors_per_cluster = 8; // 4KB clusters
    uint32_t data_sectors = total_sectors - reserved_sectors;
    // fat_size = ceil(data_sectors * 4 / 512) roughly, iterative:
    // clusters ≈ data_sectors / spc, FAT entries = clusters, FAT sectors = ceil(entries*4/512)
    uint32_t cluster_count = data_sectors / sectors_per_cluster;
    uint32_t fat_size = (cluster_count * 4 + 511) / 512;
    // Recalculate with FAT overhead
    uint32_t actual_data_sectors = total_sectors - reserved_sectors - num_fats * fat_size;
    cluster_count = actual_data_sectors / sectors_per_cluster;
    fat_size = (cluster_count * 4 + 511) / 512;

    uint32_t root_cluster = 2;

    // ── Write BPB (sector 0) ──
    memset(sector_buf, 0, 512);
    struct fat32_bpb *bpb = (struct fat32_bpb *)sector_buf;

    bpb->jmp[0] = 0xEB; bpb->jmp[1] = 0x58; bpb->jmp[2] = 0x90;
    memcpy(bpb->oem_name, "MYOS    ", 8);
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = sectors_per_cluster;
    bpb->reserved_sectors = reserved_sectors;
    bpb->num_fats = num_fats;
    bpb->root_entry_count = 0;
    bpb->total_sectors_16 = 0;
    bpb->media_type = 0xF8; // hard disk
    bpb->fat_size_16 = 0;
    bpb->sectors_per_track = 63;
    bpb->num_heads = 255;
    bpb->hidden_sectors = 0;
    bpb->total_sectors_32 = total_sectors;
    bpb->fat_size_32 = fat_size;
    bpb->ext_flags = 0;
    bpb->fs_version = 0;
    bpb->root_cluster = root_cluster;
    bpb->fs_info_sector = 1;
    bpb->backup_boot_sector = 6;
    bpb->drive_number = 0x80;
    bpb->boot_sig = 0x29;
    bpb->volume_serial = 0x12345678;

    if (volume_label) {
        memset(bpb->volume_label, ' ', 11);
        for (int i = 0; i < 11 && volume_label[i]; i++) {
            char c = volume_label[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            bpb->volume_label[i] = c;
        }
    } else {
        memcpy(bpb->volume_label, "NO NAME    ", 11);
    }
    memcpy(bpb->fs_type, "FAT32   ", 8);

    // Boot signature
    sector_buf[510] = 0x55;
    sector_buf[511] = 0xAA;

    if (ata_write_sectors(0, 1, sector_buf) < 0) return NULL;

    // ── Write FSInfo (sector 1) ──
    memset(sector_buf, 0, 512);
    // FSInfo signatures
    sector_buf[0] = 0x52; sector_buf[1] = 0x52; sector_buf[2] = 0x61; sector_buf[3] = 0x41; // "RRaA"
    sector_buf[484] = 0x72; sector_buf[485] = 0x72; sector_buf[486] = 0x41; sector_buf[487] = 0x61; // "rrAa"
    // Free cluster count
    uint32_t free_count = cluster_count - 1; // minus root dir cluster
    memcpy(sector_buf + 488, &free_count, 4);
    // Next free cluster hint
    uint32_t next_free = 3;
    memcpy(sector_buf + 492, &next_free, 4);
    sector_buf[510] = 0x55; sector_buf[511] = 0xAA;
    if (ata_write_sectors(1, 1, sector_buf) < 0) return NULL;

    // ── Zero reserved sectors 2..31 ──
    memset(sector_buf, 0, 512);
    for (uint32_t s = 2; s < reserved_sectors; s++) {
        // Write backup BPB at sector 6
        if (s == 6) {
            // Re-read BPB and write as backup
            ata_read_sectors(0, 1, sector_buf);
            ata_write_sectors(6, 1, sector_buf);
            memset(sector_buf, 0, 512);
            continue;
        }
        ata_write_sectors(s, 1, sector_buf);
    }

    // ── Initialize FAT tables ──
    uint32_t fat_start = reserved_sectors;
    for (uint8_t f = 0; f < num_fats; f++) {
        uint32_t base = fat_start + f * fat_size;

        // Zero out all FAT sectors
        memset(sector_buf, 0, 512);
        for (uint32_t s = 0; s < fat_size; s++) {
            ata_write_sectors(base + s, 1, sector_buf);
        }

        // Write reserved entries in first sector
        ata_read_sectors(base, 1, sector_buf);
        // Entry 0: media type + 0xFFFFFF00
        uint32_t entry0 = 0x0FFFFFF8;
        memcpy(sector_buf + 0, &entry0, 4);
        // Entry 1: EOC marker
        uint32_t entry1 = 0x0FFFFFFF;
        memcpy(sector_buf + 4, &entry1, 4);
        // Entry 2: root directory cluster (EOC)
        uint32_t entry2 = 0x0FFFFFFF;
        memcpy(sector_buf + 8, &entry2, 4);

        ata_write_sectors(base, 1, sector_buf);
    }

    // ── Zero root directory cluster ──
    uint32_t data_start = reserved_sectors + num_fats * fat_size;
    memset(sector_buf, 0, 512);
    for (uint8_t s = 0; s < sectors_per_cluster; s++) {
        ata_write_sectors(data_start + s, 1, sector_buf);
    }

    // Write volume label entry in root directory
    if (volume_label) {
        ata_read_sectors(data_start, 1, sector_buf);
        struct fat32_dir_entry *vol = (struct fat32_dir_entry *)sector_buf;
        memset(vol, 0, 32);
        memset(vol->name, ' ', 11);
        for (int i = 0; i < 11 && volume_label[i]; i++) {
            char c = volume_label[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            vol->name[i] = c;
        }
        vol->attr = ATTR_VOLUME_ID;
        ata_write_sectors(data_start, 1, sector_buf);
    }

    printf("fat32: formatted %u sectors (%u MB), %u clusters\n",
           total_sectors, total_sectors / 2048, cluster_count);

    // Now mount the freshly created filesystem
    return fat32_mount_disk();
}
