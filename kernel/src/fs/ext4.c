/* ============================================================================
 * ext4.c — NexusOS Ext4 Filesystem Driver
 * Purpose: Read-only mounting, directory traversal, and extent reading
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/ext4.h"
#include "fs/partition.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"

#define EXT4_DEBUG 1

/* Internal Ext4 Mount Info */
typedef struct {
    disk_partition_t *part;
    ext4_superblock_t sb;
    uint32_t block_size;
    uint32_t inodes_per_group;
    uint32_t blocks_per_group;
    uint16_t inode_size;
    uint64_t bg_desc_table_lba; /* LBA of the Block Group Descriptor Table */
} ext4_info_t;

/* Node Info */
typedef struct {
    ext4_info_t *fs;
    uint32_t inode_num;
    ext4_inode_t inode;
} ext4_node_info_t;

/* Forward declarations */
static uint64_t ext4_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer);
static vfs_node_t *ext4_finddir(vfs_node_t *node, const char *name);

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/* Read a physical ext4 block (which could be bigger than an ATA sector) */
static int ext4_read_block(ext4_info_t *fs, uint64_t block_num, uint8_t *buffer) {
    uint64_t lba = fs->part->lba_start + (block_num * (fs->block_size / 512));
    uint32_t sector_count = fs->block_size / 512;
    return ata_read_sectors(fs->part->drive, lba, sector_count, buffer);
}

/* Fetch an inode off the disk */
static int ext4_read_inode(ext4_info_t *fs, uint32_t inode_num, ext4_inode_t *inode_out) {
    if (inode_num == 0) return 0;
    
    uint32_t group = (inode_num - 1) / fs->inodes_per_group;
    uint32_t index = (inode_num - 1) % fs->inodes_per_group;

    /* Calculate where the BGD is for this group */
    uint32_t desc_size = 32; /* Assume 32-byte descriptor for basic 32-bit features */
    if (fs->sb.rev_level > 0 && (fs->sb.feature_incompat & 0x80)) {
        /* 64-bit feature */
        desc_size = 64; 
    }

    uint32_t descriptors_per_block = fs->block_size / desc_size;
    uint32_t bgd_block_offset = group / descriptors_per_block;
    uint32_t bgd_index_in_block = group % descriptors_per_block;

    uint8_t *block = kmalloc(fs->block_size);
    if (!block) return 0;

    /* BGDT usually starts right after Superblock. Superblock is at 1024 bytes. */
    uint64_t bgd_start_block = (fs->block_size == 1024) ? 2 : 1;
    
    if (!ext4_read_block(fs, bgd_start_block + bgd_block_offset, block)) {
        kfree(block);
        return 0;
    }

    ext4_bg_desc_t *bgd = (ext4_bg_desc_t *)(block + (bgd_index_in_block * desc_size));
    uint32_t inode_table_block = bgd->inode_table_lo;

    /* Determine block containing the inode */
    uint32_t block_index = (index * fs->inode_size) / fs->block_size;
    uint32_t offset_in_block = (index * fs->inode_size) % fs->block_size;

    if (!ext4_read_block(fs, inode_table_block + block_index, block)) {
        kfree(block);
        return 0;
    }

    memcpy(inode_out, block + offset_in_block, sizeof(ext4_inode_t));
    kfree(block);
    return 1;
}

/* Extent Tree Walker: Given logical block 'l_block', find physical block */
static uint64_t ext4_get_physical_block(ext4_info_t *fs, ext4_inode_t *inode, uint32_t l_block) {
    if ((inode->flags & 0x80000) == 0) {
        /* Not using extents! Fallback to Ext2 linear blocks */
        if (l_block < 12) {
            uint32_t *blocks = (uint32_t *)inode->block;
            return blocks[l_block];
        }
        /* Indirect blocks not supported in this basic Ext2 fallback yet */
        return 0;
    }

    ext4_extent_header_t *eh = (ext4_extent_header_t *)inode->block;
    if (eh->magic != EXT4_EXT_MAGIC) return 0;
    
    uint8_t *extent_buffer = kmalloc(fs->block_size);

    while (eh->depth > 0) {
        /* Find internal node */
        ext4_extent_idx_t *ix = (ext4_extent_idx_t *)((uint8_t *)eh + sizeof(ext4_extent_header_t));
        uint64_t target_phy = 0;
        for (int i = 0; i < eh->entries; i++) {
            if (l_block >= ix[i].block) {
                target_phy = ((uint64_t)ix[i].leaf_hi << 32) | ix[i].leaf_lo;
            }
        }
        if (target_phy == 0) goto out;

        if (!ext4_read_block(fs, target_phy, extent_buffer)) goto out;
        eh = (ext4_extent_header_t *)extent_buffer;
        if (eh->magic != EXT4_EXT_MAGIC) goto out;
    }

    /* We are at leaf node */
    ext4_extent_t *ex = (ext4_extent_t *)((uint8_t *)eh + sizeof(ext4_extent_header_t));
    uint64_t result_phy = 0;
    for (int i = 0; i < eh->entries; i++) {
        if (l_block >= ex[i].block && l_block < ex[i].block + ex[i].len) {
            uint64_t start_phy = ((uint64_t)ex[i].start_hi << 32) | ex[i].start_lo;
            result_phy = start_phy + (l_block - ex[i].block);
            break;
        }
    }

out:
    kfree(extent_buffer);
    return result_phy;
}

/* ── VFS Hooks ───────────────────────────────────────────────────────────── */

