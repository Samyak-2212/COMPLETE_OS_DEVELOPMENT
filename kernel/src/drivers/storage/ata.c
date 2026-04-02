/* ============================================================================
 * ata.c — NexusOS ATA PIO Storage Driver
 * Purpose: Block device reads via legacy IDE PIO
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/storage/ata.h"
#include "drivers/driver.h"
#include "drivers/pci/pci.h"
#include "hal/io.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "fs/partition.h"

static ata_drive_t primary_master;
static ata_drive_t primary_slave;

/* ── Generic IO Helpers ── */

static void ata_delay(uint32_t io_base) {
    /* Reading Alt Status 4 times takes 400ns, allowing drive to update status */
    inb(io_base + 0x206);
    inb(io_base + 0x206);
    inb(io_base + 0x206);
    inb(io_base + 0x206);
}

static int ata_wait_busy(uint32_t io_base) {
    int timeout = 100000;
    while((inb(io_base + ATA_REG_STATUS) & ATA_SR_BSY) && timeout) {
        timeout--;
        io_wait();
    }
    return timeout > 0;
}

static int ata_wait_drq(uint32_t io_base) {
    int timeout = 100000;
    while(!(inb(io_base + ATA_REG_STATUS) & ATA_SR_DRQ) && timeout) {
        if (inb(io_base + ATA_REG_STATUS) & ATA_SR_ERR) {
            return 0; /* Error occurred */
        }
        timeout--;
        io_wait();
    }
    return timeout > 0;
}

/* ── String swap for identify ── */
static void ata_fix_string(uint8_t *str, size_t max_len) {
    for (size_t i = 0; i < max_len; i += 2) {
        uint8_t c = str[i];
        str[i] = str[i + 1];
        str[i + 1] = c;
    }
    str[max_len] = '\0';
    
    /* Trim trailing spaces */
    for (int i = max_len - 1; i >= 0; i--) {
        if (str[i] == ' ') str[i] = '\0';
        else if (str[i] != '\0') break;
    }
}

/* ── Identification ── */

static int ata_identify(ata_drive_t *drive) {
    uint32_t io = drive->io_base;
    uint8_t sel = drive->is_master ? ATA_MASTER : ATA_SLAVE;

    /* Select drive */
    outb(io + ATA_REG_HDDEVSEL, sel);
    outb(io + ATA_REG_SECCOUNT0, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);

    /* Send identify command */
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(io);
    
    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0) {
        return 0; /* Drive does not exist */
    }

    /* Wait BSY clear */
    if (!ata_wait_busy(io)) return 0;

    /* Check LBA registers. If not 0, it's not ATA (e.g. ATAPI) */
    if (inb(io + ATA_REG_LBA1) != 0 || inb(io + ATA_REG_LBA2) != 0) {
        return 0; /* Not standard ATA */
    }

    /* Wait DRQ */
    if (!ata_wait_drq(io)) return 0;

    /* Read 256 words (512 bytes) of identity data */
    uint16_t ident[256] = {0};
    for (int i = 0; i < 256; i++) {
        ident[i] = inw(io + ATA_REG_DATA);
    }

    drive->present = 1;
    drive->is_lba48 = (ident[83] & (1<<10)) != 0;

    if (drive->is_lba48) {
        drive->total_sectors = ((uint64_t)ident[100]) | ((uint64_t)ident[101] << 16) | ((uint64_t)ident[102] << 32) | ((uint64_t)ident[103] << 48);
    } else {
        drive->total_sectors = ((uint32_t)ident[60]) | ((uint32_t)ident[61] << 16);
    }

    memcpy(drive->model, &ident[27], 40);
    ata_fix_string((uint8_t*)drive->model, 40);

    return 1;
}

/* ── Read Sectors ── */

int ata_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer) {
    if (!drive || !drive->present) return 0;
    
    uint32_t io = drive->io_base;
    
    if (!ata_wait_busy(io)) return 0;

    if (drive->is_lba48) {
        /* Standard 48-bit LBA read */
        outb(io + ATA_REG_HDDEVSEL, 0x40 | (drive->is_master ? 0x00 : 0x10));
        
        /* High bytes */
        outb(io + ATA_REG_SECCOUNT0, 0); /* High byte of count */
        outb(io + ATA_REG_LBA0, (uint8_t)(lba >> 24));
        outb(io + ATA_REG_LBA1, (uint8_t)(lba >> 32));
        outb(io + ATA_REG_LBA2, (uint8_t)(lba >> 40));

        /* Low bytes */
        outb(io + ATA_REG_SECCOUNT0, count);
        outb(io + ATA_REG_LBA0, (uint8_t)lba);
        outb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));

        outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    } else {
        /* Legacy 28-bit LBA read */
        outb(io + ATA_REG_HDDEVSEL, 0xE0 | (drive->is_master ? 0x00 : 0x10) | ((lba >> 24) & 0x0F));
        outb(io + ATA_REG_SECCOUNT0, count);
        outb(io + ATA_REG_LBA0, (uint8_t)lba);
        outb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
        outb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));
        
        outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    }

    /* Read the data */
    for (int i = 0; i < count; i++) {
        ata_delay(io);
        if (!ata_wait_busy(io)) return 0;
        if (!ata_wait_drq(io)) return 0;

        for (int j = 0; j < 256; j++) {
            uint16_t word = inw(io + ATA_REG_DATA);
            buffer[i * 512 + j * 2] = word & 0xFF;
            buffer[i * 512 + j * 2 + 1] = (word >> 8) & 0xFF;
        }
    }
    return 1;
}

/* ── Driver Model Inteface ── */

static int ata_probe(void *device_info) {
    pci_device_t *pci = (pci_device_t *)device_info;
    
    /* Storage -> IDE Controller */
    if (pci->class_code == PCI_CLASS_STORAGE && pci->subclass_code == PCI_SUBCLASS_STORAGE_IDE) {
        return 1;
    }
    return 0;
}

static int ata_init(void *device_info) {
    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[IDE] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Probing Primary Master drive on legacy port 0x1F0...\n");

    primary_master.io_base = ATA_PRIMARY_IO;
    primary_master.ctrl_base = ATA_PRIMARY_CTRL;
    primary_master.is_master = 1;
    primary_master.present = 0;

    if (ata_identify(&primary_master)) {
        kprintf_set_color(0x0088FF88, 0x001A1A2E);
        kprintf("[OK] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("ATA Drive found! Model: '%s' | %llu sectors (%llu MB)\n", 
            primary_master.model, 
            (unsigned long long)primary_master.total_sectors,
            (unsigned long long)(primary_master.total_sectors * 512 / (1024*1024)));
        
        partition_parse_mbr(&primary_master);
    } else {
        kprintf_set_color(0x00FF4444, 0x001A1A2E);
        kprintf("[FAIL] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("No ATA drive detected on Primary Master.\n");
    }

    /* Probe Primary Slave */
    primary_slave.io_base = ATA_PRIMARY_IO;
    primary_slave.is_master = 0;
    primary_slave.present = 0;

    if (ata_identify(&primary_slave)) {
        kprintf_set_color(0x0088FF88, 0x001A1A2E);
        kprintf("[OK] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("ATA SLAVE Drive found! Model: '%s' | %llu sectors\n", 
            primary_slave.model, 
            (unsigned long long)primary_slave.total_sectors);
        
        partition_parse_mbr(&primary_slave);
    }

    return 0;
}

static driver_t ata_driver = {
    .name = "ATA PIO Storage",
    .probe = ata_probe,
    .init = ata_init
};

void ata_init_subsystem(void) {
    driver_register(&ata_driver);
}
