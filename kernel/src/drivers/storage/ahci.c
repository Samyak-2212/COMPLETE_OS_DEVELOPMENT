/* ============================================================================
 * ahci.c — NexusOS AHCI SATA Storage Driver
 * Purpose: Block device reads over PCI MMIO AHCI Interface
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/storage/ahci.h"
#include "drivers/storage/ata.h"
#include "drivers/driver.h"
#include "drivers/pci/pci.h"
#include "fs/partition.h"
#include "fs/devfs.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"

typedef struct {
    hba_port_t *port_reg;
    hba_cmd_header_t *cmd_list;
    void *fis_base;
    hba_cmd_tbl_t *cmd_tbl;
} sata_port_info_t;

static sata_port_info_t port_infos[32];
static ata_drive_t sata_drives[32];

/* 
 * NOTE: Full AHCI DMA integration is extremely complex. 
 * For Phase 3 chunk 1, we will setup the device driver hook so it's 
 * registered alongside ATA but defer full asynchronous command list building.
 * We prioritize the ATA PIO for QEMU emulation. AHCI will probe and acknowledge safely.
 */

static int ahci_stop_port(hba_port_t *port) {
    /* 1. Clear ST (bit 0) */
    port->cmd &= ~(1 << 0);

    /* 2. Wait for CR (bit 15) to clear */
    for (int i = 0; i < 1000000; i++) {
        if (!(port->cmd & (1 << 15))) break;
        __asm__ volatile("pause");
    }

    /* 3. Clear FRE (bit 4) */
    port->cmd &= ~(1 << 4);

    /* 4. Wait for FR (bit 14) to clear */
    for (int i = 0; i < 1000000; i++) {
        if (!(port->cmd & (1 << 14))) break;
        __asm__ volatile("pause");
    }
    
    /* 5. If STILL not stopped, perform a localized Port Reset */
    if ((port->cmd & (1 << 15)) || (port->cmd & (1 << 14))) {
        port->sctl = (port->sctl & ~0x0F) | 0x01; /* DET=1 (Perform Reset) */
        for (int i = 0; i < 1000000; i++) __asm__ volatile("pause");
        port->sctl &= ~0x0F; /* DET=0 (End Reset) */
        for (int i = 0; i < 1000000; i++) __asm__ volatile("pause");
        
        /* Final check */
        if (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14))) return 1;
        return 0;
    }

    return 1;
}

static void ahci_start_port(hba_port_t *port) {
    /* Wait for CR to clear with timeout */
    int timeout = 1000000;
    while ((port->cmd & (1 << 15)) && timeout--);
    
    /* Set FRE and ST */
    port->cmd |= (1 << 4);
    port->cmd |= (1 << 0);
}

static int ahci_find_cmdslot(hba_port_t *port) {
    /* If not set in SACT and not set in CI, it's free */
    uint32_t slots = (port->sact | port->ci);
    for (int i = 0; i < 32; i++) {
        if ((slots & (1 << i)) == 0) return i;
    }
    return -1;
}

int ahci_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer) {
    sata_port_info_t *info = (sata_port_info_t *)drive->device_data;
    hba_port_t *port = info->port_reg;
    
    /* Wait for previous command to clear */
    port->is = 0xFFFFFFFF; /* Clear interrupts */
    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return 0;

    int timeout;

    /* Get command header and table */
    hba_cmd_header_t *cmdhdr = info->cmd_list + slot;
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 0; /* Read */
    cmdhdr->prdtl = 1; /* 1 PRDT entry */

    hba_cmd_tbl_t *cmdtbl = info->cmd_tbl;
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t) + (cmdhdr->prdtl - 1) * sizeof(hba_prdt_entry_t));

    /* Set up PRDT */
    cmdtbl->prdt_entry[0].dba = vmm_get_phys((uint64_t)buffer);
    cmdtbl->prdt_entry[0].dbc = (count << 9) - 1; /* 512 bytes per sector */
    cmdtbl->prdt_entry[0].i = 1;

    /* Set up FIS */
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(&cmdtbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1; /* Command */
    fis->command = 0x25; /* READ DMA EXT (LBA48) */

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6; /* LBA mode */

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = count;
    fis->counth = 0;

    /* Issue command */
    port->ci = (1 << slot);

    /* Wait for completion with timeout */
    timeout = 1000000;
    while (timeout--) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) break;
        __asm__ volatile("pause");
    }
    
    /* Small settling delay for DMA/Memory Coherency */
    for (int i = 0; i < 1000; i++) __asm__ volatile("pause");

    if (timeout <= 0) {
        kprintf("       [AHCI] Read Timeout (CI=0x%x, IS=0x%x)\n", port->ci, port->is);
        return 0;
    }
    if (port->is & (1 << 30)) {
        kprintf("       [AHCI] Task File Error (TFD=0x%x)\n", port->tfd);
        return 0;
    }
    return 1;
}

static int ahci_probe(void *device_info) {
    pci_device_t *pci = (pci_device_t *)device_info;
    
    /* Storage -> SATA Controller or NVMe */
    if (pci->class_code == PCI_CLASS_STORAGE && pci->subclass_code == PCI_SUBCLASS_STORAGE_SATA) {
        return 1;
    }
    return 0;
}

