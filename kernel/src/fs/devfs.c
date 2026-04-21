/* ============================================================================
 * devfs.c — NexusOS Device Filesystem
 * Purpose: Virtual /dev filesystem — exposes null, zero, tty0, and block nodes
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/devfs.h"
#include "fs/vfs.h"
#include "fs/partition.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "drivers/input/input_event.h"

/* Forward declaration for input_poll_event */
extern int input_poll_event(input_event_t *out);

/* ── Static node registry ─────────────────────────────────────────────────── */

#define DEVFS_MAX_NODES 32

typedef struct {
    char        name[32];
    vfs_node_t *vnode;
} devfs_entry_t;

static devfs_entry_t g_devfs_nodes[DEVFS_MAX_NODES];
static int           g_devfs_count   = 0;
static int           g_devfs_ready   = 0;  /* Guard: set after devfs_init() */
static vfs_node_t   *g_devfs_root    = NULL;

/* ── Shared static dirent (single-threaded kernel — safe) ─────────────────── */
static dirent_t g_devfs_dirent;

/* ── Internal helper: allocate and populate a vfs_node_t ─────────────────── */

static vfs_node_t *devfs_make_node(const char *name, uint32_t flags,
                                    vfs_ops_t *ops, void *impl)
{
    vfs_node_t *n = kmalloc(sizeof(vfs_node_t));
    if (!n) return NULL;
    memset(n, 0, sizeof(vfs_node_t));
    strncpy(n->name, name, 127);
    n->flags = flags;
    n->mask  = FS_PERM_R | FS_PERM_W;
    n->ops   = ops;
    n->impl  = impl;
    return n;
}

/* ── /dev/null ops ────────────────────────────────────────────────────────── */

static uint64_t null_read(vfs_node_t *n, uint64_t off, uint64_t sz,
                           uint8_t *buf)
{
    (void)n; (void)off; (void)sz; (void)buf;
    return 0; /* EOF immediately */
}

static uint64_t null_write(vfs_node_t *n, uint64_t off, uint64_t sz,
                            uint8_t *buf)
{
    (void)n; (void)off; (void)buf;
    return sz; /* Silently discard */
}

static vfs_ops_t null_ops = {
    .read    = null_read,
    .write   = null_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
    .mkdir   = NULL,
};

/* ── /dev/zero ops ────────────────────────────────────────────────────────── */

static uint64_t zero_read(vfs_node_t *n, uint64_t off, uint64_t sz,
                           uint8_t *buf)
{
    (void)n; (void)off;
    memset(buf, 0, sz);
    return sz;
}

static uint64_t zero_write(vfs_node_t *n, uint64_t off, uint64_t sz,
                            uint8_t *buf)
{
    (void)n; (void)off; (void)buf;
    return sz; /* Discard writes */
}

static vfs_ops_t zero_ops = {
    .read    = zero_read,
    .write   = zero_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
    .mkdir   = NULL,
};

/* ── /dev/tty0 ops ────────────────────────────────────────────────────────── */

/* Write: emit each byte to the kernel console */
static uint64_t tty0_write(vfs_node_t *n, uint64_t off, uint64_t sz,
                             uint8_t *buf)
{
    (void)n; (void)off;
    for (uint64_t i = 0; i < sz; i++) {
        kputchar((char)buf[i]);
    }
    return sz;
}

#include "sched/scheduler.h"

/* Read: reads input_poll_event yielding to the scheduler if no events arrive.
 * Implements basic line discipline: echoes characters and handles backspace.
 * Returns only when a newline is encountered or buffer is full. */
static uint64_t tty0_read(vfs_node_t *n, uint64_t off, uint64_t sz, uint8_t *buf)
{
    (void)n; (void)off;
    uint64_t filled = 0;

    while (filled < sz) {
        input_event_t ev;
        if (input_poll_event(&ev)) {
            if (ev.type == INPUT_EVENT_KEY_PRESS && ev.ascii != 0) {
                if (ev.ascii == '\b' || ev.ascii == KEY_DELETE) {
                    if (filled > 0) {
                        filled--;
                        kprintf("\b \b");
                    }
                } else if (ev.ascii == '\n') {
                    buf[filled++] = '\n';
                    kprintf("\n");
                    break;
                } else if (ev.ascii >= 32 && ev.ascii <= 126) {
                    buf[filled++] = (uint8_t)ev.ascii;
                    kprintf("%c", ev.ascii);
                }
            }
        } else {
            schedule();
        }
    }

    return filled;
}

static vfs_ops_t tty0_ops = {
    .read    = tty0_read,
    .write   = tty0_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
    .mkdir   = NULL,
};

/* ── Block device ops ─────────────────────────────────────────────────────── */

/* Per-node implementation descriptor (heap-allocated) */
typedef struct {
    ata_drive_t *drive;
    uint64_t     lba_start;    /* Absolute LBA onset (0 = whole disk) */
    uint64_t     size_sectors; /* Region size in 512-byte sectors */
} devfs_block_impl_t;

