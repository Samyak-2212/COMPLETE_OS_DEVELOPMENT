/* ============================================================================
 * kernel.c — NexusOS Kernel Entry Point
 * Purpose: kmain() — validates boot, initializes all subsystems
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "boot/limine_requests.h"
#include "drivers/video/framebuffer.h"
#include "display/terminal.h"
#include "shell/shell_core.h"
#include "display/display_manager.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "lib/debug.h"
#include "lib/serial.h"
#include "hal/gdt.h"
#include "hal/idt.h"
#include "hal/pic.h"
#include "hal/io.h"
#include "hal/isr.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "drivers/timer/pit.h"
#include "drivers/input/ps2_controller.h"
#include "drivers/input/ps2_keyboard.h"
#include "drivers/input/ps2_mouse.h"
#include "drivers/input/input_event.h"
#include "drivers/pci/pci.h"
#include "drivers/storage/ata.h"
#include "drivers/storage/ahci.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "fs/devfs.h"
#include "fs/partition.h"
#include "fs/fat32.h"
#include "fs/ext4.h"

/* Forward declarations for input manager */
extern void input_manager_init(void);
extern int input_poll_event(input_event_t *out);
extern int input_has_event(void);

/* ── Kernel panic ───────────────────────────────────────────────────────── */

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void kpanic(const char *msg) {
    debug_log(DEBUG_LEVEL_PANIC, "KERNEL", "PANIC: %s\n", msg);
    debug_backtrace();
    kprintf_set_color(0x00FF4444, 0x001A1A2E);
    kprintf("\n!!! KERNEL PANIC: %s\n", msg);
    hcf();
}

/* ── Version info ───────────────────────────────────────────────────────── */

#define NEXUS_VERSION_MAJOR  0
#define NEXUS_VERSION_MINOR  1
#define NEXUS_VERSION_PATCH  0
#define NEXUS_CODENAME       "Genesis"

/* ── Kernel entry point ─────────────────────────────────────────────────── */

