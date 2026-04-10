/* ============================================================================
 * vfs.h — NexusOS Virtual Filesystem Abstraction Layer
 * Purpose: Defines uniform structs for interacting with files and directories
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_FS_VFS_H
#define NEXUS_FS_VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

/* Read/Write/Execute Permissions (POSIX-style) */
#define FS_PERM_R      00400
#define FS_PERM_W      00200
#define FS_PERM_X      00100

struct vfs_node;
struct dirent;

/* Standard `struct dirent` for reading directories */
typedef struct dirent {
    char     name[256];
    uint32_t ino;         /* Inode number */
    uint32_t type;        /* FS_FILE, etc. */
} dirent_t;

/* Callback function pointers for operations on a virtual file node */
typedef struct {
    uint64_t (*read)(struct vfs_node *node, uint64_t offset, uint64_t size, uint8_t *buffer);
    uint64_t (*write)(struct vfs_node *node, uint64_t offset, uint64_t size, uint8_t *buffer);
    void (*open)(struct vfs_node *node);
    void (*close)(struct vfs_node *node);
    struct dirent *(*readdir)(struct vfs_node *node, uint32_t index);
    struct vfs_node *(*finddir)(struct vfs_node *node, const char *name);
    struct vfs_node *(*mkdir)(struct vfs_node *node, const char *name);
} vfs_ops_t;

/* The primary filesystem node struct, representing any file/folder/pipe in RAM */
typedef struct vfs_node {
    char      name[128];      /* Name of the file/directory */
    uint32_t  mask;           /* Permissions */
    uint32_t  uid;            /* User ID */
    uint32_t  gid;            /* Group ID */
    uint32_t  flags;          /* Type (file, dir, etc) */
    uint32_t  inode;          /* Inode number */
    uint64_t  length;         /* Size of the file in bytes */
    void     *impl;           /* Implementation-specific data pointer */
    
    vfs_ops_t *ops;           /* Type-specific operations (read/write/finddir) */
    
    struct vfs_node *ptr;     /* Symlink pointer or mount point */
    void      *device;        /* Pointer to an underlying physical device struct (e.g. ata_drive_t) */
} vfs_node_t;

/* Global root of the entire filesystem ("/") */
extern vfs_node_t *vfs_root;

/* VFS Function API wrappers */
uint64_t vfs_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer);
uint64_t vfs_write(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer);
void vfs_open(vfs_node_t *node);
void vfs_close(vfs_node_t *node);
dirent_t *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
vfs_node_t *vfs_resolve_path(const char *path);
int vfs_mkdir(const char *path);
int vfs_mount(const char *path, vfs_node_t *local_root);

#endif /* NEXUS_FS_VFS_H */
