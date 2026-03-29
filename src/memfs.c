#include "memfs.h"
#include "stdio.h"
#include <string.h>
#include <stdlib.h>

// Simple in-memory filesystem for demonstration

// In-memory file structure
struct memfs_file {
    char name[256];
    uint8_t data[4096];          // Small 4KB file
    size_t size;
    struct inode *inode;
};

// In-memory filesystem private data
struct memfs_private {
    struct memfs_file files[16];  // Max 16 files
    int file_count;
};

// Inode operations
static int memfs_read(struct inode *ino, uint8_t *buf, size_t offset, size_t len) {
    if (!ino || !buf || !ino->fs_data) {
        return -1;
    }
    
    struct memfs_file *file = (struct memfs_file *)ino->fs_data;
    
    if (offset > file->size) {
        return 0;  // EOF
    }
    
    size_t to_read = len;
    if (offset + to_read > file->size) {
        to_read = file->size - offset;
    }
    
    memcpy(buf, file->data + offset, to_read);
    return (int)to_read;
}

static int memfs_write(struct inode *ino, const uint8_t *buf, size_t offset, size_t len) {
    if (!ino || !buf || !ino->fs_data) {
        return -1;
    }
    
    struct memfs_file *file = (struct memfs_file *)ino->fs_data;
    
    if (offset + len > sizeof(file->data)) {
        len = sizeof(file->data) - offset;  // Truncate to file size limit
    }
    
    memcpy(file->data + offset, buf, len);
    
    if (offset + len > file->size) {
        file->size = offset + len;
    }
    
    return (int)len;
}

static int memfs_seek(struct inode *ino, long offset, int whence) {
    if (!ino) return -1;
    
    long new_pos = 0;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = ino->position + offset;
            break;
        case SEEK_END:
            new_pos = (long)ino->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos < 0) {
        return -1;
    }
    
    ino->position = new_pos;
    return 0;
}

static void memfs_close(struct inode *ino) {
    // No cleanup needed for in-memory files
    if (ino) {
        ino->ref_count = 0;
    }
}

// Filesystem operations
static int memfs_lookup(struct filesystem *fs, const char *name, struct inode **ino) {
    if (!fs || !name || !ino || !fs->fs_data) {
        return -1;
    }
    
    struct memfs_private *priv = (struct memfs_private *)fs->fs_data;
    
    // Simple linear search
    for (int i = 0; i < priv->file_count; i++) {
        if (strcmp(priv->files[i].name, name) == 0) {
            *ino = priv->files[i].inode;
            return 0;
        }
    }
    
    return -1;  // Not found
}

static int memfs_open(struct filesystem *fs, const char *path, int flags, struct inode **ino) {
    if (!fs || !path || !ino || !fs->fs_data) {
        return -1;
    }
    
    struct memfs_private *priv = (struct memfs_private *)fs->fs_data;
    
    // Simple approach: treat path as filename (no directory support)
    const char *filename = path;
    
    // Skip leading slashes
    while (*filename == '/') {
        filename++;
    }
    
    // Look for existing file
    for (int i = 0; i < priv->file_count; i++) {
        if (strcmp(priv->files[i].name, filename) == 0) {
            *ino = priv->files[i].inode;
            (*ino)->ref_count++;
            return 0;
        }
    }
    
    // File doesn't exist
    if (!(flags & O_CREAT)) {
        return -1;  // Not found and not creating
    }
    
    // Create new file
    if (priv->file_count >= 16) {
        return -1;  // Too many files
    }
    
    struct memfs_file *file = &priv->files[priv->file_count];
    strncpy(file->name, filename, sizeof(file->name) - 1);
    file->name[sizeof(file->name) - 1] = '\0';
    file->size = 0;
    
    // Create inode for this file
    struct inode *new_ino = (struct inode *)malloc(sizeof(struct inode));
    if (!new_ino) {
        return -1;  // Memory allocation failed
    }
    
    new_ino->inode_num = priv->file_count;
    new_ino->mode = 0644;  // Regular file
    new_ino->size = 0;
    new_ino->ref_count = 1;
    new_ino->fs = fs;
    new_ino->fs_data = file;
    new_ino->position = 0;
    
    // Set up inode operations
    static struct inode_ops memfs_inode_ops = {
        .read = memfs_read,
        .write = memfs_write,
        .seek = memfs_seek,
        .close = memfs_close,
    };
    new_ino->ops = &memfs_inode_ops;
    
    file->inode = new_ino;
    priv->file_count++;
    
    *ino = new_ino;
    return 0;
}

static int memfs_mkdir(struct filesystem *fs __attribute__((unused)), const char *path __attribute__((unused))) {
    // Not implemented for memfs
    return -1;
}

static int memfs_rmdir(struct filesystem *fs __attribute__((unused)), const char *path __attribute__((unused))) {
    // Not implemented for memfs
    return -1;
}

static void memfs_unmount(struct filesystem *fs) {
    if (!fs || !fs->fs_data) {
        return;
    }
    
    struct memfs_private *priv = (struct memfs_private *)fs->fs_data;
    
    // Free allocated inodes
    for (int i = 0; i < priv->file_count; i++) {
        if (priv->files[i].inode) {
            free(priv->files[i].inode);
        }
    }
    
    free(priv);
}

// Public factory function
struct filesystem *memfs_create(void) {
    // Allocate private data
    struct memfs_private *priv = (struct memfs_private *)malloc(sizeof(struct memfs_private));
    if (!priv) {
        return NULL;
    }
    
    priv->file_count = 0;
    
    // Allocate filesystem structure
    struct filesystem *fs = (struct filesystem *)malloc(sizeof(struct filesystem));
    if (!fs) {
        free(priv);
        return NULL;
    }
    
    // Set up operations
    static struct filesystem_ops memfs_ops = {
        .lookup = memfs_lookup,
        .mkdir = memfs_mkdir,
        .rmdir = memfs_rmdir,
        .open = memfs_open,
        .unmount = memfs_unmount,
    };
    
    fs->ops = &memfs_ops;
    fs->fs_data = priv;
    fs->root = NULL;  // Not used for memfs
    
    return fs;
}
