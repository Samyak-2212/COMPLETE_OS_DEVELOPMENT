/* ============================================================================
 * partition.c — NexusOS Partition Parsers
 * Purpose: Reads sector 0 to map MBR partitions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/partition.h"
#include "lib/printf.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include "fs/fat32.h"
#include "fs/ext4.h"
#include "fs/devfs.h"
#include "fs/ntfs.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

extern int ahci_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer);

static disk_partition_t g_partitions[MAX_PARTITIONS];
static int g_partition_count = 0;

/* Dispatcher for reading sectors from various disk types */
int disk_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer) {
    if (drive->type == DISK_TYPE_ATA) {
        return ata_read_sectors(drive, lba, count, buffer);
    } else if (drive->type == DISK_TYPE_SATA) {
        return ahci_read_sectors(drive, lba, count, buffer);
    }
    return 0;
}

int partition_parse_mbr(ata_drive_t *drive) {
    if (!drive || !drive->present) return -1;
    
    uint8_t *sector = kmalloc(4096);
    memset(sector, 0, 4096);
    
    if (disk_read_sectors(drive, 0, 1, sector) == 0) {
        kfree(sector);
        return -1;
    }
    
    /* Check MBR signature 0x55AA at the end */
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        kprintf("       [PART] Warning: Invalid MBR signature on %s (0x%02x%02x)\n", 
                drive->drive_name, sector[511], sector[510]);

        kfree(sector);
        return 0; 
    }
    
    /* MBR partitions start at offset 0x1BE */
    mbr_partition_entry_t *entries = (mbr_partition_entry_t*)(sector + 0x1BE);
    
    int found = 0;
    for (int i = 0; i < 4; i++) {
        /* If os_type is 0, it's an empty entry */
        if (entries[i].os_type == 0x00) continue;
        
        if (g_partition_count >= MAX_PARTITIONS) break;
        
        disk_partition_t *p = &g_partitions[g_partition_count++];
        p->drive = drive;
        p->partition_index = i;
        p->os_type = entries[i].os_type;
        p->lba_start = entries[i].lba_start;
        p->size_sectors = entries[i].size_in_sectors;
        
        kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
        kprintf("[PART] ");
        kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
        kprintf("Drive %s, Part %d: TYPE=0x%02x, LBA=0x%lx, SIZE=%u\n", 
                drive->drive_name, i,
                p->os_type, (unsigned long)p->lba_start, (uint32_t)p->size_sectors);
        
        /* Auto-probe and mount */
        void vfs_probe_and_mount(disk_partition_t *part);
        vfs_probe_and_mount(p);

        found++;
    }
    
    kfree(sector);
    return found;
}

void vfs_probe_and_mount(disk_partition_t *part) {
    char mount_path[32];
    vfs_node_t *fs_root = NULL;

    /* Try FAT32 */
    if (part->os_type == 0x0B || part->os_type == 0x0C || part->os_type == 0x0E || part->os_type == 0xEF) {
        fs_root = fat32_mount(part);
    }
    /* Try NTFS (MBR type 0x07 — also covers exFAT; ntfs_mount verifies OEM magic) */
    else if (part->os_type == 0x07) {
        fs_root = ntfs_mount(part);
    }
    /* Try Ext4 */
    else if (part->os_type == 0x83) {
        fs_root = ext4_mount(part);
    }

    if (fs_root) {
        /* Deterministic path: /mnt/<drive_name><part_index+1> */
        strcpy(mount_path, "/mnt/");
        strcat(mount_path, part->drive->drive_name);
        
        int len = strlen(mount_path);
        mount_path[len] = '1' + part->partition_index;
        mount_path[len + 1] = '\0';
        
        vfs_mkdir(mount_path);
        vfs_mount(mount_path, fs_root);
    }

    /* Register block device node in /dev regardless of FS type */
    {
        char dev_name[8];
        strncpy(dev_name, part->drive->drive_name, 3);
        dev_name[3] = '\0';
        int len = strlen(dev_name);
        dev_name[len]     = '1' + part->partition_index;
        dev_name[len + 1] = '\0';
        devfs_register_block(dev_name, part->drive,
                             part->lba_start, part->size_sectors);
    }
}

int partition_parse_gpt(ata_drive_t *drive) {
    if (!drive || !drive->present) return -1;

    uint8_t *sector = kmalloc(512);
    if (disk_read_sectors(drive, 1, 1, sector) == 0) {
        kfree(sector);
        return -1;
    }

    gpt_header_t *header = (gpt_header_t*)sector;
    if (memcmp(header->signature, "EFI PART", 8) != 0) {
        kfree(sector);
        return 0;
    }

    uint32_t count = header->num_partition_entries;
    uint32_t entry_size = header->size_partition_entry;
    uint64_t entry_lba = header->partition_entry_lba;
    
    kfree(sector);

    /* Read partition entries */
    uint8_t *entries_buf = kmalloc(512); /* Read one sector of entries at a time */
    int found = 0;

    /* For Phase 3, we just read the first sector of entries (usually 4 entries) */
    if (disk_read_sectors(drive, entry_lba, 1, entries_buf) == 0) {
        kfree(entries_buf);
        return -1;
    }

    gpt_entry_t *entries = (gpt_entry_t*)entries_buf;
    for (uint32_t i = 0; i < (512 / entry_size) && i < count; i++) {
        /* Check if GUID is all zeros (unused) */
        bool used = false;
        for (int b = 0; b < 16; b++) if (entries[i].partition_type_guid[b] != 0) used = true;
        if (!used) continue;

        if (g_partition_count >= MAX_PARTITIONS) break;

        disk_partition_t *p = &g_partitions[g_partition_count++];
        p->drive = drive;
        p->partition_index = i;
        p->os_type = 0xEE; /* GPT generic */
        p->lba_start = entries[i].starting_lba;
        p->size_sectors = entries[i].ending_lba - entries[i].starting_lba + 1;

        kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
        kprintf("[PART] ");
        kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
        kprintf("Drive %s, Part %d: LBA=0x%lx, SIZE=%u\n", 
                drive->drive_name, i, (unsigned long)p->lba_start, (uint32_t)p->size_sectors);

        vfs_probe_and_mount(p);
        found++;
    }

    kfree(entries_buf);
    return found;
}

disk_partition_t *partition_get_list(int *count_out) {
    if (count_out) *count_out = g_partition_count;
    return g_partitions;
}
