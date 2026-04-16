/* ============================================================================
 * dbg_memory.c — NexusOS Debugger Memory Inspector
 * Purpose: Safe memory read/write with 4-level page table validation.
 *          Hex+ASCII dump. Registered as 'mem' and 'memw' commands.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

#include <lib/string.h>

/* ── Forward declarations ─────────────────────────────────────────────────── */

void dbg_serial_printf(const char *fmt, ...);
void dbg_serial_puts(const char *s);
void dbg_serial_putc(char c);

/* ── Page table flags ─────────────────────────────────────────────────────── */

#define PT_PRESENT  (1ULL << 0)
#define PT_PS       (1ULL << 7)   /* Huge page (2MiB at PD level, 1GiB at PDPT) */

/*
 * Read CR3 — the physical address of the PML4 table.
 */
static inline uint64_t read_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/*
 * phys_to_virt — Convert a physical address to a virtual address using
 * the HHDM (Higher Half Direct Map). We call the kernel's accessor.
 */
extern uint64_t vmm_get_hhdm_offset(void);

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + vmm_get_hhdm_offset());
}

/* ── Page table walk ──────────────────────────────────────────────────────── */

/*
 * dbg_is_addr_mapped — Walk the 4-level page table (PML4 → PDPT → PD → PT)
 * to determine if the given virtual address is backed by a present mapping.
 *
 * Handles 1GiB huge pages (PDPT level) and 2MiB huge pages (PD level).
 *
 * This function is safe to call at any time — it only reads page table
 * entries and does not modify state. It does NOT handle 5-level paging
 * (LA57) since NexusOS uses 4-level (48-bit virtual).
 */
bool dbg_is_addr_mapped(uint64_t addr) {
    uint64_t cr3 = read_cr3() & ~0xFFFULL;  /* Mask off flags */

    /* PML4 index = bits [47:39] */
    uint64_t pml4_idx = (addr >> 39) & 0x1FF;
    volatile uint64_t *pml4 = (volatile uint64_t *)phys_to_virt(cr3);
    uint64_t pml4e = pml4[pml4_idx];
    if (!(pml4e & PT_PRESENT)) return false;

    /* PDPT index = bits [38:30] */
    uint64_t pdpt_phys = pml4e & 0x000FFFFFFFFFF000ULL;
    uint64_t pdpt_idx  = (addr >> 30) & 0x1FF;
    volatile uint64_t *pdpt = (volatile uint64_t *)phys_to_virt(pdpt_phys);
    uint64_t pdpte = pdpt[pdpt_idx];
    if (!(pdpte & PT_PRESENT)) return false;
    if (pdpte & PT_PS) return true;  /* 1 GiB page */

    /* PD index = bits [29:21] */
    uint64_t pd_phys = pdpte & 0x000FFFFFFFFFF000ULL;
    uint64_t pd_idx  = (addr >> 21) & 0x1FF;
    volatile uint64_t *pd = (volatile uint64_t *)phys_to_virt(pd_phys);
    uint64_t pde = pd[pd_idx];
    if (!(pde & PT_PRESENT)) return false;
    if (pde & PT_PS) return true;    /* 2 MiB page */

    /* PT index = bits [20:12] */
    uint64_t pt_phys = pde & 0x000FFFFFFFFFF000ULL;
    uint64_t pt_idx  = (addr >> 12) & 0x1FF;
    volatile uint64_t *pt = (volatile uint64_t *)phys_to_virt(pt_phys);
    uint64_t pte = pt[pt_idx];
    return (pte & PT_PRESENT) != 0;
}

/* ── Memory dump ──────────────────────────────────────────────────────────── */

/*
 * dbg_mem_dump — Print a hex + ASCII dump of memory at 'addr'.
 * 16 bytes per line. Checks mapping before each page boundary.
 *
 * Format:
 *   ffffffff80100000: 48 83 ec 08 48 8b 05 a1  2f 00 00 48 85 c0 74 05  H...H.../..H..t.
 */
void dbg_mem_dump(uint64_t addr, uint64_t len) {
    if (len == 0) len = 64;  /* Default to 64 bytes */

    for (uint64_t off = 0; off < len; off += 16) {
        uint64_t line_addr = addr + off;

        /* Check that this page is mapped */
        if (!dbg_is_addr_mapped(line_addr)) {
            dbg_serial_printf("%llx: <unmapped>\r\n", line_addr);
            continue;
        }

        /* Print address */
        dbg_serial_printf("%llx: ", line_addr);

        /* Print hex bytes (two groups of 8) */
        uint8_t bytes[16];
        uint64_t count = (off + 16 <= len) ? 16 : (len - off);

        for (uint64_t i = 0; i < 16; i++) {
            if (i < count && dbg_is_addr_mapped(line_addr + i)) {
                bytes[i] = *(volatile uint8_t *)(uintptr_t)(line_addr + i);
                dbg_serial_printf("%x", (unsigned)(bytes[i] >> 4));
                dbg_serial_printf("%x", (unsigned)(bytes[i] & 0xF));
            } else {
                bytes[i] = 0;
                dbg_serial_puts("  ");
            }
            dbg_serial_putc(' ');
            if (i == 7) dbg_serial_putc(' ');  /* Gap between groups */
        }

        /* Print ASCII representation */
        dbg_serial_putc(' ');
        for (uint64_t i = 0; i < 16; i++) {
            if (i < count) {
                char c = (char)bytes[i];
                dbg_serial_putc((c >= 32 && c <= 126) ? c : '.');
            }
        }
        dbg_serial_puts("\r\n");
    }
}

