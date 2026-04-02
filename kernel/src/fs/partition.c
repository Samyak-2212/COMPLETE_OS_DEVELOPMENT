/* ============================================================================
 * partition.c — NexusOS Partition Parsers
 * Purpose: Reads sector 0 to map MBR partitions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "fs/partition.h"
#include "lib/printf.h"
#include "mm/heap.h"

static disk_partition_t g_partitions[MAX_PARTITIONS];
static int g_partition_count = 0;

int partition_parse_mbr(ata_drive_t *drive) {
    if (!drive || !drive->present) return -1;
    
    /* Sector buffer */
    uint8_t *sector = kmalloc(512);
    if (!sector) return -1;
    
    if (ata_read_sectors(drive, 0, 1, sector) == 0) {
        kfree(sector);
        return -1;
    }
    
    /* Check MBR signature 0x55AA at the end */
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        kfree(sector);
        return 0; /* No MBR or invalid */
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
        
        kprintf_set_color(0x0088FF88, 0x001A1A2E);
        kprintf("[PART] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("ATA Port 0x%x (Master=%d), Part %d: TYPE=0x%02x, LBA=0x%x, SIZE=%u\n", 
                drive->io_base, drive->is_master, i,
                p->os_type, (uint32_t)p->lba_start, (uint32_t)p->size_sectors);
        
        found++;
    }
    
    kfree(sector);
    return found;
}

disk_partition_t *partition_get_list(int *count_out) {
    if (count_out) *count_out = g_partition_count;
    return g_partitions;
}
