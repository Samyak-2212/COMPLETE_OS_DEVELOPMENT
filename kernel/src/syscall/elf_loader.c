/* ELF64 loader — loads a static ELF binary into a process address space using lazy physical allocation */

#include "elf_loader.h"
#include "fs/vfs.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "hal/io.h"
#include "lib/string.h"

#define ELF_MAGIC   0x464C457F

typedef struct {
    uint32_t e_ident_magic;
    uint8_t  e_ident_class;       /* 2 = ELF64 */
    uint8_t  e_ident_data;        /* 1 = little-endian */
    uint8_t  e_ident_version;
    uint8_t  e_ident_osabi;
    uint8_t  e_ident_pad[8];
    uint16_t e_type;              /* 2 = ET_EXEC */
    uint16_t e_machine;           /* 0x3E = x86_64 */
    uint32_t e_version;
    uint64_t e_entry;             /* Entry point virtual address */
    uint64_t e_phoff;             /* Program header table offset */
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;             /* Number of program headers */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_hdr_t;

typedef struct {
    uint32_t p_type;    /* 1 = PT_LOAD */
    uint32_t p_flags;   /* PF_R=4, PF_W=2, PF_X=1 */
    uint64_t p_offset;  /* Offset in file */
    uint64_t p_vaddr;   /* Virtual address to load at */
    uint64_t p_paddr;
    uint64_t p_filesz;  /* Size in file */
    uint64_t p_memsz;   /* Size in memory (may be > filesz, zero-pad) */
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD     1
#define PF_X        1
#define PF_W        2
#define PF_R        4

int elf_load(process_t *proc, vfs_node_t *file, uint64_t *entry_out) {
    elf64_hdr_t hdr;
    if (vfs_read(file, 0, sizeof(hdr), (uint8_t*)&hdr) < (int64_t)sizeof(hdr)) return -1;
    if (hdr.e_ident_magic != ELF_MAGIC) return -1;
    if (hdr.e_ident_class != 2) return -1;   /* not 64-bit */
    if (hdr.e_machine != 0x3E) return -1;    /* not x86_64 */

    *entry_out = hdr.e_entry;

    uint64_t saved_cr3 = read_cr3();
    if (saved_cr3 != proc->cr3) write_cr3(proc->cr3);

    uint64_t hhdm_offset = vmm_get_hhdm_offset();
    proc->heap_base = 0; /* Will be set after segments if needed */

    for (uint16_t i = 0; i < hdr.e_phnum; i++) {
        elf64_phdr_t phdr;
        uint64_t phdr_offset = hdr.e_phoff + i * hdr.e_phentsize;
        vfs_read(file, phdr_offset, sizeof(phdr), (uint8_t*)&phdr);

        if (phdr.p_type != PT_LOAD) continue;
        if (phdr.p_memsz == 0) continue;

        uint64_t virt_start = phdr.p_vaddr & ~0xFFFULL;
        uint64_t virt_end   = (phdr.p_vaddr + phdr.p_memsz + 0xFFF) & ~0xFFFULL;
        uint64_t num_pages  = (virt_end - virt_start) / 4096;

        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if (phdr.p_flags & PF_W) flags |= PAGE_WRITABLE;

        /* Register VMA for this segment so #PF handler knows it's valid */
        process_add_vma(proc, phdr.p_vaddr, phdr.p_vaddr + phdr.p_memsz, flags);

        /* Update process heap base to be after the last segment */
        uint64_t seg_end = (phdr.p_vaddr + phdr.p_memsz + 0xFFF) & ~0xFFFULL;
        if (seg_end > proc->heap_base) proc->heap_base = seg_end;
        if (seg_end > proc->heap_end) proc->heap_end = seg_end;

        /* LAZY ALLOCATION (as per Memory Efficiency Mandate P1)
         * Only pre-allocate pages that contain actual file data.
         * For BSS (memsz > filesz), they will page fault and map anon pages.
         */
        uint64_t file_pages = (phdr.p_filesz + (phdr.p_vaddr & 0xFFF) + 0xFFF) / 4096;
        if (file_pages > num_pages) file_pages = num_pages;

        for (uint64_t j = 0; j < file_pages; j++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) {
                if (saved_cr3 != proc->cr3) write_cr3(saved_cr3);
                return -1;
            }
            uint64_t virt = virt_start + j * 4096;
            vmm_map_page(virt, phys, flags);
            
            memset((void *)(phys + hhdm_offset), 0, 4096);

            /* Calculate what part of this page overlaps with file data */
            uint64_t page_offset_in_segment = (virt > phdr.p_vaddr) ? (virt - phdr.p_vaddr) : 0;
            uint64_t read_offset = phdr.p_offset + page_offset_in_segment;
            
            uint64_t dest_offset_in_page = (j == 0) ? (phdr.p_vaddr & 0xFFF) : 0;
            void *dest_ptr = (void *)(phys + hhdm_offset + dest_offset_in_page);
            
            uint64_t bytes_to_read = 4096 - dest_offset_in_page;
            if (page_offset_in_segment + bytes_to_read > phdr.p_filesz) {
                bytes_to_read = phdr.p_filesz > page_offset_in_segment ? (phdr.p_filesz - page_offset_in_segment) : 0;
            }
            
            if (bytes_to_read > 0) {
                vfs_read(file, read_offset, bytes_to_read, (uint8_t*)dest_ptr);
            }
        }
    }

    if (saved_cr3 != proc->cr3) write_cr3(saved_cr3);
    return 0;
}
