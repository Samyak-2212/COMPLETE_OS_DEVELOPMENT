# Implementation Plan — AHCI DMA and Partition Bugfix

## Goal

Resolve `BUG-003` and `BUG-004` from the NexusOS Bug Pool. 

- **BUG-003**: `ahci_read_sectors` uses `vmm_get_phys()` to translate caller virtual addresses into physical DMA targets. This fails for addresses not present in the manual page walk, causing the HBA to silently DMA data directly into physical address `0x0`.
- **BUG-004**: `partition_parse_mbr` maps a single strict fixed virtual address `0xffffffffb0000000` to perform MBR reads. This is inherently unsafe, prevents concurrency, and triggers BUG-003.

## Background Context

When the system boots, `driver_manager` probes PCI devices and initializes the AHCI controller. The AHCI controller discovers SATA drives and registers them as block devices.
Immediately, `partition_parse_mbr` attempts to read sector 0 (the MBR) to find filesystems. 

Right now, `partition_parse_mbr` uses a fixed virtual address mapped specifically for it. But `ahci_read_sectors` uses `vmm_get_phys()` on the provided virtual address. `vmm_get_phys()` fails to translate the virtual address because the standard page table walk logic doesn't see it (likely due to how Paging is set up in NexusOS natively vs Limine). As a result, the PRDT `dba` field is set to `0x0`.
The AHCI controller is ordered to read 1 sector from the disk, and DMAs the data exactly to physical address 0, while the caller's buffer remains all-zeros. The MBR signature check fails, warning of an invalid MBR.

## Proposed Changes

We will decouple the AHCI DMA boundary from the caller's memory via a **per-port bounce buffer**, and refactor partition reading to use robust `kmalloc` logic similar to other NexusOS VFS constructs.

### 1. Storage Driver (AHCI) — `ahci.c` & `ahci.h`

#### [MODIFY] `kernel/src/drivers/storage/ahci.c`
1. Update `sata_port_info_t` struct to include bounce buffer tracking:
   ```c
   uint64_t bounce_phys;
   void *bounce_virt;
   ```
2. In `ahci_init()`, inside the active port setup loop:
   - Invoke `pmm_alloc_pages(32)` to reserve a contiguous 128KB physical segment (32 pages x 4096 bytes). 128KB covers the maximum ATA DMA size (255 sectors = ~130 KB).
   - Calculate its HHDM base virtual address (`bounce_phys + hhdm`).
   - Store both pointers in the port info structure. Let's map it explicitly if an explicit mapping is needed, but `pmm_alloc_pages` combined with `hhdm` should already be present natively. If the existing driver maps metadata pages via `vmm_map_page`, do the same for the bounce buffer:
     `vmm_map_page(bounce_virt, bounce_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);`
3. In `ahci_read_sectors()`:
   - Replace `cmdtbl->prdt_entry[0].dba = vmm_get_phys((uint64_t)buffer);` with the guaranteed physical target `info->bounce_phys`.
   - After validating that the AHCI DMA event completed successfully (no timeout, no task file errors), issue `memcpy(buffer, info->bounce_virt, count * 512)` to transfer the bounced payload seamlessly into the caller's intended virtual buffer.

### 2. VFS / Partitioning — `partition.c`

#### [MODIFY] `kernel/src/fs/partition.c`
1. In `partition_parse_mbr(ata_drive_t *drive)`:
   - Remove the `pmm_alloc_page()`, `0xffffffffb0000000` hardcoded address, and `vmm_map_page()` implementations completely.
   - Replace it with a standard heap allocation: `uint8_t *sector = kmalloc(4096);`.
   - Ensure `kfree(sector)` is triggered uniformly across all return paths.
2. The `disk_read_sectors(..., sector)` call will safely execute because AHCI is now using its bounce buffer.

## Verification Plan

1. Boot the OS using QEMU:
   `make run-bios QEMUFLAGS='-m 2G -serial stdio -hdb ntfs_test.img'`
2. The filesystem probe logic should parse the disk successfully. The warning `[PART] Warning: Invalid MBR signature on sdb (0x0000)` will be completely gone.
3. The prompt log will signal `[NTFS] Mounted NTFS volume`.
4. Command shell verification:
   `ls /mnt/hdb1` -> Outputs `test.txt  subdir`
