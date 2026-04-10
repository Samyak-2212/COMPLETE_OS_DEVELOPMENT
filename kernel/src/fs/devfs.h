/* ============================================================================
 * devfs.h — NexusOS Device Filesystem
 * Purpose: Virtual /dev filesystem exposing kernel devices as VFS nodes
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_FS_DEVFS_H
#define NEXUS_FS_DEVFS_H

#include "fs/vfs.h"
#include "drivers/storage/ata.h"

/* Initialize devfs and mount at /dev.
 * Must be called AFTER ramfs_init() and BEFORE ata_init_subsystem(). */
void devfs_init(void);

/* Register a block device node under /dev.
 * Safe to call before devfs_init() — silently ignored if not yet ready.
 * name       : node name (e.g. "hda", "hda1", "sda")
 * drive      : pointer to the underlying ata_drive_t
 * lba_start  : first LBA of this region (0 = whole disk)
 * size_sectors: number of sectors in this region */
void devfs_register_block(const char *name, ata_drive_t *drive,
                           uint64_t lba_start, uint64_t size_sectors);

#endif /* NEXUS_FS_DEVFS_H */
