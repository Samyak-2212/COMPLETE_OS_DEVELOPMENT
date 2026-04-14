/* ============================================================================
 * pci.c — NexusOS PCI Bus Enumeration
 * Purpose: Scan all PCI buses to populate system device list and BARs
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/pci/pci.h"
#include "drivers/driver.h"
#include "hal/io.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/heap.h"

/* Internal structure to track all PCI devices */
#define MAX_PCI_DEVICES 256
static pci_device_t pci_device_list[MAX_PCI_DEVICES];
static int pci_device_count = 0;

static void pci_scan_bus(uint8_t bus);

/* Read Configuration Space */
uint32_t pci_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_dword(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_dword(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

/* Write Configuration Space */
void pci_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

void pci_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outw(PCI_CONFIG_DATA + (offset & 2), value);
}

void pci_write_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(PCI_CONFIG_ADDRESS, address);
    outb(PCI_CONFIG_DATA + (offset & 3), value);
}

/* Base Address Register Size Decoding */
static void pci_check_bar(pci_device_t *dev, int bar_index, uint8_t offset) {
    uint32_t bar_val = pci_read_dword(dev->bus, dev->device, dev->function, offset);
    if (!bar_val) {
        dev->bars[bar_index].type = PCI_BAR_TYPE_NONE;
        return;
    }

    if (dev->class_code == 0x03 /* PCI_CLASS_DISPLAY */ || 
        dev->class_code == 0x06 /* Any Bridge (Host, ISA, etc.) */) {
        dev->bars[bar_index].type = (bar_val & 1) ? PCI_BAR_TYPE_IO : PCI_BAR_TYPE_MMIO;
        dev->bars[bar_index].address = (bar_val & 1) ? (bar_val & ~0x03) : (bar_val & ~0x0F);
        dev->bars[bar_index].size = 0; /* Unmeasured */
        return;
    }

    /* Disable interrupts to prevent EOI loss if ISA bridge I/O space is disabled */
    __asm__ volatile ("cli");

    /* PCI Spec: Disable Memory Space and I/O Space before sizing BARs */
    uint16_t cmd = pci_read_word(dev->bus, dev->device, dev->function, 0x04);
    pci_write_word(dev->bus, dev->device, dev->function, 0x04, cmd & ~(0x03));

    pci_write_dword(dev->bus, dev->device, dev->function, offset, 0xFFFFFFFF);
    uint32_t size_val = pci_read_dword(dev->bus, dev->device, dev->function, offset);
    pci_write_dword(dev->bus, dev->device, dev->function, offset, bar_val); /* Restore */

    /* Restore command register */
    pci_write_word(dev->bus, dev->device, dev->function, 0x04, cmd);

    /* Re-enable interrupts */
    __asm__ volatile ("sti");

    if (bar_val & 0x01) {
        /* I/O Port Region */
        dev->bars[bar_index].type = PCI_BAR_TYPE_IO;
        dev->bars[bar_index].address = bar_val & ~0x03;
        uint32_t limit = size_val & ~0x3;
        dev->bars[bar_index].size = ~limit + 1;
        dev->bars[bar_index].prefetchable = 0;
    } else {
        /* Memory Mapped Region */
        int type = (bar_val >> 1) & 0x03;
        dev->bars[bar_index].type = PCI_BAR_TYPE_MMIO;
        dev->bars[bar_index].prefetchable = (bar_val & 0x08) ? 1 : 0;
        
        if (type == 0x00) {
            /* 32-bit MMIO */
            dev->bars[bar_index].address = bar_val & ~0x0F;
            uint32_t limit = size_val & ~0xF;
            dev->bars[bar_index].size = ~limit + 1;
        } else if (type == 0x02) {
            /* 64-bit MMIO - Warning: It spans TWO BARs */
            uint32_t bar_hi = pci_read_dword(dev->bus, dev->device, dev->function, offset + 4);
            
            pci_write_dword(dev->bus, dev->device, dev->function, offset + 4, 0xFFFFFFFF);
            uint32_t size_hi = pci_read_dword(dev->bus, dev->device, dev->function, offset + 4);
            pci_write_dword(dev->bus, dev->device, dev->function, offset + 4, bar_hi); /* Restore */

            dev->bars[bar_index].address = (bar_val & ~0x0F) | ((uint64_t)bar_hi << 32);
            
            uint64_t limit64 = ((uint64_t)size_hi << 32) | (size_val & ~0x0F);
            dev->bars[bar_index].size = ~limit64 + 1;
        } else {
            /* Reserved / Invalid / 16-bit */
            dev->bars[bar_index].type = PCI_BAR_TYPE_NONE;
        }
    }
}

