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
    uint64_t bounce_phys;
    void *bounce_virt;
    uint8_t bus, device, func;
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
    /* 1. Clear ST (bit 0) and FRE (bit 4) */
    port->cmd &= ~(1 << 0);
    port->cmd &= ~(1 << 4);

    /* 2. Wait for CR (bit 15) and FR (bit 14) to clear */
    int timeout = 1000000;
    while (timeout--) {
        if (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14))) return 1;
        __asm__ volatile("pause");
    }

    return 0;
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

int ahci_identify(ata_drive_t *drive) {
    sata_port_info_t *info = (sata_port_info_t *)drive->device_data;
    hba_port_t *port = info->port_reg;

    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return 0;

    hba_cmd_header_t *cmdhdr = info->cmd_list + slot;
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 0;
    cmdhdr->prdtl = 1;

    hba_cmd_tbl_t *cmdtbl = info->cmd_tbl;
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));

    cmdtbl->prdt_entry[0].dba = info->bounce_phys;
    cmdtbl->prdt_entry[0].dbc = 512 - 1;
    cmdtbl->prdt_entry[0].i = 1;

    /* Clear errors and interrupts */
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    /* Wait for port not busy */
    int timeout = 1000000;
    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && timeout--) __asm__ volatile("pause");

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(cmdtbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = 0xEC; /* IDENTIFY DEVICE */

    /* Wait for port not busy */
    timeout = 1000000;
    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && timeout--) __asm__ volatile("pause");

    port->is = 0xFFFFFFFF;
    port->ci = (1 << slot);

    /* Posted write flush */
    (void)port->ci;

    timeout = 1000000;
    while (timeout--) {
        if ((port->ci & (1 << slot)) == 0) break;
        __asm__ volatile("pause");
    }

    if (timeout <= 0) {
        kprintf("       Port %d: IDENTIFY Timeout (CI=0x%x, TFD=0x%x, IS=0x%x)\n", drive->drive_name[2] - 'a', port->ci, port->tfd, port->is);
        return 0;
    }

    /* Verify mapping */
    uint64_t v_phys = vmm_get_phys((uint64_t)info->bounce_virt);
    if (v_phys != info->bounce_phys) {
        kprintf("       Port %d: ERROR - Mapping mismatch! Virt %p -> Phys %lx (Expected %lx)\n", 
                drive->drive_name[2] - 'a', info->bounce_virt, (unsigned long)v_phys, (unsigned long)info->bounce_phys);
    }

    uint16_t *buf = (uint16_t *)info->bounce_virt;
    /* Extract model string (20 words at offset 27) */
    for (int i = 0; i < 20; i++) {
        drive->model[i * 2] = (char)(buf[27 + i] >> 8);
        drive->model[i * 2 + 1] = (char)(buf[27 + i] & 0xFF);
    }
    drive->model[40] = '\0';

    /* Extract capacity (LBA48) */
    uint64_t sectors = *((uint64_t*)&buf[100]);
    if (sectors == 0) sectors = *((uint32_t*)&buf[60]); /* LBA28 fallback */
    drive->total_sectors = sectors;

    return 1;
}

