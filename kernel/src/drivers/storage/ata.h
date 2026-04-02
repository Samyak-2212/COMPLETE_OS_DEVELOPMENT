/* ============================================================================
 * ata.h — NexusOS ATA PIO Storage Driver
 * Purpose: Block device reads via legacy IDE PIO
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_STORAGE_ATA_H
#define NEXUS_DRIVERS_STORAGE_ATA_H

#include <stdint.h>

/* Primary Bus I/O Ports */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_PRIMARY_IRQ     14

/* Secondary Bus I/O Ports */
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376
#define ATA_SECONDARY_IRQ   15

/* Drive Selection */
#define ATA_MASTER          0xA0
#define ATA_SLAVE           0xB0

/* Registers offset from I/O base */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT0   0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07

/* Status bits */
#define ATA_SR_ERR          0x01    /* Error */
#define ATA_SR_IDX          0x02    /* Index */
#define ATA_SR_CORR         0x04    /* Corrected data */
#define ATA_SR_DRQ          0x08    /* Data request ready */
#define ATA_SR_SRV          0x10    /* Overlapped Mode Service Request */
#define ATA_SR_DF           0x20    /* Drive Fault Error */
#define ATA_SR_RDY          0x40    /* Drive Ready */
#define ATA_SR_BSY          0x80    /* Busy */

/* Commands */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_IDENTIFY    0xEC

typedef struct {
    int present;
    int is_master;
    int is_lba48;
    uint32_t io_base;
    uint32_t ctrl_base;
    uint64_t total_sectors;
    char model[41];
} ata_drive_t;

/* Global initialization and registry */
void ata_init_subsystem(void);

/* Public API for filesystem */
int ata_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer);

#endif /* NEXUS_DRIVERS_STORAGE_ATA_H */
