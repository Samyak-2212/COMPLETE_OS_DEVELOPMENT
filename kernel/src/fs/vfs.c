/* ============================================================================
 * vfs.c — NexusOS Virtual Filesystem Router
 * Purpose: Routes abstract vfs_read/write calls to specific filesystem implementations
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/vfs.h"
#include "lib/debug.h"
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
    
    /* If the requested parent is itself a stackable mount point, use its target */
    if ((node->flags & FS_MOUNTPOINT) && node->ptr) {
        node = node->ptr;
    }

    if ((node->flags & FS_DIRECTORY) && node->ops && node->ops->finddir) {
        vfs_node_t *child = node->ops->finddir(node, name);
        /* Transparently step across mount points when navigating down */
        if (child && (child->flags & FS_MOUNTPOINT) && child->ptr) {
            return child->ptr;
        }
        return child;
    }
    return NULL;
}

static vfs_node_t *vfs_resolve_recursive(vfs_node_t *current, const char *path) {
    if (!path || *path == '\0') return current;

    /* Skip leading slashes */
    while (*path == '/') path++;
    if (*path == '\0') return current;

    /* Extract next component */
    char buffer[128];
    int b = 0;
    const char *next_path = path;
    while (*next_path != '\0' && *next_path != '/') {
        if (b < 127) buffer[b++] = *next_path;
        next_path++;
    }
    buffer[b] = '\0';

    /* Find component in current node */
    vfs_node_t *next_node = vfs_finddir(current, buffer);
    if (!next_node) {
        debug_log(DEBUG_LEVEL_ERROR, "VFS", "resolve failed: '%s' not found", buffer);
        return NULL;
    }

    /* Recurse with the remainder of the path */
    return vfs_resolve_recursive(next_node, next_path);
}

vfs_node_t *vfs_resolve_path(const char *path) {
    if (!path || path[0] != '/') return NULL;
    
    /* Traverse root if it's a mount point */
    vfs_node_t *node = vfs_root;
    while (node && (node->flags & FS_MOUNTPOINT) && node->ptr) {
        node = node->ptr;
    }

    /* Recurse with the remainder of the path */
    return vfs_resolve_recursive(node, path + 1);
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

vfs_node_t *vfs_create(const char *path) {
    if (!path || path[0] != '/') return NULL;

    const char *last_slash = strrchr(path, '/');
    if (!last_slash) return NULL;

    char parent_path[128];
    char file_name[128];

    int parent_len = last_slash - path;
    if (parent_len == 0) {
        strcpy(parent_path, "/");
    } else {
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
    }

    strcpy(file_name, last_slash + 1);

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent) return NULL;

    if (parent->ops && parent->ops->create) {
        return parent->ops->create(parent, file_name);
    }

    return NULL;
}

int vfs_mount(const char *path, vfs_node_t *local_root) {
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) return -1;

    /* Overlay the filesystem */
    node->ptr = local_root;
    node->flags |= FS_MOUNTPOINT;
    
    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[VFS] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("Mounted filesystem at %s\n", path);
    
    return 0;
}
