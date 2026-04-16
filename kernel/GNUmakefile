# ============================================================================
# NexusOS — Kernel GNUmakefile
# Purpose: Compile and link the NexusOS kernel ELF binary
# ============================================================================

# Nuke built-in rules.
.SUFFIXES:

# Output binary name.
override OUTPUT := kernel

# Target architecture.
ARCH := x86_64

# Configuration (release by default)
BUILD_CONFIG ?= release

# ── Toolchain ───────────────────────────────────────────────────────────────
TOOLCHAIN :=
TOOLCHAIN_PREFIX :=
ifneq ($(TOOLCHAIN),)
    ifeq ($(TOOLCHAIN_PREFIX),)
        TOOLCHAIN_PREFIX := $(TOOLCHAIN)-
    endif
endif

ifneq ($(TOOLCHAIN_PREFIX),)
    CC := $(TOOLCHAIN_PREFIX)gcc
else
    CC := cc
endif

LD := $(TOOLCHAIN_PREFIX)ld

ifeq ($(TOOLCHAIN),llvm)
    CC := clang
    LD := ld.lld
endif

# ── User-controllable flags ────────────────────────────────────────────────
CFLAGS := -g -O2 -pipe
CPPFLAGS :=
NASMFLAGS := -g
LDFLAGS :=

# ── Dependency check ───────────────────────────────────────────────────────
ifneq ($(filter-out clean distclean,$(MAKECMDGOALS)),)
    ifeq ($(wildcard .deps-obtained),)
        $(error Please run the ./get-deps script first)
    endif
endif

# ── Detect Clang ────────────────────────────────────────────────────────────
override CC_IS_CLANG := $(shell ! $(CC) --version 2>/dev/null | grep -q '^Target: '; echo $$?)

# ── Internal C flags (DO NOT MODIFY) ───────────────────────────────────────
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -fno-omit-frame-pointer

# ── Debug level ────────────────────────────────────────────────────────────
DEBUG_LEVEL ?= 0
ifneq ($(DEBUG_LEVEL),0)
    override CFLAGS += -DDEBUG_LEVEL=$(DEBUG_LEVEL)
endif

# ── Internal preprocessor flags ────────────────────────────────────────────
override CPPFLAGS := \
    -I src \
    -I limine-protocol/include \
    -isystem freestnd-c-hdrs/include \
    $(CPPFLAGS) \
    -MMD \
    -MP

# Add debugger include path when enabled
ifneq ($(DEBUG_LEVEL),0)
    override CPPFLAGS += -I ../debugger/include
endif

# ── Internal NASM flags ────────────────────────────────────────────────────
override NASMFLAGS := \
    $(patsubst -g,-g -F dwarf,$(NASMFLAGS)) \
    -Wall

# ── x86_64-specific flags ──────────────────────────────────────────────────
ifeq ($(CC_IS_CLANG),1)
    override CC += \
        -target x86_64-unknown-none-elf
endif
override CFLAGS += \
    -m64 \
    -march=x86-64 \
    -mabi=sysv \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel
override LDFLAGS += \
    -m elf_x86_64
override NASMFLAGS := \
    -f elf64 \
    $(NASMFLAGS)

# ── Internal linker flags ──────────────────────────────────────────────────
override LDFLAGS += \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    --gc-sections \
    -T linker-scripts/$(ARCH).lds

# ── Source file discovery ───────────────────────────────────────────────────
override SRCFILES := $(shell find -L src cc-runtime/src -type f -not -path 'src/arch/*' 2>/dev/null | LC_ALL=C sort)
override SRCFILES += $(shell find -L src/arch/$(ARCH) -type f 2>/dev/null | LC_ALL=C sort)

# Include debugger sources when enabled (normalize path to avoid ../ escape)
ifneq ($(DEBUG_LEVEL),0)
    override DBG_RAW := $(shell find -L ../debugger/src -type f 2>/dev/null | LC_ALL=C sort)
    override DBG_NORM := $(patsubst ../debugger/%,debugger/%,$(DBG_RAW))
    override SRCFILES += $(DBG_NORM)
endif

# vpath: let make find debugger/ sources at ../debugger/ on disk
vpath debugger/% ..

# ── Object file lists ──────────────────────────────────────────────────────
override CFILES := $(filter %.c,$(SRCFILES))
override ASFILES := $(filter %.S,$(SRCFILES))
override NASMFILES := $(filter %.asm,$(SRCFILES))
override OBJ := $(addprefix obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/,$(CFILES:.c=.c.o) $(ASFILES:.S=.S.o))
override OBJ += $(addprefix obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/,$(NASMFILES:.asm=.asm.o))
override HEADER_DEPS := $(addprefix obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/,$(CFILES:.c=.c.d) $(ASFILES:.S=.S.d))

# ── Default target ──────────────────────────────────────────────────────────
.PHONY: all
all: bin-$(ARCH)-dbg$(DEBUG_LEVEL)/$(OUTPUT)

# ── Include generated dependencies ─────────────────────────────────────────
-include $(HEADER_DEPS)

# ── Link ────────────────────────────────────────────────────────────────────
bin-$(ARCH)-dbg$(DEBUG_LEVEL)/$(OUTPUT): GNUmakefile linker-scripts/$(ARCH).lds $(OBJ)
	mkdir -p "$(dir $@)"
	$(LD) $(LDFLAGS) $(OBJ) -o $@

# ── Compile C ───────────────────────────────────────────────────────────────
obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/%.c.o: %.c GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ── Compile GNU assembly ───────────────────────────────────────────────────
obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/%.S.o: %.S GNUmakefile
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# ── Assemble NASM ───────────────────────────────────────────────────────────
obj-$(ARCH)-$(BUILD_CONFIG)-dbg$(DEBUG_LEVEL)/%.asm.o: %.asm GNUmakefile
	mkdir -p "$(dir $@)"
	nasm $(NASMFLAGS) $< -o $@

# ── Clean ───────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	rm -rf bin-$(ARCH)-* obj-$(ARCH)-*
	find ../debugger -name '*.o' -o -name '*.d' | xargs rm -f 2>/dev/null; true

.PHONY: distclean
distclean:
	rm -rf bin-* obj-* .deps-obtained .cache compile_commands.json freestnd-c-hdrs cc-runtime limine-protocol
