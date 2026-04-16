/* ============================================================================
 * dbg_symbols.c — NexusOS Debugger ELF Symbol Resolver
 * Purpose: Parse the kernel ELF binary (provided by Limine) to resolve
 *          virtual addresses to "function_name+0xOFFSET" strings.
 *          Registered as 'sym' command.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

#include <lib/string.h>

/* ── Forward declarations ─────────────────────────────────────────────────── */

void dbg_serial_printf(const char *fmt, ...);
void dbg_serial_puts(const char *s);

/* Full struct definitions from Limine */
#include <limine.h>

/* Kernel API — get the in-memory ELF binary from Limine */
extern struct limine_executable_file_response *limine_get_exec_file(void);

/* ── Inline ELF64 structures (no external headers needed) ─────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;       /* Section header table offset */
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;   /* Section header entry size */
    uint16_t e_shnum;       /* Number of section headers */
    uint16_t e_shstrndx;    /* Section name string table index */
} elf64_ehdr_t;

typedef struct __attribute__((packed)) {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;     /* Offset from beginning of file */
    uint64_t sh_size;
    uint32_t sh_link;       /* For SYMTAB: index of associated STRTAB */
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} elf64_shdr_t;

typedef struct __attribute__((packed)) {
    uint32_t st_name;       /* Offset into string table */
    uint8_t  st_info;       /* Type (low 4) and binding (high 4) */
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;      /* Virtual address of symbol */
    uint64_t st_size;       /* Size of symbol */
} elf64_sym_t;

/* ELF constants */
#define SHT_SYMTAB  2
#define SHT_STRTAB  3
#define STT_FUNC    2
#define ELF64_ST_TYPE(i)  ((i) & 0xF)

/* ── Internal state ───────────────────────────────────────────────────────── */

static elf64_sym_t *sym_table    = NULL;
static const char  *sym_strtab   = NULL;
static uint64_t     sym_count    = 0;
static bool         sym_ready    = false;

/* ── Initialization ───────────────────────────────────────────────────────── */

/*
 * dbg_symbols_init — Parse the in-memory kernel ELF to locate .symtab
 * and its associated .strtab. Must be called after Limine has provided
 * the executable file response.
 *
 * This function does NOT allocate memory. It sets pointers directly into
 * the Limine-provided ELF image (which persists for the kernel's lifetime).
 */
void dbg_symbols_init(void) {
    /* Get the ELF from Limine */
    struct limine_executable_file_response *resp = limine_get_exec_file();
    if (!resp || !resp->executable_file) {
        dbg_serial_puts("[WARN][SYM] No executable file from Limine\r\n");
        return;
    }

    /* Access the limine_file fields manually to avoid pulling in limine.h */
    /* struct limine_file { uint64_t revision; void *address; uint64_t size; ... } */
    uint64_t *file_fields = (uint64_t *)resp->executable_file;
    uint8_t *elf_base = (uint8_t *)(uintptr_t)file_fields[1];  /* address */
    /* uint64_t elf_size = file_fields[2]; */                   /* size */

    if (!elf_base) {
        dbg_serial_puts("[WARN][SYM] ELF base is NULL\r\n");
        return;
    }

    /* Validate ELF magic */
    if (elf_base[0] != 0x7F || elf_base[1] != 'E' ||
        elf_base[2] != 'L'  || elf_base[3] != 'F') {
        dbg_serial_puts("[WARN][SYM] Invalid ELF magic\r\n");
        return;
    }

    elf64_ehdr_t *ehdr = (elf64_ehdr_t *)elf_base;

    /* Locate .symtab section */
    elf64_shdr_t *shdr = (elf64_shdr_t *)(elf_base + ehdr->e_shoff);

    for (uint16_t i = 0; i < ehdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            sym_table = (elf64_sym_t *)(elf_base + shdr[i].sh_offset);
            sym_count = shdr[i].sh_size / shdr[i].sh_entsize;

            /* The associated string table is at sh_link */
            uint32_t strtab_idx = shdr[i].sh_link;
            if (strtab_idx < ehdr->e_shnum &&
                shdr[strtab_idx].sh_type == SHT_STRTAB) {
                sym_strtab = (const char *)(elf_base + shdr[strtab_idx].sh_offset);
            }
            break;
        }
    }

    if (sym_table && sym_strtab && sym_count > 0) {
        sym_ready = true;
        dbg_serial_printf("[INFO][SYM] Loaded %llu symbols\r\n", sym_count);
    } else {
        dbg_serial_puts("[WARN][SYM] No .symtab found in kernel ELF\r\n");
    }
}

/* ── Symbol resolution ────────────────────────────────────────────────────── */

/*
 * dbg_resolve_symbol — Find the STT_FUNC symbol whose range contains addr.
 * Returns the symbol name (pointer into strtab) and the offset from the
 * symbol's base in *offset_out. Returns NULL if no match found.
 *
 * Algorithm: linear scan — simple, no allocation, O(n) per lookup.
 * For a kernel with < 1000 symbols this is perfectly adequate.
 */
const char *dbg_resolve_symbol(uint64_t addr, uint64_t *offset_out) {
    if (!sym_ready) return NULL;

    const char *best_name = NULL;
    uint64_t    best_base = 0;
    uint64_t    best_dist = UINT64_MAX;

    for (uint64_t i = 0; i < sym_count; i++) {
        /* Only consider function symbols */
        if (ELF64_ST_TYPE(sym_table[i].st_info) != STT_FUNC) continue;
        if (sym_table[i].st_value == 0) continue;

        uint64_t sym_addr = sym_table[i].st_value;

        /* Must be at or before the target address */
        if (sym_addr > addr) continue;

        uint64_t dist = addr - sym_addr;

        /* If the symbol has a known size, check containment */
        if (sym_table[i].st_size > 0 && dist >= sym_table[i].st_size) continue;

        /* Closest match wins */
        if (dist < best_dist) {
            best_dist = dist;
            best_base = sym_addr;
            best_name = sym_strtab + sym_table[i].st_name;
        }
    }

    if (best_name && offset_out) {
        *offset_out = addr - best_base;
    }
    return best_name;
}

/* ── Command handler ──────────────────────────────────────────────────────── */

static uint64_t sym_parse_hex(const char *s) {
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

/*
 * sym <addr> — Resolve a virtual address to a symbol name.
 */
static int dbg_cmd_sym(int argc, char **argv) {
    if (argc < 2) {
        dbg_serial_puts("Usage: sym <addr>\r\n");
        return -1;
    }

    uint64_t addr = sym_parse_hex(argv[1]);
    uint64_t offset = 0;
    const char *name = dbg_resolve_symbol(addr, &offset);

    if (name) {
        dbg_serial_printf("0x%llx = %s+0x%llx\r\n", addr, name, offset);
    } else {
        dbg_serial_printf("0x%llx = <unknown>\r\n", addr);
    }
    return 0;
}
DBG_REGISTER_COMMAND(sym, "sym <addr>", "Resolve address to symbol", dbg_cmd_sym);

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
