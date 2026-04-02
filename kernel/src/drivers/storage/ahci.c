/* ============================================================================
 * ahci.c — NexusOS AHCI SATA Storage Driver
 * Purpose: Block device reads over PCI MMIO AHCI Interface
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/storage/ahci.h"
#include "drivers/driver.h"
#include "drivers/pci/pci.h"
#include "lib/printf.h"
#include "mm/vmm.h"
#include "mm/pmm.h"

/* 
 * NOTE: Full AHCI DMA integration is extremely complex. 
 * For Phase 3 chunk 1, we will setup the device driver hook so it's 
 * registered alongside ATA but defer full asynchronous command list building.
 * We prioritize the ATA PIO for QEMU emulation. AHCI will probe and acknowledge safely.
 */

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
    uint64_t mmio_addr = 0;
    for (int i = 0; i < 6; i++) {
        if (pci->bars[i].type == PCI_BAR_TYPE_MMIO && pci->bars[i].size > 0) {
            mmio_addr = pci->bars[i].address;
            /* In a real implementation we would map this address to Virtual mem */
        }
    }
    
    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[AHCI] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("SATA Controller found. ABAR at Phys 0x%016llx (Init Deferred)\n", (unsigned long long)mmio_addr);

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
