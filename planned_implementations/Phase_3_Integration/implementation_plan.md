# Implementation Plan — Phase 3: Storage, VFS, and Kernel Shell

This plan covers the integration and verification of Phase 3 components which are currently in a "partially implemented but inactive" state. The goal is to move from a basic keyboard-echo kernel to a system with functional PCI scanning, disk access, a virtual filesystem, and an interactive Linux-like shell.

## User Review Required

> [!IMPORTANT]
> The implementation agent will need to uncomment several lines in `kernel.c` and potentially fix IRQ conflicts between the PIC and the newly enabled storage/PCI subsystems.

> [!WARNING]
> The current FAT32 and ext4 drivers are marked as "naive" (reading only the first cluster/block). Full file system support will require follow-up projects once the basic integration is verified.

## Proposed Changes

### 1. Driver Management & PCI
Activate the Plug-and-Play (PnP) system.

#### [MODIFY] [driver_manager.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/driver_manager.c)
- Ensure `driver_register` and `driver_probe_device` are robust.
- Link PCI device discovery to the driver registry.

#### [MODIFY] [pci.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/pci/pci.c)
- Expand scan range beyond Bus 0 if necessary.
- Verify BAR decoding for 64-bit MMIO.

---

### 2. Storage & Partitions
Enable disk detection and partition table parsing.

#### [MODIFY] [ata.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/drivers/storage/ata.c)
- Ensure PIO reads are stable and handle timeouts correctly.
- Verify LBA48 support for large disks.

#### [MODIFY] [partition.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/partition.c)
- Finalize MBR and GPT parsing logic.
- Populate the system-wide partition list.

---

### 3. VFS & Filesystems
Establish the directory hierarchy and mount physical disks.

#### [MODIFY] [vfs.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/vfs.c)
- Implement path resolution (`vfs_resolve_path`) to handle `/mnt/hda1/...` lookups.
- Add support for mounting filesystems at specific points.

#### [MODIFY] [ramfs.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/fs/ramfs.c)
- Complete `mkdir` and `touch` (file creation) logic.

---

### 4. Kernel Shell (Interactive)
The primary interface for this phase.

#### [NEW] [terminal_shell.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/terminal_shell.c)
- Implement command dispatch for `ls`, `cat`, `echo`, `mkdir`, `cd`, `pwd`, etc.
- Implement the "Linux-like" command behavior specified in the architecture.

---

### 5. Kernel Entry Integration
Hook everything together.

#### [MODIFY] [kernel.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/kernel.c)
- Uncomment Phase 3 initialization calls.
- Transition from "keyboard echo" to "kernel shell loop".

## Verification Plan

### Automated Tests
- `make all`: Verify 0 errors, 0 C warnings.
- `make run`: Boot in QEMU with a secondary disk image to test storage.

### Manual Verification
1. **PCI Scan**: Check serial logs or screen for "Discovered X PCI devices".
2. **Disk Detection**: Verify "ATA Drive found" message appears.
3. **Shell Interaction**:
   - `ls /` -> Expect `dev/`, `tmp/`, `mnt/`.
   - `echo "Hello" > /tmp/test.txt`
   - `cat /tmp/test.txt` -> Expect "Hello".
   - `ls /mnt/hda1` (if disk provided) -> Expect files from disk image.
