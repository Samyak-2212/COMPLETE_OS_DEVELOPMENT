/* ============================================================================
 * fat32.h — NexusOS FAT32 Filesystem Driver
 * Purpose: Read-only access to FAT32 partitions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_FS_FAT32_H
#define NEXUS_FS_FAT32_H

#include <stdint.h>
#include "fs/vfs.h"
#include "fs/partition.h"

/* Primary Boot Record (PBR) of a FAT32 Partition */
typedef struct {
    uint8_t  jmp[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* FAT32 Extended Boot Record */
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed)) fat32_pbr_t;

/* FAT32 Directory Entry */
typedef struct {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_res;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_acc_date;
    uint16_t cluster_high;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dirent_t;

/* Initialize and mount a FAT32 filesystem from a partition */
vfs_node_t *fat32_mount(disk_partition_t *part);

#endif /* NEXUS_FS_FAT32_H */
