/* ============================================================================
 * pci_ids.h — NexusOS PCI Vendor and Class Identifiers
 * Purpose: Constants for enumerating PCI bridges, classes, and vendors
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_PCI_IDS_H
#define NEXUS_DRIVERS_PCI_IDS_H

/* Common Vendor IDs */
#define PCI_VENDOR_INTEL    0x8086
#define PCI_VENDOR_AMD      0x1022
#define PCI_VENDOR_NVIDIA   0x10DE
#define PCI_VENDOR_VMWARE   0x15AD
#define PCI_VENDOR_VIRTIO   0x1AF4
#define PCI_VENDOR_QEMU     0x1234
#define PCI_VENDOR_REALTEK  0x10EC

/* Class Codes */
#define PCI_CLASS_UNCLASSIFIED      0x00
#define PCI_CLASS_STORAGE           0x01
#define PCI_CLASS_NETWORK           0x02
#define PCI_CLASS_DISPLAY           0x03
#define PCI_CLASS_MULTIMEDIA        0x04
#define PCI_CLASS_MEMORY            0x05
#define PCI_CLASS_BRIDGE            0x06
#define PCI_CLASS_COMMUNICATION     0x07
#define PCI_CLASS_BASE_SYSTEM       0x08
#define PCI_CLASS_INPUT_DEVICE      0x09
#define PCI_CLASS_DOCKING_STATION   0x0A
#define PCI_CLASS_PROCESSOR         0x0B
#define PCI_CLASS_SERIAL_BUS        0x0C
#define PCI_CLASS_WIRELESS          0x0D
#define PCI_CLASS_INTELLIGENT_IO    0x0E
#define PCI_CLASS_SATELLITE_COMM    0x0F
#define PCI_CLASS_ENCRYPTION        0x10
#define PCI_CLASS_SIGNAL_PROC       0x11
#define PCI_CLASS_PROCESSING_ACCEL  0x12
#define PCI_CLASS_NON_ESSENTIAL     0x13
#define PCI_CLASS_COPROCESSOR       0x40
#define PCI_CLASS_UNASSIGNED        0xFF

/* Storage Subclasses (Class 0x01) */
#define PCI_SUBCLASS_STORAGE_SCSI   0x00
#define PCI_SUBCLASS_STORAGE_IDE    0x01
#define PCI_SUBCLASS_STORAGE_FLOPPY 0x02
#define PCI_SUBCLASS_STORAGE_IPI    0x03
#define PCI_SUBCLASS_STORAGE_RAID   0x04
#define PCI_SUBCLASS_STORAGE_ATA    0x05
#define PCI_SUBCLASS_STORAGE_SATA   0x06
#define PCI_SUBCLASS_STORAGE_SAS    0x07
#define PCI_SUBCLASS_STORAGE_NVME   0x08

/* Display Subclasses (Class 0x03) */
#define PCI_SUBCLASS_DISPLAY_VGA    0x00
#define PCI_SUBCLASS_DISPLAY_XGA    0x01
#define PCI_SUBCLASS_DISPLAY_3D     0x02

/* Bridge Subclasses (Class 0x06) */
#define PCI_SUBCLASS_BRIDGE_HOST    0x00
#define PCI_SUBCLASS_BRIDGE_ISA     0x01
#define PCI_SUBCLASS_BRIDGE_EISA    0x02
#define PCI_SUBCLASS_BRIDGE_MCA     0x03
#define PCI_SUBCLASS_BRIDGE_PCI     0x04
#define PCI_SUBCLASS_BRIDGE_PCMCIA  0x05
#define PCI_SUBCLASS_BRIDGE_NUBUS   0x06
#define PCI_SUBCLASS_BRIDGE_CARDBUS 0x07

/* Serial Bus Subclasses (Class 0x0C) */
#define PCI_SUBCLASS_SERIAL_FIREWIRE 0x00
#define PCI_SUBCLASS_SERIAL_ACCESS   0x01
#define PCI_SUBCLASS_SERIAL_SSA      0x02
#define PCI_SUBCLASS_SERIAL_USB      0x03
#define PCI_SUBCLASS_SERIAL_FIBER    0x04
#define PCI_SUBCLASS_SERIAL_SMBUS    0x05

/* USB Programming Interfaces (ProgIF for Class 0x0C, Subclass 0x03) */
#define PCI_PROGIF_USB_UHCI          0x00
#define PCI_PROGIF_USB_OHCI          0x10
#define PCI_PROGIF_USB_EHCI          0x20
#define PCI_PROGIF_USB_XHCI          0x30
#define PCI_PROGIF_USB_UNSPECIFIED   0x80
#define PCI_PROGIF_USB_DEVICE        0xFE

/* Helper function prototypes for ID names */
const char *pci_get_class_name(int class_code, int subclass_code);

#endif /* NEXUS_DRIVERS_PCI_IDS_H */
