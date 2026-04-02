/* ============================================================================
 * partition.h — NexusOS Partition Parsers (MBR & GPT)
 * Purpose: Discover and register disk partitions from raw block devices
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_FS_PARTITION_H
#define NEXUS_FS_PARTITION_H

#include <stdint.h>
#include "drivers/storage/ata.h"

/* Standard MBR Partition Entry (16 bytes) */
typedef struct {
    uint8_t  boot_indicator;
    uint8_t  start_chs[3];
    uint8_t  os_type;        /* e.g., 0x0C = FAT32, 0x07 = NTFS/exFAT, 0x83 = Linux */
    uint8_t  end_chs[3];
    uint32_t lba_start;
    uint32_t size_in_sectors;
} __attribute__((packed)) mbr_partition_entry_t;

/* Discovered Partition Metadata */
typedef struct {
    ata_drive_t *drive;      /* Underlying physical drive */
    uint8_t  partition_index;/* 0-3 for primary MBR partitions */
    uint8_t  os_type;        /* Filesystem hint (0x0B, 0x0C = FAT32) */
    uint64_t lba_start;      /* Absolute LBA where this partition begins */
    uint64_t size_sectors;   /* Total size of partition in sectors */
} disk_partition_t;

/* Maximum partitions we'll track globally for now */
#define MAX_PARTITIONS 16

/* Parse MBR on the given ATA drive, registers any found partitions */
int partition_parse_mbr(ata_drive_t *drive);

/* Get list of discovered partitions */
disk_partition_t *partition_get_list(int *count_out);

#endif /* NEXUS_FS_PARTITION_H */
