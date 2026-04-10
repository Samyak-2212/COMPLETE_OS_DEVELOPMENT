/* ============================================================================
 * ramfs.c — NexusOS RAM Filesystem
 * Purpose: Provides an in-memory root filesystem
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/ramfs.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"

#define RAMFS_MAX_CHILDREN 64

/* Internal struct to store ramfs node data */
typedef struct {
    uint8_t *data;          /* File contents */
    uint64_t capacity;      /* Max allocated bytes */
    
    /* If this is a directory */
    vfs_node_t *children[RAMFS_MAX_CHILDREN];
    int child_count;
} ramfs_internal_t;

static vfs_ops_t ramfs_ops;

static uint64_t ramfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if ((node->flags & FS_FILE) != FS_FILE) return 0;
    
    ramfs_internal_t *priv = (ramfs_internal_t*)node->impl;
    if (!priv || !priv->data) return 0;
    
    if (offset >= node->length) return 0;
    uint64_t readable = node->length - offset;
    if (size > readable) size = readable;
    
    memcpy(buffer, priv->data + offset, size);
    return size;
}

static uint64_t ramfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if ((node->flags & FS_FILE) != FS_FILE) return 0;
    
    ramfs_internal_t *priv = (ramfs_internal_t*)node->impl;
    if (!priv) return 0;

    /* Reallocate if needed */
    uint64_t required = offset + size;
    if (required > priv->capacity) {
        uint64_t new_cap = (required + 4096) & ~4095;
        uint8_t *new_data = kmalloc(new_cap);
        if (!new_data) return 0; /* OOM */
        
        memset(new_data, 0, new_cap);
        if (priv->data) {
            memcpy(new_data, priv->data, node->length);
            kfree(priv->data);
        }
        priv->data = new_data;
        priv->capacity = new_cap;
    }

    memcpy(priv->data + offset, buffer, size);
    if (required > node->length) {
        node->length = required;
    }
    
    return size;
}

static dirent_t *ramfs_readdir(vfs_node_t *node, uint32_t index) {
    if ((node->flags & FS_DIRECTORY) != FS_DIRECTORY) return NULL;
    
    ramfs_internal_t *priv = (ramfs_internal_t*)node->impl;
    if (!priv || index >= (uint32_t)priv->child_count) return NULL;
    
    vfs_node_t *child = priv->children[index];
    if (!child) return NULL;

    /* We dynamically allocate a dirent in heap to return, caller must free it. Currently naive. */
    /* Wait, standard readdir usually returns a persistent dirent pointer. We'll store it in priv to avoid leaks. */
    
    /* For simplicity in this demo, we just return a static instance. (Not thread safe) */
    static dirent_t d;
    strcpy(d.name, child->name);
    d.ino = child->inode;
    d.type = child->flags;
    return &d;
}

static vfs_node_t *ramfs_finddir(vfs_node_t *node, const char *name) {
    if ((node->flags & FS_DIRECTORY) != FS_DIRECTORY) return NULL;
    
    ramfs_internal_t *priv = (ramfs_internal_t*)node->impl;
    if (!priv) return NULL;
    
    for (int i = 0; i < priv->child_count; i++) {
        if (strcmp(priv->children[i]->name, name) == 0) {
            return priv->children[i];
        }
    }
    return NULL;
}

static vfs_node_t *ramfs_create_dir(vfs_node_t *parent, const char *name) {
    ramfs_internal_t *priv = (ramfs_internal_t*)parent->impl;
    if (priv->child_count >= RAMFS_MAX_CHILDREN) return NULL;

    vfs_node_t *node = kmalloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    strcpy(node->name, name);
    node->flags = FS_DIRECTORY;
    node->mask = parent->mask;
    node->ops = &ramfs_ops;

    ramfs_internal_t *node_priv = kmalloc(sizeof(ramfs_internal_t));
    memset(node_priv, 0, sizeof(ramfs_internal_t));
    node->impl = node_priv;

    priv->children[priv->child_count++] = node;
    return node;
}

vfs_node_t *ramfs_init(void) {
    ramfs_ops.read = ramfs_read;
    ramfs_ops.write = ramfs_write;
    ramfs_ops.open = NULL;
    ramfs_ops.close = NULL;
    ramfs_ops.readdir = ramfs_readdir;
    ramfs_ops.finddir = ramfs_finddir;
    ramfs_ops.mkdir = ramfs_create_dir;

    /* Create root node */
    vfs_node_t *root = kmalloc(sizeof(vfs_node_t));
    memset(root, 0, sizeof(vfs_node_t));
    
    strcpy(root->name, "/");
    root->flags = FS_DIRECTORY;
    root->mask = FS_PERM_R | FS_PERM_W | FS_PERM_X;
    root->ops = &ramfs_ops;
    
    ramfs_internal_t *priv = kmalloc(sizeof(ramfs_internal_t));
    memset(priv, 0, sizeof(ramfs_internal_t));
    root->impl = priv;
    
    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[VFS] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("RAMFS initialized (root `/`)\n");
    
    /* Create root hierarchy */
    ramfs_create_dir(root, "dev");
    ramfs_create_dir(root, "tmp");
    ramfs_create_dir(root, "mnt");

    return root;
}
