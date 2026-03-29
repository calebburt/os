#ifndef MEMFS_H
#define MEMFS_H

#include "vfs.h"

// Create an in-memory filesystem (useful for testing)
// Returns a filesystem instance ready to mount
struct filesystem *memfs_create(void);

#endif
