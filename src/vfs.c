#include "vfs.h"
#include <stddef.h>
#include <string.h>

// Maximum number of mounted filesystems
#define MAX_MOUNTS 16

// Maximum number of files opened at the same time
#define MAX_OPENS 16

// Global mount table
static struct filesystem *mount_table[MAX_MOUNTS];
static int mount_count = 0;

static struct open_file handles[MAX_OPENS];
static int open_count = 0;

// VFS initialization
void vfs_init(void) {
    // Initialize mount table
    for (int i = 0; i < MAX_MOUNTS; i++) {
        mount_table[i] = NULL;
    }
    mount_count = 0;
}

// Find the filesystem for a given path
static struct filesystem *vfs_find_filesystem(const char *path, const char **rel_path) {
    if (!path || path[0] != '/') {
        return NULL;
    }
    
    struct filesystem *best_match = NULL;
    size_t best_len = 0;
    
    // Find the longest matching mount point
    for (int i = 0; i < mount_count; i++) {
        if (!mount_table[i]) continue;
        
        const char *mp = mount_table[i]->mount_point;
        size_t mp_len = strlen(mp);
        
        if (mp_len == 1 && mp[0] == '/') {
            // Root filesystem "/" matches everything
            best_match = mount_table[i];
            best_len = 1;
            *rel_path = path + 1;  // Skip leading /
        } else if (strncmp(path, mp, mp_len) == 0) {
            // Check if this is a better match
            if (mp_len > best_len) {
                best_match = mount_table[i];
                best_len = mp_len;
                *rel_path = path + mp_len;
                if (*rel_path[0] == '/') {
                    (*rel_path)++;  // Skip separator after mount point
                }
            }
        }
    }
    
    return best_match;
}

// Mount a filesystem at a path
int vfs_mount(const char *mount_point, struct filesystem *fs) {
    if (!mount_point || !fs || mount_count >= MAX_MOUNTS) {
        return -1;
    }
    
    // Check for duplicate mount point
    for (int i = 0; i < mount_count; i++) {
        if (mount_table[i] && strcmp(mount_table[i]->mount_point, mount_point) == 0) {
            return -1;  // Already mounted
        }
    }
    
    // Store filesystem in mount table
    mount_table[mount_count] = fs;
    strncpy(fs->mount_point, mount_point, sizeof(fs->mount_point) - 1);
    fs->mount_point[sizeof(fs->mount_point) - 1] = '\0';
    mount_count++;
    
    return 0;
}

// Unmount a filesystem
int vfs_unmount(const char *mount_point) {
    if (!mount_point) {
        return -1;
    }
    
    for (int i = 0; i < mount_count; i++) {
        if (mount_table[i] && strcmp(mount_table[i]->mount_point, mount_point) == 0) {
            // Call filesystem's unmount operation
            if (mount_table[i]->ops && mount_table[i]->ops->unmount) {
                mount_table[i]->ops->unmount(mount_table[i]);
            }
            
            // Remove from mount table
            for (int j = i; j < mount_count - 1; j++) {
                mount_table[j] = mount_table[j + 1];
            }
            mount_count--;
            return 0;
        }
    }
    
    return -1;  // Not found
}

// Resolve a path to an inode
int vfs_lookup_path(const char *path, struct inode **ino) {
    if (!path || !ino) {
        return -1;
    }
    
    const char *rel_path = NULL;
    struct filesystem *fs = vfs_find_filesystem(path, &rel_path);
    
    if (!fs || !fs->ops || !fs->ops->lookup) {
        return -1;  // No filesystem or lookup not supported
    }
    
    return fs->ops->lookup(fs, rel_path ? rel_path : path, ino);
}

// Open a file
struct inode *vfs_open(const char *path, int flags) {
    if (!path) {
        return NULL;
    }
    
    const char *rel_path = NULL;
    struct filesystem *fs = vfs_find_filesystem(path, &rel_path);
    
    if (!fs || !fs->ops || !fs->ops->open) {
        return NULL;
    }
    
    struct inode *ino = NULL;
    if (fs->ops->open(fs, rel_path ? rel_path : path, flags, &ino) < 0) {
        return NULL;
    }
    
    return ino;
}

struct open_file vfs_open_handle(const char *path, int flags) {
    struct open_file file;
    file.inode = vfs_open(path, flags);
}

// Read from an inode
int vfs_read(struct inode *ino, uint8_t *buf, size_t len) {
    if (!ino || !buf || !ino->ops || !ino->ops->read) {
        return -1;
    }
    
    int bytes_read = ino->ops->read(ino, buf, ino->position, len);
    if (bytes_read > 0) {
        ino->position += bytes_read;
    }
    return bytes_read;
}

// Write to an inode
int vfs_write(struct inode *ino, const uint8_t *buf, size_t len) {
    if (!ino || !buf || !ino->ops || !ino->ops->write) {
        return -1;
    }
    
    int bytes_written = ino->ops->write(ino, buf, ino->position, len);
    if (bytes_written > 0) {
        ino->position += bytes_written;
    }
    return bytes_written;
}

// Seek in an inode
int vfs_seek(struct inode *ino, long offset, int whence) {
    if (!ino || !ino->ops || !ino->ops->seek) {
        return -1;
    }
    
    return ino->ops->seek(ino, offset, whence);
}

// Close an inode
int vfs_close(struct inode *ino) {
    if (!ino) {
        return -1;
    }
    
    ino->ref_count--;
    
    if (ino->ref_count <= 0 && ino->ops && ino->ops->close) {
        ino->ops->close(ino);
    }
    
    return 0;
}

// Create a directory
int vfs_mkdir(const char *path) {
    if (!path) {
        return -1;
    }
    
    const char *rel_path = NULL;
    struct filesystem *fs = vfs_find_filesystem(path, &rel_path);
    
    if (!fs || !fs->ops || !fs->ops->mkdir) {
        return -1;
    }
    
    return fs->ops->mkdir(fs, rel_path ? rel_path : path);
}

// Remove a directory
int vfs_rmdir(const char *path) {
    if (!path) {
        return -1;
    }
    
    const char *rel_path = NULL;
    struct filesystem *fs = vfs_find_filesystem(path, &rel_path);
    
    if (!fs || !fs->ops || !fs->ops->rmdir) {
        return -1;
    }
    
    return fs->ops->rmdir(fs, rel_path ? rel_path : path);
}