void kmain(void) {
    /* ================================================================
     * Phase 1: Boot validation + framebuffer
     * ================================================================ */

    /* 1. Validate base revision */
    if (!limine_base_revision_supported()) {
        hcf();
    }

    /* 2. Initialize framebuffer */
    if (framebuffer_init() != 0) {
        hcf();
    }

    /* 3. Initialize kprintf */
    kprintf_init();
    
    /* 3.5 Initialize Debugger (COM1) */
    debug_init();
    debug_set_mode(DEBUG_MODE_HR);
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "NexusOS Kernel Boot Sequence Started");

    framebuffer_clear(0x001A1A2E);  /* Dark navy blue */

    /* 4. Print boot banner */
    kprintf_set_color(0x0000CCFF, 0x001A1A2E);
    kprintf("  _   _                       ___  ____  \n");
    kprintf(" | \\ | | _____  ___   _ ___  / _ \\/ ___| \n");
    kprintf(" |  \\| |/ _ \\ \\/ / | | / __|/ | | \\___ \\ \n");
    kprintf(" | |\\  |  __/>  <| |_| \\__ \\ |_| |___) |\n");
    kprintf(" |_| \\_|\\___/_/\\_\\\\__,_|___/\\___/|____/ \n");
    kprintf("\n");

    kprintf_set_color(0x00FFFFFF, 0x001A1A2E);
    kprintf("  NexusOS v%d.%d.%d \"%s\" — x86_64 Monolithic Kernel\n\n",
            NEXUS_VERSION_MAJOR, NEXUS_VERSION_MINOR,
            NEXUS_VERSION_PATCH, NEXUS_CODENAME);

    /* 5. Print boot info */
    const framebuffer_info_t *fb = framebuffer_get_info();
    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Framebuffer: %ux%u @ %u BPP\n",
            (unsigned int)fb->width, (unsigned int)fb->height,
            (unsigned int)fb->bpp);

    /* Memory map summary */
    struct limine_memmap_response *memmap = limine_get_memmap();
    if (memmap) {
        uint64_t total_usable = 0;
        for (uint64_t i = 0; i < memmap->entry_count; i++) {
            if (memmap->entries[i]->type == LIMINE_MEMMAP_USABLE) {
                total_usable += memmap->entries[i]->length;
            }
        }
        kprintf_set_color(0x0088FF88, 0x001A1A2E);
        kprintf("[OK] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("Memory: %llu MiB usable (%llu entries)\n",
                (unsigned long long)(total_usable / (1024 * 1024)),
                (unsigned long long)memmap->entry_count);
    } else {
        kpanic("No memory map from bootloader");
    }

    /* Firmware type */
    struct limine_firmware_type_response *fw = limine_get_firmware_type();
    if (fw) {
        const char *fw_type = "Unknown";
        if (fw->firmware_type == LIMINE_FIRMWARE_TYPE_X86BIOS) fw_type = "BIOS";
        else if (fw->firmware_type == LIMINE_FIRMWARE_TYPE_EFI32) fw_type = "UEFI 32-bit";
        else if (fw->firmware_type == LIMINE_FIRMWARE_TYPE_EFI64) fw_type = "UEFI 64-bit";
        kprintf_set_color(0x0088FF88, 0x001A1A2E);
        kprintf("[OK] ");
        kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
        kprintf("Firmware: %s\n", fw_type);
    }

    /* ================================================================
     * Phase 2: Core kernel infrastructure
     * ================================================================ */

    kprintf_set_color(0x00FFCC00, 0x001A1A2E);
    kprintf("\n--- Phase 2: Core Kernel Init ---\n\n");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);

    /* 3. Load our GDT + TSS */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing GDT...");
    gdt_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "GDT initialized.");

    /* 4. Load IDT + ISR stubs */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing IDT...");
    idt_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "IDT initialized.");

    /* 5. Remap PIC */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing PIC...");
    pic_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "PIC remapped.");

    /* 6. Initialize PMM */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing PMM...");
    pmm_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "PMM initialized.");

    /* 7. Initialize VMM */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing VMM...");
    vmm_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "VMM initialized.");

    /* 8. Initialize kernel heap (256 KiB initial) */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing Heap...");
    heap_init(64);  /* 64 pages = 256 KiB */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Heap initialized.");

    /* 9. Initialize PIT timer (1ms tick) */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing PIT...");
    pit_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "PIT initialized.");

    /* 10. Initialize PS/2 controller */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing PS/2 Controller...");
    int ps2_ports = ps2_controller_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "PS/2 Controller initialized (%d ports).", ps2_ports);

    /* 11. Initialize PS/2 keyboard (if port 1 detected) */
    if (ps2_ports & 1) {
        debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing Keyboard...");
        ps2_keyboard_init();
        debug_log(DEBUG_LEVEL_INFO, "BOOT", "Keyboard initialized.");
    }

    /* 12. Initialize PS/2 mouse (if port 2 detected) */
    if (ps2_ports & 2) {
        debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing Mouse...");
        ps2_mouse_init();
        debug_log(DEBUG_LEVEL_INFO, "BOOT", "Mouse initialized.");
    }

    /* 13. Initialize input manager */
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initializing Input Manager...");
    input_manager_init();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Input Manager initialized.");

    /* 14. Enable interrupts and configure APIC Virtual Wire */
    uint64_t apic_base_msr = rdmsr(0x1B);
    apic_base_msr |= (1ULL << 11); /* Ensure APIC is enabled in MSR */
    wrmsr(0x1B, apic_base_msr);
    
    /* Identity map the Local APIC MMIO region (0xFEE00000) */
    /* Flags: Present (1) | Writable (2) | No-Cache (16) */
    uint64_t lapic_phys = 0xFEE00000;
    vmm_map_page(lapic_phys, lapic_phys, 1 | 2 | 16);

    volatile uint32_t *lapic = (volatile uint32_t *)lapic_phys;

    /* Configure LVT LINT0 as ExtInt (bit 8..10 = 111, mode 7). Mask bit 16 = 0. */
    lapic[0x350 / 4] = 0x00000700; 
    /* Enable APIC via Spurious Interrupt Vector Register (bit 8) */
    lapic[0x0F0 / 4] |= (1 << 8);

    debug_log(DEBUG_LEVEL_INFO, "BOOT", "APIC Virtual Wire (LINT0) configured.");
    
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Initial PIC State:");
    pic_dump_state();
    
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Enabling interrupts (sti)...");
    sti();
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Interrupts enabled.");

    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Interrupts enabled (Virtual Wire Mode)\n");

    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);

    /* 15. Mount Root RAM Filesystem */
    vfs_root = ramfs_init();

    /* 16-pre. Mount devfs at /dev (after ramfs creates /dev stub, before storage init) */
    devfs_init();

    /* ================================================================
     * Phase 3: Hardware & Storage Initialization
     * ================================================================ */

    kprintf_set_color(0x00FFCC00, 0x001A1A2E);
    kprintf("\n--- Phase 3: Hardware & Storage Initialization ---\n\n");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);

    /* 16. Driver Registration & Hardware Discovery */
    kprintf("Registering storage drivers...\n");
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Registering storage drivers...");
    ata_init_subsystem();
    ahci_init_subsystem();

    /* 17. PCI Enumeration (Triggers driver probes, partition discovery, and mounting) */
    pci_init(); 

    /* ================================================================
     * Initialization complete — Test and demo
     * ================================================================ */

    kprintf_set_color(0x00FFCC00, 0x001A1A2E);
    kprintf("\n--- Boot Complete ---\n\n");

    /* Test PMM */
    kprintf_set_color(0x00AAAAFF, 0x001A1A2E);
    uint64_t test_page = pmm_alloc_page();
    kprintf("  PMM test: alloc=0x%016llx", (unsigned long long)test_page);
    pmm_free_page(test_page);
    kprintf(" -> freed OK\n");

    /* Test heap */
    void *test_ptr = kmalloc(128);
    kprintf("  Heap test: kmalloc(128)=%p", test_ptr);
    kfree(test_ptr);
    kprintf(" -> kfree OK\n");

    /* Timer test */
    kprintf("  Timer test: ticks=%llu\n", (unsigned long long)pit_get_ticks());

    /* ================================================================
     * Phase 3: Terminal & Shell
     * ================================================================ */

    kprintf_set_color(0x00FFCC00, 0x001A1A2E);
    debug_log(DEBUG_LEVEL_INFO, "BOOT", "Launching Kernel Shell...");
    
    /* Late-allocate terminal backbuffers now that heap is online */
    terminal_allocate_backbuffer(&main_terminal);
    
    /* Clear boot logs for a fresh shell entry */
    terminal_clear(&main_terminal);
    
    /* Main kernel shell — never returns */
    shell_run();
}
