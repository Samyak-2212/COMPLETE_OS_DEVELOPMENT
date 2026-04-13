# Build System Protocol

This document provides definitive instructions for building and running NexusOS. All agents must follow these protocols when modifying the build system or adding new hardware boot modes.

## 1. Core Build Commands

| Command | Action |
| :--- | :--- |
| `make all` | Builds the kernel and bootable ISO image. |
| `make all-hdd` | Builds a bootable GPT HDD image. |
| `make clean` | Removes all config-specific object files and binaries. |
| `make distclean` | Deep clean including external dependencies (Limine, OVMF). |

## 2. Build Configurations (`BUILD_CONFIG`)

NexusOS supports multiple build configurations. The system uses **zero-clean switching**, meaning object files are stored in config-specific directories to avoid redundant recompilation.

- **Defaults**: `BUILD_CONFIG=release`
- **Toggle Debugger**: `make BUILD_CONFIG=debug`
- **Effect**: If NOT in debug mode, all debugger files (`debug.c`, `serial.c`) are stripped from the build.

> [!IMPORTANT]
> When switching between `debug` and `release`, do NOT run `make clean`. The system will automatically use `obj-x86_64-debug/` or `obj-x86_64-release/`.

## 3. Boot Targets

Targets are divided into BIOS and UEFI modes. Each mode supports optional features.

### BIOS Targets
- `make bios`: Standard BIOS boot.
- `make bios-ntfs`: BIOS boot with `ntfs_test.img` attached as `-hdb`.
- `make bios-noscreen`: BIOS boot with `-display none` (use for serial-only diagnostics).
- `make bios-ntfs-noscreen`: Combined NTFS and headless mode.

### UEFI Targets
- `make uefi`: Standard UEFI boot using local OVMF variables.
- `make uefi-ntfs`: UEFI boot with `ntfs_test.img` attached as `-hdb`.
- `make uefi-noscreen`: UEFI boot with `-display none`.
- `make uefi-ntfs-noscreen`: Combined NTFS and headless mode.

> [!NOTE]
> UEFI targets store persistent NVRAM variables in `.build/ovmf/`. This avoids permission issues and keeps the workspace clean.

## 4. Extension Protocol (Adding Boot Targets)

To add a new boot target (e.g., `bios-usb`), follow these steps in the root `GNUmakefile`:

1. **Define Flags**: Add to the QEMU Configuration section.
   ```makefile
   QEMU_USB_FLAGS := -device qemu-xhci -device usb-kbd
   ```
2. **Implement Target**: Add to the Boot Targets section.
   ```makefile
   bios-usb: $(IMAGE_NAME).iso
   	$(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS) $(QEMU_USB_FLAGS)
   ```
3. **Register Phony**: Add the new target to the `.PHONY` list at the top of the Boot Targets section.

## 5. Agent Safety

- **Hardware Structs**: Every kernel struct mapping to hardware MUST use `__attribute__((packed))`.
- **MMIO**: All MMIO access MUST use `volatile` pointers.
- **Warnings**: The build MUST pass with **0 errors and 0 C warnings**.
- **Debugger**: Never include debugger sources in the `release` configuration.