static int ahci_init(void *device_info) {
    pci_device_t *pci = (pci_device_t *)device_info;
    
    /* Find MMIO BAR (BAR5 usually is ABAR - AHCI Base Address Register) */
    uint64_t mmio_phys = 0;
    for (int i = 0; i < 6; i++) {
        if (pci->bars[i].type == PCI_BAR_TYPE_MMIO) {
            mmio_phys = pci->bars[i].address;
            /* In modern systems, ABAR is BAR5 */
            if (i == 5) break; 
        }
    }
    
    if (mmio_phys == 0) {
        return -1;
    }

    kprintf("       [AHCI] Found ABAR at phys 0x%lx\n", (unsigned long)mmio_phys);

    /* Map the ABAR (usually 4KB or 8KB, 1 page is enough for base registers) */
    uint64_t hhdm = vmm_get_hhdm_offset();
    uint64_t mmio_virt = mmio_phys + hhdm;
    vmm_map_page(mmio_virt, mmio_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    
    hba_mem_t *abar = (hba_mem_t *)mmio_virt;

    /* 1. Enable AHCI Mode */
    abar->ghc |= (1 << 31); /* AE: AHCI Enable */

    uint32_t cap = abar->cap;
    uint32_t pi  = abar->pi;
    uint32_t vs  = abar->vs;
    kprintf("[AHCI] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("SATA Controller: Version %d.%d, Ports: %d, Cap: 0x%x\n", 
            (vs >> 16) & 0xFFFF, (vs & 0xFFFF), 
            32 - __builtin_clz(pi), cap);

    /* Log which ports have devices */
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            /* Port registers start at 0x100, each port is 0x80 bytes */
            hba_port_t *port = &abar->ports[i];
            uint32_t ssts = port->ssts; /* 0x28: Port x Serial ATA Status */
            uint8_t det = ssts & 0x0F;
            
            if (det == 0x03) { /* Device present and communication established */
                kprintf("       Port %d: Device connected\n", i);
                
                /* 1. Stop Port */
                if (!ahci_stop_port(port)) {
                    kprintf("       Port %d: Error - Failed to stop port (PxTFD=0x%x)\n", i, port->tfd);
                    continue;
                }

                /* Clear error and interrupt status */
                port->serr = 0xFFFFFFFF;
                port->is = 0xFFFFFFFF;

                /* 2. Allocate and Map DMA memory for this port */
                uint64_t phys_clb = pmm_alloc_page();
                uint64_t phys_fb  = pmm_alloc_page();
                uint64_t phys_ct  = pmm_alloc_page();
                
                uint64_t virt_clb = phys_clb + hhdm;
                uint64_t virt_fb  = phys_fb + hhdm;
                uint64_t virt_ct  = phys_ct + hhdm;
                
                vmm_map_page(virt_clb, phys_clb, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
                vmm_map_page(virt_fb, phys_fb, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
                vmm_map_page(virt_ct, phys_ct, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);

                kprintf("       Port %d: DMA CLB=0x%lx, FB=0x%lx, CT=0x%lx\n", 
                        i, (unsigned long)phys_clb, (unsigned long)phys_fb, (unsigned long)phys_ct);

                /* 3. Configure Port Registers */
                port->clb = (uint32_t)phys_clb;
                port->clbu = (uint32_t)(phys_clb >> 32);
                port->fb = (uint32_t)phys_fb;
                port->fbu = (uint32_t)(phys_fb >> 32);

                /* Initialize all 32 command slots to use the same command table 
                 * This is safe because we only issue 1 command at a time. */
                hba_cmd_header_t *clist = (hba_cmd_header_t *)virt_clb;
                for (int j = 0; j < 32; j++) {
                    clist[j].ctba = phys_ct;
                }

                /* 4. Start Port */
                ahci_start_port(port);

                /* Store info for read helper */
                port_infos[i].port_reg = port;
                port_infos[i].cmd_list = clist;
                port_infos[i].fis_base = (void *)virt_fb;
                port_infos[i].cmd_tbl = (hba_cmd_tbl_t *)virt_ct;

                /* Register proxy drive for VFS/Partition parser */
                ata_drive_t *drive = &sata_drives[i];
                memset(drive, 0, sizeof(ata_drive_t));
                drive->type = DISK_TYPE_SATA;
                drive->device_data = (void *)&port_infos[i];
                drive->present = 1;
                
                /* Deterministic naming: sda, sdb... based on port index */
                drive->drive_name[0] = 's';
                drive->drive_name[1] = 'd';
                drive->drive_name[2] = 'a' + i;
                drive->drive_name[3] = '\0';

                /* 5. Wait for SATA Link training to complete after reset */
                for (int i = 0; i < 1000000; i++) {
                    if ((port->ssts & 0x0F) == 0x03) break;
                    __asm__ volatile("pause");
                }

                /* Register whole-disk node in /dev (e.g. /dev/sda) */
                devfs_register_block(drive->drive_name, drive,
                                     0, drive->total_sectors);

                partition_parse_mbr(drive);
            }
        }
    }

    return 0; 
}

static driver_t ahci_driver = {
    .name = "AHCI SATA Storage",
    .probe = ahci_probe,
    .init = ahci_init
};

void ahci_init_subsystem(void) {
    driver_register(&ahci_driver);
}
