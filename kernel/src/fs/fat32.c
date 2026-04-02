/* ============================================================================
 * fat32.c — NexusOS FAT32 Filesystem Driver
 * Purpose: Mounting, traversing, and reading FAT32 partitions (Read Only)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/fat32.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "mm/heap.h"

/* Basic fat32 instance data */
typedef struct {
    disk_partition_t *part;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
} fat32_fs_t;

/* Node-specific local data */
typedef struct {
    fat32_fs_t *fs;
    uint32_t first_cluster;
    uint32_t file_size;
} fat32_node_data_t;

static vfs_ops_t fat32_ops;

/* Helper to log internal info */
static void trim_spaces(char *str, int max_len) {
    for (int i = max_len - 1; i >= 0; i--) {
        if (str[i] == ' ') str[i] = 0;
        else break;
    }
}

/* Parse an 8.3 filename */
static void parse_fat32_name(char *dest, const char *src) {
    int d = 0;
    for (int i = 0; i < 8 && src[i] != ' '; i++) {
        dest[d++] = src[i];
    }
    if (src[8] != ' ') {
        dest[d++] = '.';
        for (int i = 8; i < 11 && src[i] != ' '; i++) {
            dest[d++] = src[i];
        }
    }
    dest[d] = 0;
}

static dirent_t *fat32_readdir(vfs_node_t *node, uint32_t index) {
    fat32_node_data_t *n_data = (fat32_node_data_t*)node->impl;
    if (!n_data) return NULL;
    fat32_fs_t *fs = n_data->fs;

    /* For simplicity, we just safely read the first cluster of the directory.
     * Full FAT32 traversals would chase the cluster chain. */
    uint32_t cluster_lba = fs->data_lba + (n_data->first_cluster - 2) * fs->sectors_per_cluster;
    
    uint8_t *cluster_buf = kmalloc(fs->bytes_per_cluster);
    if (ata_read_sectors(fs->part->drive, cluster_lba, fs->sectors_per_cluster, cluster_buf) == 0) {
        kfree(cluster_buf);
        return NULL;
    }

    fat32_dirent_t *entries = (fat32_dirent_t*)cluster_buf;
    uint32_t max_entries = fs->bytes_per_cluster / sizeof(fat32_dirent_t);
    
    uint32_t valid_count = 0;
    static dirent_t d_out;
    
    for (uint32_t i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break; /* End of directory */
        if (entries[i].name[0] == (char)0xE5) continue; /* Deleted entry */
        if (entries[i].attr == 0x0F) continue; /* LFN entry (we skip Long File Names for now) */
        if (entries[i].attr & 0x08) continue; /* Volume label */
        
        if (valid_count == index) {
            parse_fat32_name(d_out.name, entries[i].name);
            d_out.ino = entries[i].cluster_low | (entries[i].cluster_high << 16);
            d_out.type = (entries[i].attr & 0x10) ? FS_DIRECTORY : FS_FILE;
            kfree(cluster_buf);
            return &d_out;
        }
        valid_count++;
    }

    kfree(cluster_buf);
    return NULL;
}

static vfs_node_t *fat32_finddir(vfs_node_t *node, const char *name) {
    fat32_node_data_t *n_data = (fat32_node_data_t*)node->impl;
    if (!n_data) return NULL;
    fat32_fs_t *fs = n_data->fs;

    uint32_t cluster_lba = fs->data_lba + (n_data->first_cluster - 2) * fs->sectors_per_cluster;
    
    uint8_t *cluster_buf = kmalloc(fs->bytes_per_cluster);
    if (ata_read_sectors(fs->part->drive, cluster_lba, fs->sectors_per_cluster, cluster_buf) == 0) {
        kfree(cluster_buf);
        return NULL;
    }

    fat32_dirent_t *entries = (fat32_dirent_t*)cluster_buf;
    uint32_t max_entries = fs->bytes_per_cluster / sizeof(fat32_dirent_t);
    
    for (uint32_t i = 0; i < max_entries; i++) {
        if (entries[i].name[0] == 0x00) break;
        if (entries[i].name[0] == (char)0xE5) continue;
        if (entries[i].attr == 0x0F) continue;
        if (entries[i].attr & 0x08) continue;
        
        char parsed_name[14];
        parse_fat32_name(parsed_name, entries[i].name);
        
        if (strcmp(parsed_name, name) == 0) {
            vfs_node_t *child = kmalloc(sizeof(vfs_node_t));
            memset(child, 0, sizeof(vfs_node_t));
            strcpy(child->name, parsed_name);
            child->flags = (entries[i].attr & 0x10) ? FS_DIRECTORY : FS_FILE;
            child->length = entries[i].file_size;
            child->ops = &fat32_ops;
            
            fat32_node_data_t *c_data = kmalloc(sizeof(fat32_node_data_t));
            c_data->fs = fs;
            c_data->first_cluster = entries[i].cluster_low | (entries[i].cluster_high << 16);
            c_data->file_size = entries[i].file_size;
            child->impl = c_data;
            
            kfree(cluster_buf);
            return child;
        }
    }

    kfree(cluster_buf);
    return NULL;
}

