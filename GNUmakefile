# ============================================================================
# NexusOS — Root GNUmakefile
# Purpose: Build bootable ISO/HDD images, manage Limine, run QEMU.
#
# KEY BEHAVIOURS:
#   make all              → builds all 3 kernels (release, hidbug, lodbug)
#                           and packs a multi-kernel ISO with full boot menu.
#   make run bios         → build all + boot ISO (shows menu, 5s timeout)
#   make run bios hidbug  → build all + boot ISO auto-selecting hidbug kernel
#   make run bios lodbug  → build all + boot ISO auto-selecting lodbug kernel
#   make run bios-noscreen hidbug → headless QEMU + serial stdio (for agents)
#   make clean            → wipe all build artefacts
# ============================================================================

.SUFFIXES:

ARCH         := x86_64
BUILD_CONFIG := release

# ── Debug level for 'run' target overlay ────────────────────────────────────
# Default: multi-kernel menu (DEBUG_LEVEL=all).
# If hidbug/lodbug present on command line, QEMU uses matching limine.conf.in
RUN_LEVEL := 0
ifneq ($(filter lodbug,$(MAKECMDGOALS)),)
    RUN_LEVEL := 2
else ifneq ($(filter hidbug,$(MAKECMDGOALS)),)
    RUN_LEVEL := 1
endif

# Kernel binary names (inside iso)
KERN_RELEASE := nexus-kernel
KERN_HID     := nexus-kernel-hid
KERN_LOD     := nexus-kernel-lod

override IMAGE_NAME := nexusos-$(ARCH)

# ── Run detection ────────────────────────────────────────────────────────────
SHOULD_RUN := 0
ifneq ($(filter run run-%,$(MAKECMDGOALS)),)
    SHOULD_RUN := 1
endif

# ── QEMU ────────────────────────────────────────────────────────────────────
HOST_CC      := cc
HOST_CFLAGS  := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS    :=

QEMUFLAGS :=
QEMU      := qemu-system-$(ARCH)

# ── QEMU serial ──────────────────────────────────────────────────────────────
# Always -serial stdio. Serial activity is controlled by which kernel boots:
#   dbg0 (release)  → kernel never touches serial → silent
#   dbg1 (hidbug)   → kernel logs boot + shell to serial
#   dbg2 (lodbug)   → kernel opens interactive nxdbg> console on serial
# QEMU must never null-route serial — user/agent may pick any kernel variant.
QEMU_COMMON          := -M q35 -m 2G -serial stdio -vga std $(QEMUFLAGS)
QEMU_COMMON_NOSCREEN := -M q35 -m 2G -serial stdio -vga std $(QEMUFLAGS)
QEMU_BIOS_FLAGS := -boot d -cdrom $(IMAGE_NAME).iso
QEMU_UEFI_FLAGS := -drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
                   -drive if=pflash,unit=1,format=raw,file=.build/ovmf/ovmf-vars.fd \
                   -cdrom $(IMAGE_NAME).iso

.build/ovmf/ovmf-vars.fd: edk2-ovmf
	@mkdir -p .build/ovmf
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd .build/ovmf/ovmf-vars.fd

# ── Default target ───────────────────────────────────────────────────────────
.PHONY: all all-hdd
all: $(IMAGE_NAME).iso
all-hdd: $(IMAGE_NAME).hdd

# ── Pseudo targets (consumed by RUN_LEVEL detection) ─────────────────────────
.PHONY: hidbug lodbug
hidbug:
	@:
lodbug:
	@:

# ── Kernel builds (all 3 levels, incremental) ────────────────────────────────
# Only recompiles changed files. Each level has its own obj dir.
kernel/.deps-obtained:
	./kernel/get-deps

.PHONY: kernel-all kernel-release kernel-hid kernel-lod

kernel-release: kernel/.deps-obtained
	$(MAKE) -C kernel BUILD_CONFIG=$(BUILD_CONFIG) DEBUG_LEVEL=0

kernel-hid: kernel/.deps-obtained
	$(MAKE) -C kernel BUILD_CONFIG=$(BUILD_CONFIG) DEBUG_LEVEL=1

kernel-lod: kernel/.deps-obtained
	$(MAKE) -C kernel BUILD_CONFIG=$(BUILD_CONFIG) DEBUG_LEVEL=2

kernel-all: kernel-release kernel-hid kernel-lod

# ── ISO generation (multi-kernel, full boot menu) ────────────────────────────
# Uses limine-multi.conf.in for real hardware (5s timeout, menu).
# All 3 kernels always present in ISO.
$(IMAGE_NAME).iso: limine/limine kernel-all
	rm -rf iso_root
	mkdir -p iso_root/boot
	# Copy all 3 kernels with distinct names
	cp -v kernel/bin-$(ARCH)-dbg0/kernel iso_root/boot/$(KERN_RELEASE)
	cp -v kernel/bin-$(ARCH)-dbg1/kernel iso_root/boot/$(KERN_HID)
	cp -v kernel/bin-$(ARCH)-dbg2/kernel iso_root/boot/$(KERN_LOD)
	mkdir -p iso_root/boot/limine
	# Use multi-kernel menu config for real hardware
	cp -v limine-multi.conf.in iso_root/boot/limine/limine.conf
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

