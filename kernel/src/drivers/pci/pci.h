/* ============================================================================
 * pci.h — NexusOS PCI Bus Manager
 * Purpose: Memory-mapped / port I/O PCI configuration space interface
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_PCI_PCI_H
#define NEXUS_DRIVERS_PCI_PCI_H

#include <stdint.h>
#include "drivers/pci/pci_ids.h"

/* Type 1 PCI configuration space ports */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* Base address register types */
#define PCI_BAR_TYPE_NONE   0
#define PCI_BAR_TYPE_MMIO   1
#define PCI_BAR_TYPE_IO     2

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    
    uint16_t vendor_id;
    uint16_t device_id;
    
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t prog_if;
    uint8_t revision_id;
    
    uint8_t header_type;
    uint8_t multi_function;

    /* Up to 6 BARs for normal devices (header type 0x00) */
    struct {
        int      type;       /* MMIO or IO */
        uint64_t address;    /* Physical base address */
        uint64_t size;       /* Memory or IO port region size */
        int      prefetchable;
    } bars[6];
    
    uint8_t irq_line;
    uint8_t irq_pin;
} pci_device_t;

/* Read operations */
uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint8_t  pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/* Write operations */
void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
void pci_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);

/* Initialize the PCI subsystem and map discovered devices to registered drivers */
void pci_init(void);

/* Get all discovered PCI devices (useful for manual subsystem scanning) */
pci_device_t *pci_get_devices(int *count_out);

#endif /* NEXUS_DRIVERS_PCI_PCI_H */
