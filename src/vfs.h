#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

#define MAX_OPENS 16

// POSIX-style mode bits encoded in inode->mode and stat::mode.
#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)

struct stat {
    uint64_t size;
    uint32_t mode;
};

struct dirent {
    char     name[64];
    uint32_t mode;
    uint64_t size;
};

extern struct open_file *handles[MAX_OPENS];
extern int open_count;

// Forward declarations
struct inode;
struct filesystem;

// Inode operations - filesystem drivers implement these
struct inode_ops {
    int (*read)(struct inode *ino, uint8_t *buf, size_t offset, size_t len);
    int (*write)(struct inode *ino, const uint8_t *buf, size_t offset, size_t len);
    int (*seek)(struct inode *ino, long offset, int whence);
    int (*readdir)(struct inode *ino, int index, struct dirent *out);
    void (*close)(struct inode *ino);
};

// Filesystem operations - filesystem drivers implement these
struct filesystem_ops {
    int (*lookup)(struct filesystem *fs, const char *name, struct inode **ino);
    int (*mkdir)(struct filesystem *fs, const char *path);
    int (*rmdir)(struct filesystem *fs, const char *path);
    int (*unlink)(struct filesystem *fs, const char *path);
    int (*open)(struct filesystem *fs, const char *path, int flags, struct inode **ino);
    void (*unmount)(struct filesystem *fs);
};

// Inode represents a file/directory
struct inode {
    uint64_t inode_num;              // Filesystem-specific inode number
    uint32_t mode;                   // File type and permissions
    uint64_t size;                   // File size in bytes
    uint32_t ref_count;              // Reference counting
    struct filesystem *fs;           // Owning filesystem
    struct inode_ops *ops;           // Operation function pointers
    void *fs_data;                   // Filesystem-specific data
    long position;                   // Current file position
};

// Mounted filesystem instance
struct filesystem {
    char mount_point[256];           // Where this FS is mounted (e.g., "/", "/mnt/disk")
    struct filesystem_ops *ops;      // Operation function pointers
    void *fs_data;                   // Filesystem-specific private data
    struct inode *root;              // Root inode of this filesystem
};

// Open file with handle

struct open_file {
    long handle;
    struct inode *inode;
};

// VFS initialization
void vfs_init(void);

// Mount/unmount filesystems
int vfs_mount(const char *mount_point, struct filesystem *fs);
int vfs_unmount(const char *mount_point);

// File operations through VFS
struct inode *vfs_open(const char *path, int flags);
int vfs_read(struct inode *ino, uint8_t *buf, size_t len);
int vfs_write(struct inode *ino, const uint8_t *buf, size_t len);
int vfs_seek(struct inode *ino, long offset, int whence);
int vfs_close(struct inode *ino);
int vfs_stat(const char *path, struct stat *out);
int vfs_readdir(struct inode *ino, int index, struct dirent *out);

// Path resolution
int vfs_lookup_path(const char *path, struct inode **ino);
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);

// User-space utils
struct open_file *vfs_open_handle(const char *path, int flags);

#endif
