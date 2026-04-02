/* ============================================================================
 * ramfs.h — NexusOS RAM Filesystem
 * Purpose: Provides a simple in-memory root filesystem to mount other FS onto
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_FS_RAMFS_H
#define NEXUS_FS_RAMFS_H

#include "fs/vfs.h"

/* Initialize the RAM filesystem and return its root node */
vfs_node_t *ramfs_init(void);

#endif /* NEXUS_FS_RAMFS_H */
