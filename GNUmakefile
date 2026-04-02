# ============================================================================
# NexusOS — Root GNUmakefile
# Purpose: Build bootable ISO/HDD images, manage Limine, run QEMU
# ============================================================================

# Nuke built-in rules.
.SUFFIXES:

# Target architecture (x86_64 only for NexusOS).
ARCH := x86_64

# Default QEMU flags.
QEMUFLAGS := -m 2G -serial stdio

override IMAGE_NAME := nexusos-$(ARCH)

# Host compiler for building Limine CLI tool.
HOST_CC := cc
HOST_CFLAGS := -g -O2 -pipe
HOST_CPPFLAGS :=
HOST_LDFLAGS :=
HOST_LIBS :=

# ── Default target ──────────────────────────────────────────────────────────
.PHONY: all
all: $(IMAGE_NAME).iso

.PHONY: all-hdd
all-hdd: $(IMAGE_NAME).hdd

# ── QEMU run targets ───────────────────────────────────────────────────────
.PHONY: run
run: edk2-ovmf $(IMAGE_NAME).iso
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd /tmp/ovmf-vars-run.fd
	qemu-system-$(ARCH) \
		-M q35 \
		-vga std \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=/tmp/ovmf-vars-run.fd \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-bios
run-bios: $(IMAGE_NAME).iso
	qemu-system-$(ARCH) \
		-M q35 \
		-cdrom $(IMAGE_NAME).iso \
		-boot d \
		$(QEMUFLAGS)

.PHONY: run-uefi
run-uefi: edk2-ovmf $(IMAGE_NAME).iso
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd /tmp/ovmf-vars-uefi.fd
	qemu-system-$(ARCH) \
		-M q35 \
		-vga std \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=/tmp/ovmf-vars-uefi.fd \
		-cdrom $(IMAGE_NAME).iso \
		$(QEMUFLAGS)

.PHONY: run-hdd
run-hdd: edk2-ovmf $(IMAGE_NAME).hdd
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd /tmp/ovmf-vars-hdd.fd
	qemu-system-$(ARCH) \
		-M q35 \
		-vga std \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=/tmp/ovmf-vars-hdd.fd \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-uefi-hdd
run-uefi-hdd: edk2-ovmf $(IMAGE_NAME).hdd
	cp edk2-ovmf/ovmf-vars-$(ARCH).fd /tmp/ovmf-vars-uefi-hdd.fd
	qemu-system-$(ARCH) \
		-M q35 \
		-vga std \
		-drive if=pflash,unit=0,format=raw,file=edk2-ovmf/ovmf-code-$(ARCH).fd,readonly=on \
		-drive if=pflash,unit=1,format=raw,file=/tmp/ovmf-vars-uefi-hdd.fd \
		-hda $(IMAGE_NAME).hdd \
		$(QEMUFLAGS)

.PHONY: run-hdd-bios
run-hdd-bios: $(IMAGE_NAME).hdd ext4.img
	qemu-system-$(ARCH) \
		-M pc \
		-drive file=$(IMAGE_NAME).hdd,format=raw,index=0,media=disk \
		-drive file=ext4.img,format=raw,index=1,media=disk \
		$(QEMUFLAGS)

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
	$(MAKE) -C kernel

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
