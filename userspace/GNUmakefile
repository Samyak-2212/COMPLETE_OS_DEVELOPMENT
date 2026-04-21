# userspace/GNUmakefile — Build all userspace ELFs for NexusOS
# Usage: make all    → builds libc, init, shell
#        make clean  → removes build artifacts

TOOLCHAIN  ?= x86_64-elf-
CC          = $(TOOLCHAIN)gcc
AS          = nasm
LD          = $(TOOLCHAIN)ld
AR          = $(TOOLCHAIN)ar

# Userspace compiler flags — NOT kernel flags (no -ffreestanding, etc.)
CFLAGS = -std=gnu11 -Wall -Wextra -Werror \
         -nostdlib -nostdinc -fno-builtin \
         -I $(CURDIR)/libc/include \
         -fno-stack-protector \
         -m64 -march=x86-64 \
         -mno-red-zone \
         -fno-pic -static \
         -Os -ffunction-sections -fdata-sections

LDFLAGS = -static -nostdlib --gc-sections

BUILD_DIR = build

export CC AS LD AR CFLAGS LDFLAGS BUILD_DIR

.PHONY: all clean libc init shell setup

all: setup libc init shell

setup:
	@mkdir -p $(BUILD_DIR)

libc: setup
	$(MAKE) -C libc

init: libc setup
	$(MAKE) -C init

shell: libc setup
	$(MAKE) -C shell

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C init clean
	$(MAKE) -C shell clean
	rm -rf $(BUILD_DIR)