static uint64_t block_read(vfs_node_t *n, uint64_t off, uint64_t sz,
                             uint8_t *buf)
{
    devfs_block_impl_t *impl = (devfs_block_impl_t *)n->impl;
    if (!impl || !impl->drive || !impl->drive->present) return 0;
    if (sz == 0) return 0;

    uint64_t lba   = impl->lba_start + (off / 512);
    uint8_t  count = (uint8_t)((sz + 511) / 512);

    /* Clamp to region bounds */
    if ((lba - impl->lba_start) + count > impl->size_sectors) return 0;

    return disk_read_sectors(impl->drive, lba, count, buf) ? sz : 0;
}

static uint64_t block_write(vfs_node_t *n, uint64_t off, uint64_t sz,
                              uint8_t *buf)
{
    /* Write not implemented for Phase 3 block nodes */
    (void)n; (void)off; (void)sz; (void)buf;
    return 0;
}

static vfs_ops_t block_ops = {
    .read    = block_read,
    .write   = block_write,
    .open    = NULL,
    .close   = NULL,
    .readdir = NULL,
    .finddir = NULL,
    .mkdir   = NULL,
};

/* ── Devfs root directory ops ─────────────────────────────────────────────── */

/* readdir: iterate g_devfs_nodes[] */
static dirent_t *devfs_readdir(vfs_node_t *node, uint32_t index)
{
    (void)node;
    if ((int)index >= g_devfs_count) return NULL;

    vfs_node_t *child = g_devfs_nodes[index].vnode;
    if (!child) return NULL;

    strcpy(g_devfs_dirent.name, child->name);
    g_devfs_dirent.ino  = child->inode;
    g_devfs_dirent.type = child->flags;
    return &g_devfs_dirent;
}

/* finddir: linear search through g_devfs_nodes[] */
static vfs_node_t *devfs_finddir(vfs_node_t *node, const char *name)
{
    (void)node;
    for (int i = 0; i < g_devfs_count; i++) {
        if (strcmp(g_devfs_nodes[i].name, name) == 0) {
            return g_devfs_nodes[i].vnode;
        }
    }
    return NULL;
}

static vfs_ops_t devfs_dir_ops = {
    .read    = NULL,
    .write   = NULL,
    .open    = NULL,
    .close   = NULL,
    .readdir = devfs_readdir,
    .finddir = devfs_finddir,
    .mkdir   = NULL,
};

/* ── Internal: append a pre-built vfs_node_t to the registry ─────────────── */

static void devfs_append(const char *name, vfs_node_t *vnode)
{
    if (g_devfs_count >= DEVFS_MAX_NODES) return;
    strncpy(g_devfs_nodes[g_devfs_count].name, name, 31);
    g_devfs_nodes[g_devfs_count].vnode = vnode;
    g_devfs_count++;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialize devfs: build root node, register standard devices, mount at /dev */
void devfs_init(void)
{
    /* Build the /dev root directory node */
    g_devfs_root = kmalloc(sizeof(vfs_node_t));
    if (!g_devfs_root) return;
    memset(g_devfs_root, 0, sizeof(vfs_node_t));
    strcpy(g_devfs_root->name, "dev");
    g_devfs_root->flags = FS_DIRECTORY;
    g_devfs_root->mask  = FS_PERM_R | FS_PERM_W | FS_PERM_X;
    g_devfs_root->ops   = &devfs_dir_ops;
    g_devfs_root->impl  = NULL; /* No heap array — uses g_devfs_nodes[] */

    /* Register /dev/null */
    vfs_node_t *null_node = devfs_make_node("null", FS_FILE, &null_ops, NULL);
    if (null_node) devfs_append("null", null_node);

    /* Register /dev/zero */
    vfs_node_t *zero_node = devfs_make_node("zero", FS_FILE, &zero_ops, NULL);
    if (zero_node) devfs_append("zero", zero_node);

    /* Register /dev/tty0 */
    vfs_node_t *tty0_node = devfs_make_node("tty0", FS_CHARDEVICE,
                                              &tty0_ops, NULL);
    if (tty0_node) devfs_append("tty0", tty0_node);

    /* Mount devfs at /dev (replaces the ramfs stub directory) */
    if (vfs_mount("/dev", g_devfs_root) != 0) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[DevFS] ERROR: vfs_mount(\"/dev\") failed\n");
        return;
    }

    /* Mark ready AFTER mount so register_block guards work correctly */
    g_devfs_ready = 1;

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[DevFS] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("Mounted at /dev (null, zero, tty0 ready)\n");
}

/* Register a block device node in /dev.
 * Silently returns if devfs_init() has not yet been called. */
void devfs_register_block(const char *name, ata_drive_t *drive,
                           uint64_t lba_start, uint64_t size_sectors)
{
    if (!g_devfs_ready) return; /* Guard: devfs not yet initialized */
    if (g_devfs_count >= DEVFS_MAX_NODES) return;

    devfs_block_impl_t *impl = kmalloc(sizeof(devfs_block_impl_t));
    if (!impl) return;
    impl->drive        = drive;
    impl->lba_start    = lba_start;
    impl->size_sectors = size_sectors;

    vfs_node_t *bnode = devfs_make_node(name, FS_BLOCKDEVICE,
                                         &block_ops, impl);
    if (!bnode) {
        kfree(impl);
        return;
    }
    bnode->length = size_sectors * 512;

    devfs_append(name, bnode);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[DevFS] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("/dev/%s registered (%llu sectors)\n",
            name, (unsigned long long)size_sectors);
}