/* Probe specific device/function */
static void pci_check_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read_word(bus, device, function, 0x00);
    if (vendor_id == 0xFFFF) return; /* Device doesn't exist */

    if (pci_device_count >= MAX_PCI_DEVICES) return;

    pci_device_t *dev = &pci_device_list[pci_device_count++];
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_read_word(bus, device, function, 0x02);
    
    /* Enable Bus Mastering */
    uint16_t command = pci_read_word(bus, device, function, 0x04);
    pci_write_word(bus, device, function, 0x04, command | (1 << 2));
    
    dev->revision_id = pci_read_byte(bus, device, function, 0x08);
    dev->prog_if = pci_read_byte(bus, device, function, 0x09);
    dev->subclass_code = pci_read_byte(bus, device, function, 0x0A);
    dev->class_code = pci_read_byte(bus, device, function, 0x0B);
    
    uint8_t header_type = pci_read_byte(bus, device, function, 0x0E);
    dev->header_type = header_type & 0x7F;
    dev->multi_function = (header_type & 0x80) ? 1 : 0;

    dev->irq_line = pci_read_byte(bus, device, function, 0x3C);
    dev->irq_pin = pci_read_byte(bus, device, function, 0x3D);

    /* Read BARs for Type 00h (standard devices) */
    if (dev->header_type == 0x00) {
        for (int i = 0; i < 6; i++) {
            pci_check_bar(dev, i, 0x10 + (i * 4));
            
            /* If this was a 64-bit BAR, skip the next BAR */
            if (dev->bars[i].type == PCI_BAR_TYPE_MMIO && 
               ((pci_read_dword(bus, device, function, 0x10 + (i * 4)) >> 1) & 0x03) == 0x02) {
                i++; 
                if (i < 6) {
                    dev->bars[i].type = PCI_BAR_TYPE_NONE;
                }
            }
        }
    }

    const char *class_str = pci_get_class_name(dev->class_code, dev->subclass_code);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[PCI] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("%02x:%02x.%x %04x:%04x (%s)\n", 
            bus, device, function, vendor_id, dev->device_id, class_str);

    /* Push the newly discovered device into the PnP manager */
    driver_probe_device(dev);

    /* If this is a PCI-to-PCI bridge, scan the secondary bus */
    if (dev->class_code == 0x06 && dev->subclass_code == 0x04) {
        uint8_t secondary_bus = pci_read_byte(bus, device, function, 0x19);
        pci_scan_bus(secondary_bus);
    }
}

static void pci_check_device(uint8_t bus, uint8_t device) {
    uint16_t vendor_id = pci_read_word(bus, device, 0, 0x00);
    if (vendor_id == 0xFFFF) return; /* Slot is empty */

    pci_check_function(bus, device, 0);

    uint8_t header_type = pci_read_byte(bus, device, 0, 0x0E);
    if ((header_type & 0x80) != 0) {
        /* Multi-function device, check remaining functions */
        for (uint8_t func = 1; func < 8; func++) {
            if (pci_read_word(bus, device, func, 0x00) != 0xFFFF) {
                pci_check_function(bus, device, func);
            }
        }
    }
}

void pci_scan_bus(uint8_t bus) {
    for (uint8_t device = 0; device < 32; device++) {
        pci_check_device(bus, device);
    }
}

void pci_init(void) {
    pci_device_count = 0;

    /* Start recursive scan from Bus 0 */
    pci_scan_bus(0);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("Discovered %d PCI devices\n", pci_device_count);
}

pci_device_t *pci_get_devices(int *count_out) {
    if (count_out) *count_out = pci_device_count;
    return pci_device_list;
}

/* Helper to map ID numbers to nice strings for the log */
const char *pci_get_class_name(int class_code, int subclass_code) {
    switch (class_code) {
        case PCI_CLASS_STORAGE:
            if (subclass_code == PCI_SUBCLASS_STORAGE_IDE) return "IDE Controller";
            if (subclass_code == PCI_SUBCLASS_STORAGE_ATA) return "ATA Controller";
            if (subclass_code == PCI_SUBCLASS_STORAGE_SATA) return "SATA Controller";
            if (subclass_code == PCI_SUBCLASS_STORAGE_NVME) return "NVMe Controller";
            return "Mass Storage";
        case PCI_CLASS_NETWORK:
            return "Network Controller";
        case PCI_CLASS_DISPLAY:
            if (subclass_code == PCI_SUBCLASS_DISPLAY_VGA) return "VGA Display";
            return "Display Controller";
        case PCI_CLASS_BRIDGE:
            if (subclass_code == PCI_SUBCLASS_BRIDGE_HOST) return "Host Bridge";
            if (subclass_code == PCI_SUBCLASS_BRIDGE_ISA) return "ISA Bridge";
            if (subclass_code == PCI_SUBCLASS_BRIDGE_PCI) return "PCI-PCI Bridge";
            return "Bridge";
        case PCI_CLASS_SERIAL_BUS:
            if (subclass_code == PCI_SUBCLASS_SERIAL_USB) return "USB Controller";
            if (subclass_code == PCI_SUBCLASS_SERIAL_SMBUS) return "SMBus";
            return "Serial Bus";
        case PCI_CLASS_MEMORY:
            return "Memory Controller";
        case PCI_CLASS_BASE_SYSTEM:
            return "Base System Peripheral";
        case PCI_CLASS_MULTIMEDIA:
            return "Multimedia Controller";
        default:
            return "Unknown Device";
    }
}
