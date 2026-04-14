#include "debugger.h"
#include <boot/limine_requests.h>
#include <lib/string.h>

/* ELF64 structures */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

#define SHT_SYMTAB 2
#define STT_FUNC   2

static Elf64_Sym *sym_table = NULL;
static const char *str_table = NULL;
static size_t sym_count = 0;

void debugger_symbols_init(void) {
    struct limine_executable_file_response *resp = limine_get_exec_file();
    if (!resp || !resp->executable_file) return;

    void *elf_base = resp->executable_file->address;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)elf_base;

    /* Verify ELF Magic */
    if (memcmp(ehdr->e_ident, "\x7f\x45\x4c\x46", 4) != 0) return;

    Elf64_Shdr *shdrs = (Elf64_Shdr *)((uintptr_t)elf_base + ehdr->e_shoff);
    
    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            sym_table = (Elf64_Sym *)((uintptr_t)elf_base + shdrs[i].sh_offset);
            sym_count = shdrs[i].sh_size / sizeof(Elf64_Sym);
            
            /* Linked string table */
            uint32_t link = shdrs[i].sh_link;
            str_table = (const char *)((uintptr_t)elf_base + shdrs[link].sh_offset);
            break;
        }
    }
}

const char* debugger_resolve_symbol(uint64_t addr, uint64_t *offset) {
    if (!sym_table || !str_table) return NULL;

    Elf64_Sym *best_sym = NULL;
    uint64_t max_val = 0;

    /* Linear scan for now, could be binary search if sorted */
    for (size_t i = 0; i < sym_count; i++) {
        uint8_t type = sym_table[i].st_info & 0xF;
        if (type != STT_FUNC) continue;

        if (addr >= sym_table[i].st_value && sym_table[i].st_value > max_val) {
            max_val = sym_table[i].st_value;
            best_sym = &sym_table[i];
        }
    }

    if (best_sym) {
        *offset = addr - best_sym->st_value;
        return &str_table[best_sym->st_name];
    }

    return NULL;
}