/* ── Memory write ─────────────────────────────────────────────────────────── */

/*
 * dbg_mem_write — Write bytes to a virtual address.
 * Verifies all pages are mapped before touching memory.
 * Returns true on success, false if any page is unmapped.
 */
bool dbg_mem_write(uint64_t addr, const uint8_t *data, uint64_t len) {
    /* Pre-flight: check every target page */
    for (uint64_t i = 0; i < len; i++) {
        if (!dbg_is_addr_mapped(addr + i)) {
            dbg_serial_printf("memw: address 0x%llx is unmapped\r\n", addr + i);
            return false;
        }
    }

    /* Write */
    volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)addr;
    for (uint64_t i = 0; i < len; i++) {
        dst[i] = data[i];
    }

    dbg_serial_printf("memw: wrote %llu bytes at 0x%llx\r\n", len, addr);
    return true;
}

/* ── Backtrace ────────────────────────────────────────────────────────────── */

/*
 * dbg_backtrace — Walk the frame pointer chain and print return addresses.
 * Requires -fno-omit-frame-pointer (set in build system).
 */
void dbg_backtrace(void) {
    uint64_t rbp;
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));

    dbg_serial_puts("backtrace:\r\n");

    for (int frame = 0; frame < 32; frame++) {
        if (!dbg_is_addr_mapped(rbp) || !dbg_is_addr_mapped(rbp + 8)) {
            break;
        }
        uint64_t ret_addr  = *(volatile uint64_t *)(rbp + 8);
        uint64_t next_rbp  = *(volatile uint64_t *)(rbp);

        /* Try symbol resolution if available */
        uint64_t sym_off = 0;
        const char *name = dbg_resolve_symbol(ret_addr, &sym_off);
        if (name) {
            dbg_serial_printf("  #%d  0x%llx  %s+0x%llx\r\n",
                              frame, ret_addr, name, sym_off);
        } else {
            dbg_serial_printf("  #%d  0x%llx\r\n", frame, ret_addr);
        }

        if (next_rbp == 0 || next_rbp <= rbp) break;  /* Stack exhausted */
        rbp = next_rbp;
    }
}

/* ── Command handlers ─────────────────────────────────────────────────────── */

static uint64_t cmd_parse_hex(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint64_t val = 0;
    while (*s) {
        char c = *s++;
        if      (c >= '0' && c <= '9') val = val * 16 + (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = val * 16 + (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = val * 16 + (uint64_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

static uint64_t cmd_parse_uint(const char *s) {
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (uint64_t)(*s++ - '0');
    return val;
}

/*
 * mem <addr> [len] — Hex+ASCII memory dump.
 */
static int dbg_cmd_mem(int argc, char **argv) {
    if (argc < 2) {
        dbg_serial_puts("Usage: mem <addr> [len]\r\n");
        return -1;
    }
    uint64_t addr = cmd_parse_hex(argv[1]);
    uint64_t len  = (argc >= 3) ? cmd_parse_uint(argv[2]) : 64;
    dbg_mem_dump(addr, len);
    return 0;
}
DBG_REGISTER_COMMAND(mem, "mem <addr> [len]", "Hex+ASCII memory dump", dbg_cmd_mem);

/*
 * memw <addr> <byte> [byte ...] — Write bytes to memory.
 */
static int dbg_cmd_memw(int argc, char **argv) {
    if (argc < 3) {
        dbg_serial_puts("Usage: memw <addr> <byte> [byte ...]\r\n");
        return -1;
    }
    uint64_t addr = cmd_parse_hex(argv[1]);
    uint8_t data[64];
    int count = 0;
    for (int i = 2; i < argc && count < 64; i++) {
        data[count++] = (uint8_t)cmd_parse_hex(argv[i]);
    }
    dbg_mem_write(addr, data, (uint64_t)count);
    return 0;
}
DBG_REGISTER_COMMAND(memw, "memw <addr> <b0> ...", "Write bytes to memory", dbg_cmd_memw);

/*
 * bt — Stack backtrace.
 */
static int dbg_cmd_bt(int argc, char **argv) {
    (void)argc; (void)argv;
    dbg_backtrace();
    return 0;
}
DBG_REGISTER_COMMAND(bt, "bt", "Stack backtrace", dbg_cmd_bt);

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
