/* ============================================================================
 * vmm.h — NexusOS Virtual Memory Manager
 * Purpose: 4-level x86_64 paging (PML4 → PDPT → PD → PT)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_MM_VMM_H
#define NEXUS_MM_VMM_H

#include <stdint.h>

/* ── Page flags ──────────────────────────────────────────────────────────── */

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITABLE   (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_WRITETHROUGH (1ULL << 3)
#define PAGE_NOCACHE    (1ULL << 4)
#define PAGE_ACCESSED   (1ULL << 5)
#define PAGE_DIRTY      (1ULL << 6)
#define PAGE_HUGE       (1ULL << 7)     /* 2 MiB page in PD level */
#define PAGE_GLOBAL     (1ULL << 8)
#define PAGE_NX         (1ULL << 63)    /* No-Execute */

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Initialize the VMM: create new page tables, map kernel + HHDM */
void vmm_init(void);

/* Map a single 4 KiB page: virtual → physical with given flags */
void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

/* Unmap a single 4 KiB page */
void vmm_unmap_page(uint64_t virt);

/* Get the physical address mapped at virt, or 0 if not mapped */
uint64_t vmm_get_phys(uint64_t virt);

/* Get the HHDM offset (for physical → virtual conversion) */
uint64_t vmm_get_hhdm_offset(void);

#endif /* NEXUS_MM_VMM_H */
