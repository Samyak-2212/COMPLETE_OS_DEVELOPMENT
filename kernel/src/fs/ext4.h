/* ============================================================================
 * ext4.h — NexusOS Ext4 Filesystem definitions
 * Purpose: Read-only Ext4 parser definitions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef EXT4_H
#define EXT4_H

#include "fs/vfs.h"
#include "fs/partition.h"
#include <stdint.h>

#define EXT4_SUPERBLOCK_OFFSET 1024
#define EXT4_MAGIC             0xEF53

/* ── Ext4 Superblock ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t inodes_count;
    uint32_t blocks_count_lo;
    uint32_t r_blocks_count_lo;
    uint32_t free_blocks_count_lo;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_cluster_size;
    uint32_t blocks_per_group;
    uint32_t clusters_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    
    /* EXT4_DYNAMIC_REV Specific */
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    char     volume_name[16];
    char     last_mounted[64];
    uint32_t algorithm_usage_bitmap;

    /* Padding out to 1024 bytes */
    uint8_t padding[816];
} __attribute__((packed)) ext4_superblock_t;

/* ── Block Group Descriptor ──────────────────────────────────────────────── */
typedef struct {
    uint32_t block_bitmap_lo;
    uint32_t inode_bitmap_lo;
    uint32_t inode_table_lo;
    uint16_t free_blocks_count_lo;
    uint16_t free_inodes_count_lo;
    uint16_t used_dirs_count_lo;
    uint16_t flags;
    uint32_t exclude_bitmap_lo;
    uint16_t block_bitmap_csum_lo;
    uint16_t inode_bitmap_csum_lo;
    uint16_t itable_unused_lo;
    uint16_t checksum;
    /* 64-bit feature adds more fields here, but for now we only strictly need bottom 32-bit pointers */
    uint32_t block_bitmap_hi;
    uint32_t inode_bitmap_hi;
    uint32_t inode_table_hi;
    uint16_t free_blocks_count_hi;
    uint16_t free_inodes_count_hi;
    uint16_t used_dirs_count_hi;
    uint16_t itable_unused_hi;
    uint32_t exclude_bitmap_hi;
    uint16_t block_bitmap_csum_hi;
    uint16_t inode_bitmap_csum_hi;
    uint32_t reserved;
} __attribute__((packed)) ext4_bg_desc_t;

/* ── Ext4 Inode ──────────────────────────────────────────────────────────── */
typedef struct {
    uint16_t mode;
    uint16_t uid;
    uint32_t size_lo;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks_lo;
    uint32_t flags;
    uint32_t osd1;
    uint8_t  block[60]; /* Block pointers or Extent tree */
    uint32_t generation;
    uint32_t file_acl_lo;
    uint32_t size_hi;
    uint32_t obso_faddr;
    /* Usually 256 bytes per inode, padding hereafter */
} __attribute__((packed)) ext4_inode_t;

/* ── Extent Tree ─────────────────────────────────────────────────────────── */
#define EXT4_EXT_MAGIC 0xF30A

typedef struct {
    uint16_t magic;
    uint16_t entries;
    uint16_t max;
    uint16_t depth;
    uint32_t generation;
} __attribute__((packed)) ext4_extent_header_t;

typedef struct {
    uint32_t block;   /* First logical block extent covers */
    uint32_t leaf_lo; /* Lower 32-bits of physical block of next level */
    uint16_t leaf_hi; /* Upper 16-bits */
    uint16_t unused;
} __attribute__((packed)) ext4_extent_idx_t;

typedef struct {
    uint32_t block;      /* First logical block */
    uint16_t len;        /* Number of blocks in extent */
    uint16_t start_hi;   /* Upper 16-bits of physical block */
    uint32_t start_lo;   /* Lower 32-bits of physical block */
} __attribute__((packed)) ext4_extent_t;

/* ── Directory Entry ─────────────────────────────────────────────────────── */
typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[255]; /* Variable length */
} __attribute__((packed)) ext4_dir_entry_t;

/* Public API */
vfs_node_t *ext4_mount(disk_partition_t *part);

#endif
