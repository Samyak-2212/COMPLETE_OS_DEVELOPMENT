/* ============================================================================
 * ahci.h — NexusOS AHCI SATA Storage Driver
 * Purpose: Block device reads over PCI MMIO AHCI Interface
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_STORAGE_AHCI_H
#define NEXUS_DRIVERS_STORAGE_AHCI_H

#include <stdint.h>

/* Global initialization and registry */
void ahci_init_subsystem(void);

#endif /* NEXUS_DRIVERS_STORAGE_AHCI_H */
