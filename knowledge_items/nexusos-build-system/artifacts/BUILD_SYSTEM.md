# Build System Protocol

This document provides definitive instructions for building and running NexusOS. All agents must follow these protocols when modifying the build system or adding new hardware boot modes.

## 1. Core Build Commands

| Command | Action |
| :--- | :--- |
| `make <mode>` | Builds the OS binary and ISO (e.g., `make bios`, `make uefi`). |
| `make run <mode>` | Builds AND launches the OS (e.g., `make run bios`). |
| `make run` | Builds and launches in **UEFI** mode (default). |
| `make all` | Builds the default ISO (`nexusos-x86_64.iso`). |
| `make clean` | Removes object files and binaries for the current config. |
| `make distclean` | Deep clean of all configurations and dependencies. |

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
- `make bios`: Builds the BIOS bootable ISO.
- `make run bios`: Builds and launches QEMU in BIOS mode.
- `make run bios-ntfs`: Launches BIOS mode with `ntfs_test.img` attached.
- `make run bios-noscreen`: Launches BIOS mode without a graphical window.

### UEFI Targets
- `make uefi`: Builds the UEFI bootable ISO.
- `make run uefi`: Builds and launches QEMU in UEFI mode.
- `make run uefi-ntfs`: Launches UEFI mode with `ntfs_test.img` attached.
- `make run uefi-noscreen`: Launches UEFI mode without a graphical window.

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