int ahci_read_sectors(ata_drive_t *drive, uint64_t lba, uint8_t count, uint8_t *buffer) {
    sata_port_info_t *info = (sata_port_info_t *)drive->device_data;
    hba_port_t *port = info->port_reg;
    
    /* Register access check/recovery path */
    if (port->is == 0xFFFFFFFF) {
        /* Re-enforce PCI Memory Space and Bus Master bits */
        pci_write_word(info->bus, info->device, info->func, 0x04, 0x0007);
        hba_mem_t *abar = (hba_mem_t *)(vmm_get_hhdm_offset() + (uint64_t)info->port_reg - ((uint64_t)info->port_reg % 0x1000));
        abar->ghc |= (1 << 31);
        (void)abar->ghc;
        
        if (port->is == 0xFFFFFFFF) {
            kprintf("       Port %d: Fatal register access failure (ABORT)\n", drive->drive_name[2] - 'a');
            return 0;
        }
    }
    
    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;

    int slot = ahci_find_cmdslot(port);
    if (slot == -1) return 0;

    hba_cmd_header_t *cmdhdr = info->cmd_list + slot;
    cmdhdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmdhdr->w = 0;
    cmdhdr->prdtl = 1;

    hba_cmd_tbl_t *cmdtbl = info->cmd_tbl;
    memset(cmdtbl, 0, sizeof(hba_cmd_tbl_t));

    cmdtbl->prdt_entry[0].dba = info->bounce_phys;
    cmdtbl->prdt_entry[0].dbc = (count << 9) - 1;
    cmdtbl->prdt_entry[0].i = 1;

    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)(cmdtbl->cfis);
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = 0x25; /* READ DMA EXT */

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 0x40; /* LBA mode */

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = count;
    fis->counth = 0;

    /* Wait for port not busy */
    int timeout = 1000000;
    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && timeout--) __asm__ volatile("pause");

    /* Memory Barrier before issue */
    __asm__ volatile("sfence" ::: "memory");

    port->is = 0xFFFFFFFF;
    port->ci = (1 << slot);

    /* Posted write flush */
    (void)port->ci;

    timeout = 1000000;
    while (timeout--) {
        if ((port->ci & (1 << slot)) == 0) break;
        if (port->is & (1 << 30)) break;
        __asm__ volatile("pause");
    }
    
    if (timeout <= 0 || (port->is & (1 << 30))) {
        kprintf("       Port %d: READ Timeout/Error (CI=0x%x, TFD=0x%x, IS=0x%x)\n", drive->drive_name[2] - 'a', port->ci, port->tfd, port->is);
        return 0;
    }

    /* Memory Barrier after DMA */
    __asm__ volatile("lfence" ::: "memory");

    memcpy(buffer, info->bounce_virt, count * 512);
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
    uint8_t pci_bus = pci->bus;
    uint8_t pci_dev = pci->device;
    uint8_t pci_func = pci->function;
    
    uint64_t mmio_phys = 0;
    for (int i = 0; i < 6; i++) {
        if (pci->bars[i].type == PCI_BAR_TYPE_MMIO) {
            mmio_phys = pci->bars[i].address;
            if (i == 5) break; 
        }
    }
    
    if (mmio_phys == 0) return -1;

    /* Enforce PCI Command bits: Memory Space (bit 1) + Bus Master (bit 2) */
    uint16_t pci_cmd = pci_read_word(pci->bus, pci->device, pci->function, 0x04);
    pci_write_word(pci->bus, pci->device, pci->function, 0x04, pci_cmd | 0x06);

    uint64_t hhdm = vmm_get_hhdm_offset();
    uint64_t mmio_virt = mmio_phys + hhdm;
    vmm_map_page(mmio_virt, mmio_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    vmm_map_page(mmio_virt + 0x1000, mmio_phys + 0x1000, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    
    hba_mem_t *abar = (hba_mem_t *)mmio_virt;

    /* 1. Global AHCI Enable */
    abar->ghc |= (1 << 31);
    (void)abar->ghc; /* Flush */

    uint32_t pi = abar->pi;
    uint32_t vs = abar->vs;
    uint32_t cap = abar->cap;
    
    kprintf("[AHCI] SATA Controller: Version %d.%d, Ports: %d, Cap: 0x%x\n", 
            (vs >> 16) & 0xFFFF, (vs & 0xFFFF), 
            32 - __builtin_clz(pi), cap);

    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            hba_port_t *port = &abar->ports[i];
            
            /* Wait for Link Training (Up to 1s) */
            int timeout = 1000000;
            while ((port->ssts & 0x0F) != 0x03 && timeout--) __asm__ volatile("pause");

            uint8_t det = port->ssts & 0x0F;
            if (det == 0x03) {
                if (port->sig != 0x00000101) {
                    kprintf("       Port %d: Connected (Sig=0x%x) - Skipping non-SATA\n", i, port->sig);
                    continue;
                }

                kprintf("       Port %d: SATA Drive Connected\n", i);
                
                if (!ahci_stop_port(port)) continue;

                port->serr = 0xFFFFFFFF;
                port->is = 0xFFFFFFFF;

                uint64_t phys_clb = pmm_alloc_page();
                uint64_t phys_fb  = pmm_alloc_page();
                uint64_t phys_ct  = pmm_alloc_page();
                
                uint64_t virt_clb = phys_clb + hhdm;
                uint64_t virt_fb  = phys_fb + hhdm;
                uint64_t virt_ct  = phys_ct + hhdm;
                
                uint64_t bounce_phys = pmm_alloc_pages(32);
                uint64_t bounce_virt = bounce_phys + hhdm; /* STABLE HHDM MAPPING */
                
                kprintf("       Port %d: DMA CLB=0x%lx, FB=0x%lx, CT=0x%lx, BOUNCE=0x%lx\n", 
                        i, (unsigned long)phys_clb, (unsigned long)phys_fb, (unsigned long)phys_ct, (unsigned long)bounce_phys);

                port->clb = (uint32_t)phys_clb;
                port->clbu = (uint32_t)(phys_clb >> 32);
                port->fb = (uint32_t)phys_fb;
                port->fbu = (uint32_t)(phys_fb >> 32);

                hba_cmd_header_t *clist = (hba_cmd_header_t *)virt_clb;
                memset(clist, 0, 4096);
                for (int j = 0; j < 32; j++) {
                    clist[j].ctba = phys_ct;
                }

                ahci_start_port(port);

                port_infos[i].port_reg = port;
                port_infos[i].cmd_list = clist;
                port_infos[i].fis_base = (void *)virt_fb;
                port_infos[i].cmd_tbl = (hba_cmd_tbl_t *)virt_ct;
                port_infos[i].bounce_phys = bounce_phys;
                port_infos[i].bounce_virt = (void *)bounce_virt;
                port_infos[i].bounce_virt = (void *)bounce_virt;
                port_infos[i].bus = pci_bus;
                port_infos[i].device = pci_dev;
                port_infos[i].func = pci_func;

                ata_drive_t *drive = &sata_drives[i];
                memset(drive, 0, sizeof(ata_drive_t));
                drive->type = DISK_TYPE_SATA;
                drive->device_data = (void *)&port_infos[i];
                drive->present = 1;
                drive->read_sectors = ahci_read_sectors;
                drive->drive_name[0] = 's';
                drive->drive_name[1] = 'd';
                drive->drive_name[2] = 'a' + i;
                drive->drive_name[3] = '\0';

                /* Perform IDENTIFY to get model and size */
                if (ahci_identify(drive)) {
                    kprintf("       Port %d: Model: %s, Capacity: %llu sectors\n", i, drive->model, (unsigned long long)drive->total_sectors);
                } else {
                    kprintf("       Port %d: Warning - IDENTIFY failed\n", i);
                }

                devfs_register_block(drive->drive_name, drive, 0, drive->total_sectors);
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
