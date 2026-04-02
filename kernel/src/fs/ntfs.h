/* ============================================================================
 * ntfs.h — NexusOS NTFS Filesystem Driver
 * Purpose: Structures and definitions for read-only NTFS parsing
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef FS_NTFS_H
#define FS_NTFS_H

#include "fs/vfs.h"
#include "fs/partition.h"

/* Magic */
#define NTFS_MAGIC 0x205346544EULL // "NTFS    "

/* Attributes */
#define NTFS_ATTR_STANDARD_INFO 0x10
#define NTFS_ATTR_ATTRIBUTE_LIST 0x20
#define NTFS_ATTR_FILE_NAME 0x30
#define NTFS_ATTR_DATA 0x80
#define NTFS_ATTR_INDEX_ROOT 0x90
#define NTFS_ATTR_INDEX_ALLOCATION 0xA0
#define NTFS_ATTR_BITMAP 0xB0

/* MFT Record Flags */
#define NTFS_RECORD_IN_USE 0x01
#define NTFS_RECORD_IS_DIR 0x02

#pragma pack(push, 1)

/* Bios Parameter Block */
typedef struct {
    uint8_t jump[3];
    uint64_t oem_id;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t media_descriptor;
    uint16_t unused1;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t unused2;
    uint32_t unused3;
    uint64_t total_sectors;
    uint64_t mft_cluster;
    uint64_t mft_mirr_cluster;
    int8_t clusters_per_mft_record;
    uint8_t unused4[3];
    int8_t clusters_per_index_buffer;
    uint8_t unused5[3];
    uint64_t serial_number;
    uint32_t checksum;
} ntfs_bpb_t;

/* MFT Record Header */
typedef struct {
    uint32_t magic;          // "FILE"
    uint16_t update_seq_offset;
    uint16_t update_seq_size;
    uint64_t logfile_sn;
    uint16_t seq_number;
    uint16_t hard_link_count;
    uint16_t attrs_offset;
    uint16_t flags;
    uint32_t real_size;
    uint32_t alloc_size;
    uint64_t base_record;
    uint16_t next_attr_id;
    uint16_t reserved;
    uint32_t mft_record_number;
} ntfs_mft_record_t;

/* Generic Attribute Header */
typedef struct {
    uint32_t type;
    uint32_t length;
    uint8_t non_resident;
    uint8_t name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attr_id;
} ntfs_attr_header_t;

/* Resident Attribute */
typedef struct {
    ntfs_attr_header_t header;
    uint32_t value_length;
    uint16_t value_offset;
    uint16_t indexed_flag;
} ntfs_resident_attr_t;

/* Non-Resident Attribute */
typedef struct {
    ntfs_attr_header_t header;
    uint64_t starting_vcn;
    uint64_t last_vcn;
    uint16_t data_runs_offset;
    uint16_t comp_unit_size;
    uint32_t padding;
    uint64_t alloc_size;
    uint64_t real_size;
    uint64_t init_size;
} ntfs_non_resident_attr_t;

/* Standard Information Attribute */
typedef struct {
    uint64_t creation_time;
    uint64_t altered_time;
    uint64_t mft_changed_time;
    uint64_t read_time;
    uint32_t file_attributes;
    uint32_t max_versions;
    uint32_t version_number;
    uint32_t class_id;
} ntfs_attr_std_info_t;

/* File Name Attribute */
typedef struct {
    uint64_t parent_dir_ref;
    uint64_t creation_time;
    uint64_t altered_time;
    uint64_t mft_changed_time;
    uint64_t read_time;
    uint64_t alloc_size;
    uint64_t real_size;
    uint32_t flags;
    uint32_t er_info;
    uint8_t name_length;
    uint8_t name_type;
    uint16_t name[1]; // UTF-16
} ntfs_attr_file_name_t;

#pragma pack(pop)

/* VFS Hook */
vfs_node_t *ntfs_mount(disk_partition_t *part);

#endif
