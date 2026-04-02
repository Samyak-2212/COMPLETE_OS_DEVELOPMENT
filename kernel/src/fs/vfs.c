/* ============================================================================
 * vfs.c — NexusOS Virtual Filesystem Router
 * Purpose: Routes abstract vfs_read/write calls to specific filesystem implementations
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/vfs.h"
#include "lib/string.h"
#include "lib/printf.h"

vfs_node_t *vfs_root = NULL;

uint64_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if (node && node->ops && node->ops->read) {
        return node->ops->read(node, offset, size, buffer);
    }
    return 0;
}

uint64_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if (node && node->ops && node->ops->write) {
        return node->ops->write(node, offset, size, buffer);
    }
    return 0;
}

void vfs_open(vfs_node_t *node) {
    if (node && node->ops && node->ops->open) {
        node->ops->open(node);
    }
}

void vfs_close(vfs_node_t *node) {
    if (node && node->ops && node->ops->close) {
        node->ops->close(node);
    }
}

dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index) {
    if ((node->flags & FS_DIRECTORY) == FS_DIRECTORY && node->ops && node->ops->readdir) {
        return node->ops->readdir(node, index);
    }
    return NULL;
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name) {
    if ((node->flags & FS_DIRECTORY) == FS_DIRECTORY && node->ops && node->ops->finddir) {
        return node->ops->finddir(node, name);
    }
    return NULL;
}
