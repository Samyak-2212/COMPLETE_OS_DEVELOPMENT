# Implementation Plan — DevFS (`/dev` Virtual Filesystem)

## Goal

Implement a virtual device filesystem (`devfs`) mounted at `/dev`. It
exposes device nodes as VFS files so userspace (Phase 4+) and kernel
shell commands can interact with hardware through the VFS abstraction.
The ramfs `/dev` directory created at boot is **replaced** by the devfs
mount point.

---

## Background — What `/dev` Needs Now vs Later

| Node | Phase | Purpose |
|---|---|---|
| `/dev/null` | 3 | Write sink, read returns 0 bytes |
| `/dev/zero` | 3 | Read returns infinite zero bytes |
| `/dev/tty0` | 3 | Character device stub (read/write mapped to terminal) |
| `/dev/hda` | 3 | Block device node for first ATA drive |
| `/dev/hda1`..`hda4` | 3 | Partition block nodes |
| `/dev/sda` | 3 | Block device node for first AHCI drive |
| `/dev/sda1`..`sda4` | 3 | AHCI partition nodes |
| `/dev/fb0` | 4 | Framebuffer (Phase 6 GUI, deferred) |
| `/dev/input/event0` | 4 | Input events (Phase 4 userspace, deferred) |

Phase 3 delivers: `null`, `zero`, `tty0`, and static block device
nodes for discovered drives. Phase 4 adds `fb0` and `input/`.

---

## Architecture

DevFS is a **virtual** filesystem — no disk, no kmalloc'd child arrays.
Instead the node tree is a small **static array** of pre-registered
`devfs_node_t` entries. The VFS ops iterate this array for `readdir`
and `finddir`.

```c
/* File-private to devfs.c */
typedef struct {
    const char          *name;
    uint32_t             flags;     /* FS_FILE or FS_DIRECTORY */
    vfs_node_t          *vnode;     /* Pre-built vfs_node_t */
} devfs_entry_t;

#define DEVFS_MAX_NODES 32
static devfs_entry_t g_devfs_nodes[DEVFS_MAX_NODES];
static int           g_devfs_count = 0;
```

Each device type has its own `vfs_ops_t` with custom `read`/`write`
callbacks. The same `devfs_readdir` and `devfs_finddir` are shared
across all directory nodes.

---

## Files

### [NEW] `kernel/src/fs/devfs.h`

```c
#ifndef NEXUS_FS_DEVFS_H
#define NEXUS_FS_DEVFS_H

#include "fs/vfs.h"
#include "drivers/storage/ata.h"

/* Initialize devfs and mount at /dev */
void devfs_init(void);

/* Register a block device node (called by ATA/AHCI after drive detection) */
void devfs_register_block(const char *name, ata_drive_t *drive,
                          uint64_t lba_start, uint64_t size_sectors);

#endif
```

### [NEW] `kernel/src/fs/devfs.c`

#### `null` device ops
```c
static uint64_t null_read(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) {
    (void)n; (void)off; (void)sz; (void)buf;
    return 0;   /* EOF immediately */
}
static uint64_t null_write(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) {
    (void)n; (void)off; (void)buf;
    return sz;  /* Silently discard */
}
```

#### `zero` device ops
```c
static uint64_t zero_read(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) {
    (void)n; (void)off;
    memset(buf, 0, sz);
    return sz;
}
```

#### `tty0` device ops (maps to terminal)
```c
/* Read blocks waiting for input_poll_event; write calls kputchar */
static uint64_t tty0_read(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) { ... }
static uint64_t tty0_write(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) { ... }
```

#### Block device ops (hda, sda, etc.)
```c
/* impl = ata_drive_t*, device = partition offset */
typedef struct {
    ata_drive_t *drive;
    uint64_t     lba_start;   /* 0 for whole-disk node */
    uint64_t     size_sectors;
} devfs_block_impl_t;

static uint64_t block_read(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf) {
    devfs_block_impl_t *impl = (devfs_block_impl_t*)n->impl;
    uint64_t lba    = impl->lba_start + off / 512;
    uint8_t  count  = (sz + 511) / 512;
    return disk_read_sectors(impl->drive, lba, count, buf) ? sz : 0;
}
```

