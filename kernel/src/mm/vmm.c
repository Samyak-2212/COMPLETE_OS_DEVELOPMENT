/* ============================================================================
 * vmm.c — NexusOS Virtual Memory Manager
 * Purpose: 4-level x86_64 paging: map/unmap pages, walk page tables
 * Author: NexusOS Kernel Team
 *
 * NOTE: During Phase 2, we continue using the Limine-provided page tables
 * (which already map the kernel in higher-half and provide HHDM).
 * We augment them by providing map/unmap functionality for future use.
 * A full page table rebuild from scratch is deferred to when we actually
 * need to create new address spaces (Phase 4 — userspace).
 * ============================================================================ */

#include "mm/vmm.h"
#include "mm/pmm.h"
#include "hal/io.h"
#include "boot/limine_requests.h"
#include "lib/string.h"
#include "lib/printf.h"

/* ── State ──────────────────────────────────────────────────────────────── */

static uint64_t hhdm_offset = 0;
static uint64_t *pml4 = (void *)0;     /* Current PML4 (virtual address) */

/* ── Helpers ────────────────────────────────────────────────────────────── */

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static inline uint64_t virt_to_phys_hhdm(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}

/* Extract page table indices from a virtual address */
#define PML4_INDEX(addr)  (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x1FF)

/* Get or create a page table entry at a given level.
 * If create=1 and the entry is not present, allocate a new table. */
static uint64_t *vmm_get_next_level(uint64_t *table, uint64_t index, int create) {
    if (table[index] & PAGE_PRESENT) {
        /* Entry exists — follow it */
        uint64_t phys = table[index] & 0x000FFFFFFFFFF000ULL;
        return (uint64_t *)phys_to_virt(phys);
    }

    if (!create) {
        return (void *)0;
    }

    /* Allocate a new page table */
    uint64_t new_page = pmm_alloc_page();
    if (new_page == 0) {
        return (void *)0; /* Out of memory */
    }

    /* Zero it out (pmm_alloc_page already zeros, but be safe) */
    memset(phys_to_virt(new_page), 0, 4096);

    /* Install the entry with present + writable + user (permissive at higher levels) */
    table[index] = new_page | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    return (uint64_t *)phys_to_virt(new_page);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void vmm_init(void) {
    struct limine_hhdm_response *hhdm = limine_get_hhdm();
    if (!hhdm) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] VMM: No HHDM response\n");
        return;
    }

    hhdm_offset = hhdm->offset;

    /* Read current CR3 — Limine already set up valid page tables */
    uint64_t cr3_val = read_cr3();
    pml4 = (uint64_t *)phys_to_virt(cr3_val & 0x000FFFFFFFFFF000ULL);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("VMM: Using Limine page tables, PML4 at phys 0x%016llx\n",
            (unsigned long long)(cr3_val & 0x000FFFFFFFFFF000ULL));
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pdpt = vmm_get_next_level(pml4, PML4_INDEX(virt), 1);
    if (!pdpt) return;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 1);
    if (!pd) return;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 1);
    if (!pt) return;

    pt[PT_INDEX(virt)] = (phys & 0x000FFFFFFFFFF000ULL) | flags;
    invlpg(virt);
}

void vmm_unmap_page(uint64_t virt) {
    uint64_t *pdpt = vmm_get_next_level(pml4, PML4_INDEX(virt), 0);
    if (!pdpt) return;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 0);
    if (!pd) return;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 0);
    if (!pt) return;

    pt[PT_INDEX(virt)] = 0;
    invlpg(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t *pdpt = vmm_get_next_level(pml4, PML4_INDEX(virt), 0);
    if (!pdpt) return 0;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 0);
    if (!pd) return 0;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 0);
    if (!pt) return 0;

    if (!(pt[PT_INDEX(virt)] & PAGE_PRESENT)) return 0;

    return (pt[PT_INDEX(virt)] & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

uint64_t vmm_get_hhdm_offset(void) {
    return hhdm_offset;
}
