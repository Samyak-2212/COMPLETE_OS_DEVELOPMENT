# ============================================================================
# NexusOS — Root GNUmakefile
# Purpose: Build bootable ISO/HDD images, manage Limine, run QEMU
# ============================================================================

# Nuke built-in rules.
.SUFFIXES:

# Target architecture (x86_64 only for NexusOS).
ARCH := x86_64

# Configuration (release by default)
BUILD_CONFIG := release

# Debugger Toggle (1 = enabled, 0 = disabled)
# Defaults to 1 if BUILD_CONFIG is debug, otherwise 0.
ifeq ($(BUILD_CONFIG),debug)
    DEBUGGER ?= 1
else
    DEBUGGER ?= 0
endif

# Default QEMU flags.
QEMUFLAGS :=

# ── Launch Logic ───────────────────────────────────────────────────────────
# If 'run' or any 'run-*' target is on the command line, we enable QEMU execution.
# This allows 'make bios' to build only, and 'make run bios' to build and run.
SHOULD_RUN := 0
ifneq ($(filter run run-%,$(MAKECMDGOALS)),)
    SHOULD_RUN := 1
endif

# If the ONLY target is 'run', default to 'run uefi'.
ifeq ($(MAKECMDGOALS),run)
    # We will trigger the uefi target with SHOULD_RUN forced to 1.
endif

override IMAGE_NAME := nexusos-$(ARCH)

# Host compiler for building Limine CLI tool.
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

# ── Default targets ─────────────────────────────────────────────────────────
.PHONY: all all-hdd
all: $(IMAGE_NAME).iso
all-hdd: $(IMAGE_NAME).hdd

# ── QEMU Configuration ─────────────────────────────────────────────────────
QEMU := qemu-system-$(ARCH)
QEMU_COMMON := -M q35 -m 2G -serial stdio -vga std $(QEMUFLAGS)
QEMU_BIOS_FLAGS := -boot d -cdrom $(IMAGE_NAME).iso
QEMU_UEFI_FLAGS := -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
                   -drive if=pflash,unit=1,format=raw,file=.build/ovmf/ovmf-vars.fd \
                   -cdrom $(IMAGE_NAME).iso

# Helper to ensure local OVMF vars exist
.build/ovmf/ovmf-vars.fd: edk2-ovmf
	@mkdir -p .build/ovmf
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd .build/ovmf/ovmf-vars.fd

# ── Boot Targets ───────────────────────────────────────────────────────────
.PHONY: bios uefi bios-ntfs uefi-ntfs bios-noscreen uefi-noscreen bios-ntfs-noscreen uefi-ntfs-noscreen

bios: $(IMAGE_NAME).iso
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS))

uefi: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS))

bios-ntfs: $(IMAGE_NAME).iso
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found. Use make_ntfs_img.sh to create it."; exit 1; fi
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS) -hdb ntfs_test.img)

uefi-ntfs: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found. Use make_ntfs_img.sh to create it."; exit 1; fi
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS) -hdb ntfs_test.img)

bios-noscreen: $(IMAGE_NAME).iso
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS) -display none)

uefi-noscreen: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS) -display none)

bios-ntfs-noscreen: $(IMAGE_NAME).iso
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found. Use make_ntfs_img.sh to create it."; exit 1; fi
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS) -hdb ntfs_test.img -display none)

uefi-ntfs-noscreen: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found. Use make_ntfs_img.sh to create it."; exit 1; fi
	$(if $(filter 1,$(SHOULD_RUN)), $(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS) -hdb ntfs_test.img -display none)

# ── Launch Targets ──────────────────────────────────────────────────────────
.PHONY: run run-%
# 'make run' with no target defaults to UEFI boot.
run:
	@if [ "$(MAKECMDGOALS)" = "run" ]; then $(MAKE) uefi SHOULD_RUN=1; fi

# Pattern rule: 'make run-xxx' is equivalent to 'make run xxx'
run-%: % ;

# ── External dependencies ──────────────────────────────────────────────────
edk2-ovmf:
	curl -L https://github.com/osdev0/edk2-ovmf-nightly/releases/latest/download/edk2-ovmf.tar.gz | gunzip | tar -xf -

limine/limine:
	rm -rf limine
	git clone https://codeberg.org/Limine/Limine.git limine --branch=v11.x-binary --depth=1
	$(MAKE) -C limine \
		CC="$(HOST_CC)" \
		CFLAGS="$(HOST_CFLAGS)" \
		CPPFLAGS="$(HOST_CPPFLAGS)" \
		LDFLAGS="$(HOST_LDFLAGS)" \
		LIBS="$(HOST_LIBS)"

# ── Kernel build ────────────────────────────────────────────────────────────
kernel/.deps-obtained:
	./kernel/get-deps

.PHONY: kernel
kernel: kernel/.deps-obtained
	$(MAKE) -C kernel BUILD_CONFIG=$(BUILD_CONFIG) DEBUGGER=$(DEBUGGER)

# ── ISO generation ──────────────────────────────────────────────────────────
$(IMAGE_NAME).iso: limine/limine kernel
	rm -rf iso_root
	mkdir -p iso_root/boot
	cp -v kernel/bin-$(ARCH)/kernel iso_root/boot/
	mkdir -p iso_root/boot/limine
	cp -v limine.conf iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT
	cp -v limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp -v limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp -v limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root

# ── HDD/USB image generation ───────────────────────────────────────────────
$(IMAGE_NAME).hdd: limine/limine kernel
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M -v "NEXUS_OS"
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin-$(ARCH)/kernel ::/boot
	mcopy -i $(IMAGE_NAME).hdd@@1M limine.conf ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT

ext4.img:
	rm -f ext4.img ext4_part.img
	dd if=/dev/zero bs=1M count=63 of=ext4_part.img
	mkfs.ext4 -q -F ext4_part.img
	dd if=/dev/zero bs=1M count=64 seek=64 of=ext4.img
	parted -s ext4.img mklabel msdos mkpart primary ext4 1MiB 64MiB
	dd if=ext4_part.img of=ext4.img bs=512 seek=2048 conv=notrunc
	rm -f ext4_part.img

# ── Clean targets ───────────────────────────────────────────────────────────
.PHONY: clean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root $(IMAGE_NAME).iso $(IMAGE_NAME).hdd ext4.img ext4_part.img

.PHONY: distclean
distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root *.iso *.hdd limine edk2-ovmf