#### `devfs_init()` — bootstrap
```
1. Initialize the g_devfs_nodes[] array
2. Create the /dev root vfs_node_t (FS_DIRECTORY, devfs_readdir/finddir ops)
3. Register /dev/null  (FS_FILE, null_ops)
4. Register /dev/zero  (FS_FILE, zero_ops)
5. Register /dev/tty0  (FS_CHARDEVICE, tty0_ops)
6. Call vfs_mount("/dev", devfs_root)
```

#### `devfs_register_block(name, drive, lba_start, size_sectors)`
Called by `ata_init_subsystem()` and `ahci_init_subsystem()` after each
drive/partition is confirmed present:
```
Allocate devfs_block_impl_t
Allocate vfs_node_t
Set flags = FS_BLOCKDEVICE, ops = &block_ops, impl = impl_ptr
Append to g_devfs_nodes[]
```

---

## Integration Points

### `kernel/src/fs/devfs.c` → `kernel/src/fs/partition.c`

After `vfs_probe_and_mount()` discovers a partition, also call
`devfs_register_block()` for the partition node:

```diff
+#include "fs/devfs.h"
 void vfs_probe_and_mount(disk_partition_t *part) {
     ...
     if (fs_root) {
         vfs_mkdir(mount_path);
         vfs_mount(mount_path, fs_root);
     }
+    /* Register block device node in /dev regardless of FS type */
+    char dev_name[8];
+    strcpy(dev_name, part->drive->drive_name);
+    int len = strlen(dev_name);
+    dev_name[len] = '1' + part->partition_index;
+    dev_name[len+1] = '\0';
+    devfs_register_block(dev_name, part->drive, part->lba_start, part->size_sectors);
 }
```

### `kernel/src/kernel.c`

Add `devfs_init()` call **after** `ramfs_init()` returns and **before**
`pci_init()` / `ata_init_subsystem()`:

```diff
+#include "fs/devfs.h"
 ...
 vfs_root = ramfs_init();
+devfs_init();   /* Mounts /dev; block nodes added later by ata/ahci */
 ...
 ata_init_subsystem();   /* Will call devfs_register_block for each drive */
 ahci_init_subsystem();
 pci_init();
```

The storage drivers must call `devfs_register_block()` for the whole-disk
node (`lba_start = 0, size_sectors = drive->total_sectors`) immediately after
IDENTIFY succeeds, before partition scanning.

### `kernel/src/drivers/storage/ata.c` and `ahci.c`

After successful IDENTIFY for each drive, add:
```c
devfs_register_block(drive.drive_name, &drive, 0, drive.total_sectors);
```

This requires adding `#include "fs/devfs.h"` to both files.

---

## VFS Node Creation Helper

To avoid repetition, a private `devfs_make_node()` helper:

```c
static vfs_node_t *devfs_make_node(const char *name, uint32_t flags,
                                    vfs_ops_t *ops, void *impl) {
    vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
    memset(n, 0, sizeof(vfs_node_t));
    strncpy(n->name, name, 127);
    n->flags = flags;
    n->ops   = ops;
    n->impl  = impl;
    return n;
}
```

---

## Ordering Constraint

```
ramfs_init()          ← creates /dev as empty ramfs directory
devfs_init()          ← calls vfs_mount("/dev", devfs_root)
                         /dev is now a devfs mount point
                         /dev/null, /dev/zero, /dev/tty0 visible
ata_init_subsystem()  ← calls devfs_register_block("hda", ...)
ahci_init_subsystem() ← calls devfs_register_block("sda", ...)
pci_init()            ← triggers partition_parse_mbr/gpt
partition discovery   ← calls devfs_register_block("hda1", ...)
```

Calling `devfs_register_block` before `devfs_init()` would crash (NULL
array pointer). The ordering in `kernel.c` enforces this correctly.

---

## Verification Plan

### Automated Test
```bash
make clean && make all   # 0 errors, 0 warnings
make run
```

### Manual Verification in Shell
```
ls /dev
  → null  zero  tty0  hda  hda1

cat /dev/null
  → (prints nothing, immediate exit)

echo thisdisappears > /dev/null
  → (silent, no error)

# Reads 16 zero bytes from /dev/zero:
cat /dev/zero
  → (terminal would fill with nulls — verifiable via serial log)

ls /dev
  → hda, hda1 (if QEMU has a secondary disk with -hdb)
```

### Build Verification
- No circular includes: `devfs.h` includes `vfs.h` and `ata.h` only.
  `ata.c` includes `devfs.h`. `partition.c` includes `devfs.h`. No cycles.
- `__attribute__((packed))` not needed — devfs has no hardware structs.