# ── QEMU-run helper: rebuild ISO with a specific limine.conf ─────────────────
# Called by run targets when a debug level is specified.
# Patches the ISO's limine.conf to auto-boot the right kernel (timeout: 0).
# Uses the existing kernel binaries — no recompilation.
define PATCH_ISO_CONF
	rm -rf iso_root_patch
	mkdir -p iso_root_patch/boot iso_root_patch/boot/limine iso_root_patch/EFI/BOOT
	cp kernel/bin-$(ARCH)-dbg0/kernel iso_root_patch/boot/$(KERN_RELEASE)
	cp kernel/bin-$(ARCH)-dbg1/kernel iso_root_patch/boot/$(KERN_HID)
	cp kernel/bin-$(ARCH)-dbg2/kernel iso_root_patch/boot/$(KERN_LOD)
	cp limine-dbg$(1).conf.in               iso_root_patch/boot/limine/limine.conf
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root_patch/boot/limine/
	cp limine/BOOTX64.EFI  iso_root_patch/EFI/BOOT/
	cp limine/BOOTIA32.EFI iso_root_patch/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root_patch -o $(IMAGE_NAME).iso
	./limine/limine bios-install $(IMAGE_NAME).iso
	rm -rf iso_root_patch
endef

# ── Boot targets ─────────────────────────────────────────────────────────────
.PHONY: bios uefi bios-noscreen uefi-noscreen bios-ntfs uefi-ntfs bios-ntfs-noscreen uefi-ntfs-noscreen

bios: $(IMAGE_NAME).iso
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS)
endif

uefi: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS)
endif

bios-noscreen: $(IMAGE_NAME).iso
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON_NOSCREEN) $(QEMU_BIOS_FLAGS) -display none
endif

uefi-noscreen: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON_NOSCREEN) $(QEMU_UEFI_FLAGS) -display none
endif

bios-ntfs: $(IMAGE_NAME).iso
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found."; exit 1; fi
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON) $(QEMU_BIOS_FLAGS) -hdb ntfs_test.img
endif

uefi-ntfs: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found."; exit 1; fi
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON) $(QEMU_UEFI_FLAGS) -hdb ntfs_test.img
endif

bios-ntfs-noscreen: $(IMAGE_NAME).iso
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found."; exit 1; fi
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON_NOSCREEN) $(QEMU_BIOS_FLAGS) -hdb ntfs_test.img -display none
endif

uefi-ntfs-noscreen: $(IMAGE_NAME).iso .build/ovmf/ovmf-vars.fd
	@if [ "$(SHOULD_RUN)" = "1" ] && [ ! -f ntfs_test.img ]; then echo "WARNING: ntfs_test.img not found."; exit 1; fi
ifeq ($(SHOULD_RUN),1)
  ifneq ($(RUN_LEVEL),0)
	$(call PATCH_ISO_CONF,$(RUN_LEVEL))
  endif
	$(QEMU) $(QEMU_COMMON_NOSCREEN) $(QEMU_UEFI_FLAGS) -hdb ntfs_test.img -display none
endif

# ── Launch targets ───────────────────────────────────────────────────────────
.PHONY: run run-%
run:
	@if [ "$(MAKECMDGOALS)" = "run" ]; then $(MAKE) uefi SHOULD_RUN=1; fi

run-%: % ;

# ── External dependencies ─────────────────────────────────────────────────────
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

# ── HDD/USB image (multi-kernel) ─────────────────────────────────────────────
$(IMAGE_NAME).hdd: limine/limine kernel-all
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=128 of=$(IMAGE_NAME).hdd
	PATH=$$PATH:/usr/sbin:/sbin sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00 -m 1
	./limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M -v "NEXUS_OS"
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin-$(ARCH)-dbg0/kernel ::/boot/$(KERN_RELEASE)
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin-$(ARCH)-dbg1/kernel ::/boot/$(KERN_HID)
	mcopy -i $(IMAGE_NAME).hdd@@1M kernel/bin-$(ARCH)-dbg2/kernel ::/boot/$(KERN_LOD)
	mcopy -i $(IMAGE_NAME).hdd@@1M limine-multi.conf.in ::/boot/limine/limine.conf
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/limine-bios.sys ::/boot/limine
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTX64.EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M limine/BOOTIA32.EFI ::/EFI/BOOT

# ── ext4 test image ──────────────────────────────────────────────────────────
ext4.img:
	rm -f ext4.img ext4_part.img
	dd if=/dev/zero bs=1M count=63 of=ext4_part.img
	mkfs.ext4 -q -F ext4_part.img
	dd if=/dev/zero bs=1M count=64 seek=64 of=ext4.img
	parted -s ext4.img mklabel msdos mkpart primary ext4 1MiB 64MiB
	dd if=ext4_part.img of=ext4.img bs=512 seek=2048 conv=notrunc
	rm -f ext4_part.img

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean distclean
clean:
	$(MAKE) -C kernel clean
	rm -rf iso_root iso_root_patch $(IMAGE_NAME).iso $(IMAGE_NAME).hdd ext4.img ext4_part.img

distclean:
	$(MAKE) -C kernel distclean
	rm -rf iso_root iso_root_patch *.iso *.hdd limine edk2-ovmf .build
