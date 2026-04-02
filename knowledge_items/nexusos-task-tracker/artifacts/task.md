# NexusOS Development — Task Tracker

## Phase 0: Initialization ✅
- [x] Examine repository, research Limine protocol
- [x] Create architecture specification
- [x] Update for GUI readiness, multi-FS, Linux shell commands
- [x] User review of updated architecture

## Phase 1: Limine Boot + Hello World ✅
- [x] 1.1 Build system (GNUmakefile, linker script, get-deps)
- [x] 1.2 Limine config
- [x] 1.3 Limine request structs
- [x] 1.4 String utilities
- [x] 1.5 kprintf + framebuffer console
- [x] 1.6 Kernel entry (kmain)
- [x] 1.V Verification — ISO builds, 0 warnings, QEMU boots

## Phase 2: Core Kernel Infrastructure ✅
- [x] 2.1 I/O port helpers
- [x] 2.2 GDT + TSS (IST1 for double-fault)
- [x] 2.3 IDT + ISR stubs (256 entries, iretq)
- [x] 2.4 PIC remapping (IRQ0→INT32)
- [x] 2.5 PIT timer (1ms)
- [x] 2.6 PMM (bitmap allocator)
- [x] 2.7 VMM (4-level paging, reuses Limine tables)
- [x] 2.8 Kernel heap (free-list with coalescing)
- [x] 2.9 Spinlocks
- [x] 2.10 PS/2 controller
- [x] 2.11 PS/2 keyboard driver (scancode set 1)
- [x] 2.12 PS/2 mouse driver (3/4-byte packets)
- [x] 2.13 Input manager (256-entry ring buffer)
- [x] 2.V Verification — builds 0 errors, ISO generated, keyboard echo works

## Phase 3: PCI, Storage, Filesystem, Terminal, Shell ← NEXT
- [ ] 3.1 Driver interface + PnP manager
- [ ] 3.2 PCI enumeration
- [ ] 3.3 ATA PIO driver
- [ ] 3.4 AHCI driver
- [ ] 3.5 VFS + ramfs (root filesystem)
- [ ] 3.6 Partition detection (MBR/GPT)
- [ ] 3.7 FAT32 driver
- [ ] 3.8 NTFS driver (read-only)
- [ ] 3.9 ext2/ext4 driver (read-only)
- [ ] 3.10 Terminal console (VT100)
- [ ] 3.11 Display manager (terminal↔GUI switching)
- [ ] 3.12 Kernel shell (21 Linux-like commands)
- [ ] 3.13 devfs (/dev)
- [ ] 3.V Verification

## Phase 4: Multitasking, USB, Userspace
- [ ] 4.1 Process/thread model
- [ ] 4.2 Context switch + scheduler
- [ ] 4.3 System calls
- [ ] 4.4 User-mode transition
- [ ] 4.5 Minimal libc
- [ ] 4.6 Init process + userspace shell
- [ ] 4.7 xHCI host controller
- [ ] 4.8 USB enumeration
- [ ] 4.9 USB HID driver
- [ ] 4.V Verification

## Phase 5: Polish & Physical Hardware
- [ ] 5.1 ACPI (shutdown/reboot)
- [ ] 5.2 APIC (replace PIC)
- [ ] 5.3 Physical hardware testing
- [ ] 5.V Verification

## Phase 6: GUI / Desktop Environment (RESERVED — Userspace)
- [ ] 6.1 Userspace API bridge (mmap /dev/fb0)
- [ ] 6.2 Display Server
- [ ] 6.3 Window Compositor
- [ ] 6.4 GUI Apps
- [ ] 6.V Verification