static uint64_t fat32_read(vfs_node_t *node, uint64_t offset, uint64_t size, uint8_t *buffer) {
    if ((node->flags & FS_FILE) != FS_FILE) return 0;
    
    fat32_node_data_t *n_data = (fat32_node_data_t*)node->impl;
    fat32_fs_t *fs = n_data->fs;

    if (offset >= n_data->file_size) return 0;
    if (offset + size > n_data->file_size) {
        size = n_data->file_size - offset;
    }

    /* Naive read: we just read the first cluster (sufficient for small text files like a hello.txt) */
    /* Real implementations must walk the FAT tables */
    uint32_t cluster_lba = fs->data_lba + (n_data->first_cluster - 2) * fs->sectors_per_cluster;
    
    uint8_t *cluster_buf = kmalloc(fs->bytes_per_cluster);
    ata_read_sectors(fs->part->drive, cluster_lba, fs->sectors_per_cluster, cluster_buf);
    
    memcpy(buffer, cluster_buf + offset, size);
    kfree(cluster_buf);
    
    return size;
}

vfs_node_t *fat32_mount(disk_partition_t *part) {
    uint8_t *sector = kmalloc(512);
    if (ata_read_sectors(part->drive, part->lba_start, 1, sector) == 0) {
        kfree(sector);
        return NULL;
    }

    fat32_pbr_t *pbr = (fat32_pbr_t*)sector;
    if (pbr->jmp[0] != 0xEB && pbr->jmp[0] != 0xE9) {
        kfree(sector);
        return NULL; /* Bad signature */
    }

    fat32_fs_t *fs = kmalloc(sizeof(fat32_fs_t));
    fs->part = part;
    fs->fat_lba = part->lba_start + pbr->reserved_sectors;
    fs->data_lba = fs->fat_lba + (pbr->fat_count * pbr->sectors_per_fat_32);
    fs->root_cluster = pbr->root_cluster;
    fs->sectors_per_cluster = pbr->sectors_per_cluster;
    fs->bytes_per_cluster = pbr->sectors_per_cluster * pbr->bytes_per_sector;

    char label[12];
    strncpy(label, pbr->volume_label, 11);
    label[11] = 0;
    trim_spaces(label, 11);

    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[FAT32] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Mounted VOLUME '%s' (Cluster Size: %u bytes)\n", label, fs->bytes_per_cluster);

    fat32_ops.read = fat32_read;
    fat32_ops.write = NULL; /* Read only */
    fat32_ops.readdir = fat32_readdir;
    fat32_ops.finddir = fat32_finddir;

    vfs_node_t *root = kmalloc(sizeof(vfs_node_t));
    memset(root, 0, sizeof(vfs_node_t));
    strcpy(root->name, label);
    root->flags = FS_DIRECTORY;
    root->ops = &fat32_ops;
    
    fat32_node_data_t *n_data = kmalloc(sizeof(fat32_node_data_t));
    n_data->fs = fs;
    n_data->first_cluster = fs->root_cluster;
    n_data->file_size = 0;
    root->impl = n_data;

    kfree(sector);
    return root;
}
