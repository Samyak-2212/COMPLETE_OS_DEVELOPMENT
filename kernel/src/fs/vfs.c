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
    if (!node) return NULL;
    
    /* If this is a mount point, traverse into the mounted FS */
    if ((node->flags & FS_MOUNTPOINT) && node->ptr) {
        node = node->ptr;
    }

    if ((node->flags & FS_DIRECTORY) == FS_DIRECTORY && node->ops && node->ops->finddir) {
        return node->ops->finddir(node, name);
    }
    return NULL;
}

vfs_node_t *vfs_resolve_path(const char *path) {
    if (!path || path[0] != '/') return NULL;
    if (path[1] == '\0') return vfs_root;

    vfs_node_t *current = vfs_root;
    char buffer[128];
    int b = 0;
    
    for (int i = 1; ; i++) {
        if (path[i] == '/' || path[i] == '\0') {
            if (b > 0) {
                buffer[b] = '\0';
                vfs_node_t *next = vfs_finddir(current, buffer);
                if (!next) return NULL;

                /* Note: In a complete VFS, we'd handle intermediate node freeing here 
                 * if they were dynamically allocated by the underlying FS. */
                current = next;
                b = 0;
            }
            if (path[i] == '\0') break;
        } else {
            if (b < 127) {
                buffer[b++] = path[i];
            }
        }
    }
    
    return current;
}

int vfs_mkdir(const char *path) {
    if (!path || path[0] != '/') return -1;

    /* Handle the case where we're making a directory in root */
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) return -1;

    char parent_path[128];
    char dir_name[128];

    int parent_len = last_slash - path;
    if (parent_len == 0) {
        strcpy(parent_path, "/");
    } else {
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }

    strcpy(dir_name, last_slash + 1);

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent) return -1;

    if (parent->ops && parent->ops->mkdir) {
        vfs_node_t *new_node = parent->ops->mkdir(parent, dir_name);
        return new_node ? 0 : -1;
    }

    return -1;
}

int vfs_mount(const char *path, vfs_node_t *local_root) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return -1;

    /* Overlay the filesystem */
    node->ptr = local_root;
    node->flags |= FS_MOUNTPOINT;
    
    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[VFS] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Mounted filesystem at %s\n", path);
    
    return 0;
}