static uint64_t ext4_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    ext4_node_info_t *ninfo = (ext4_node_info_t *)node->device;
    ext4_info_t *fs = ninfo->fs;

    if (offset >= ninfo->inode.size_lo) return 0;
    if (offset + size > ninfo->inode.size_lo) {
        size = ninfo->inode.size_lo - offset;
    }

    uint64_t bytes_read = 0;
    uint8_t *block_buf = kmalloc(fs->block_size);
    if (!block_buf) return 0;

    while (bytes_read < size) {
        uint32_t l_block = (offset + bytes_read) / fs->block_size;
        uint32_t block_offset = (offset + bytes_read) % fs->block_size;
        
        uint64_t p_block = ext4_get_physical_block(fs, &ninfo->inode, l_block);
        if (p_block == 0) {
            memset(block_buf, 0, fs->block_size); /* Sparse or hole */
        } else {
            if (!ext4_read_block(fs, p_block, block_buf)) break;
        }

        uint32_t to_copy = fs->block_size - block_offset;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;

        memcpy(buffer + bytes_read, block_buf + block_offset, to_copy);
        bytes_read += to_copy;
    }

    kfree(block_buf);
    return bytes_read;
}

static vfs_node_t *ext4_finddir(vfs_node_t *node, const char *name) {
    ext4_node_info_t *ninfo = (ext4_node_info_t *)node->device;
    ext4_info_t *fs = ninfo->fs;

    if ((ninfo->inode.mode & 0xF000) != 0x4000) return NULL; /* Not a directory */

    uint8_t *dir_buf = kmalloc(fs->block_size);
    if (!dir_buf) return NULL;

    uint32_t dir_blocks = ninfo->inode.size_lo / fs->block_size;
    if (dir_blocks == 0 && ninfo->inode.size_lo > 0) dir_blocks = 1;

    for (uint32_t i = 0; i < dir_blocks; i++) {
        uint64_t p_block = ext4_get_physical_block(fs, &ninfo->inode, i);
        if (p_block == 0) break;
        if (!ext4_read_block(fs, p_block, dir_buf)) break;

        uint32_t offset = 0;
        while (offset < fs->block_size) {
            ext4_dir_entry_t *entry = (ext4_dir_entry_t *)(dir_buf + offset);
            if (entry->inode == 0 || entry->rec_len == 0) break;

            if (strlen(name) == entry->name_len && strncmp(entry->name, name, entry->name_len) == 0) {
                /* Found target! Build dirent and child node */
                vfs_node_t *child = kmalloc(sizeof(vfs_node_t));
                memset(child, 0, sizeof(vfs_node_t));
                strncpy(child->name, name, sizeof(child->name) - 1);
                
                ext4_node_info_t *cinfo = kmalloc(sizeof(ext4_node_info_t));
                cinfo->fs = fs;
                cinfo->inode_num = entry->inode;
                ext4_read_inode(fs, cinfo->inode_num, &cinfo->inode);
                
                child->device = cinfo;
                
                vfs_ops_t *ops = kmalloc(sizeof(vfs_ops_t));
                memset(ops, 0, sizeof(vfs_ops_t));
                ops->read = ext4_read;
                ops->finddir = ext4_finddir;
                child->ops = ops;
                
                int is_dir = (cinfo->inode.mode & 0xF000) == 0x4000;
                child->flags = is_dir ? FS_DIRECTORY : FS_FILE;
                child->length = cinfo->inode.size_lo;

                kfree(dir_buf);
                return child;
            }
            offset += entry->rec_len;
        }
    }

    kfree(dir_buf);
    return NULL;
}

/* ── Mount ───────────────────────────────────────────────────────────────── */

vfs_node_t *ext4_mount(disk_partition_t *part) {
    if (!part) return NULL;

    /* Read Superblock (starts at offset 1024) */
    uint8_t buffer[2048];
    if (!ata_read_sectors(part->drive, part->lba_start + 2, 2, buffer)) return NULL;

    ext4_superblock_t *sb = (ext4_superblock_t *)(buffer + 0); /* 1024 bytes into partition is sector 2, offset 0 */
    if (sb->magic != EXT4_MAGIC) {
        return NULL;
    }

    ext4_info_t *fs = kmalloc(sizeof(ext4_info_t));
    if (!fs) return NULL;

    fs->part = part;
    fs->sb = *sb;
    fs->block_size = 1024 << sb->log_block_size;
    fs->inodes_per_group = sb->inodes_per_group;
    fs->blocks_per_group = sb->blocks_per_group;
    fs->inode_size = (sb->rev_level == 0) ? 128 : sb->inode_size;

    /* Create the Root Node (Inode 2) */
    vfs_node_t *root = kmalloc(sizeof(vfs_node_t));
    memset(root, 0, sizeof(vfs_node_t));
    strncpy(root->name, "ext4_root", sizeof(root->name) - 1);
    root->flags = FS_DIRECTORY;
    
    vfs_ops_t *ops = kmalloc(sizeof(vfs_ops_t));
    memset(ops, 0, sizeof(vfs_ops_t));
    ops->read = ext4_read;
    ops->finddir = ext4_finddir;
    root->ops = ops;

    ext4_node_info_t *ninfo = kmalloc(sizeof(ext4_node_info_t));
    ninfo->fs = fs;
    ninfo->inode_num = 2; /* Root is always inode 2 */
    
    if (!ext4_read_inode(fs, 2, &ninfo->inode)) {
        kfree(fs);
        kfree(root);
        kfree(ninfo);
        return NULL;
    }

    root->device = ninfo;
    root->length = ninfo->inode.size_lo;

    /* Success! Get volume name */
    char label[17] = {0};
    strncpy(label, sb->volume_name, 16);
    if (label[0] == 0) strncpy(label, "EXT4_VOL", 8);

    kprintf_set_color(0x00FF88FF, 0x001A1A2E);
    kprintf("[EXT4] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Mounted VOLUME '%s' (Block Size: %u bytes)\n", label, fs->block_size);

    return root;
}
