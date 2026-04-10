# Implementation Plan — Agent 1: Storage & PnP Driver Specialist

## Role & Expertise
You are a **Senior Low-Level Driver Engineer**. Your expertise lies in x86_64 PCI enumeration, Bus mastering, and ATA/AHCI PIO/DMA operations. You write high-performance, robust driver code that follows strict hardware specs.

## Objective
Finalize the Plug-and-Play (PnP) driver infrastructure and enable stable disk access.

## Proposed Changes

### 1. Driver Management
#### [MODIFY] [driver_manager.c](file:///c:/Users/naska\OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/driver_manager.c)
- Ensure `driver_register` is thread-safe (use spinlocks if necessary).
- Optimize `driver_probe_device` to support class-based and ID-based matching.

### 2. PCI Subsystem
#### [MODIFY] [pci.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/pci/pci.c)
- Implement recursive bus scanning (Multi-bus support).
- Enable Bus Mastering for discovered PCI devices.
- Integrate with `driver_manager.c` to trigger probes during discovery.

### 3. Storage Drivers
#### [MODIFY] [ata.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/storage/ata.c)
- Implement robust timeout handling for PIO reads.
- Verify LBA48 address translation for drives > 128GB.

#### [MODIFY] [ahci.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/storage/ahci.c)
- Transition from skeleton to HBA identification.
- Map the ABAR (BAR5) to virtual memory using `vmm_map_pages`.

## Verification Plan
1. **Serial Logs**: Verify "PCI Device Found" for every controller on the bus.
2. **Disk Identity**: Verify `ata_identify` returns correct Model/Serial via QEMU logs.
3. **PnP Trace**: Verify `driver_probe_device` successfully hands off an IDE controller to `ata.c`.
