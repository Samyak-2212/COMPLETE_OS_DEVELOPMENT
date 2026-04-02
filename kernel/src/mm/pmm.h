/* ============================================================================
 * pmm.h — NexusOS Physical Memory Manager
 * Purpose: Bitmap-based physical page frame allocator
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_MM_PMM_H
#define NEXUS_MM_PMM_H

#include <stdint.h>
#include <stddef.h>

/* Page size and shift */
#define PMM_PAGE_SIZE   4096
#define PMM_PAGE_SHIFT  12

/* Initialize the PMM using the Limine memory map */
void pmm_init(void);

/* Allocate a single physical page (4 KiB). Returns physical address, or 0 on failure. */
uint64_t pmm_alloc_page(void);

/* Free a single physical page (4 KiB). */
void pmm_free_page(uint64_t phys_addr);

/* Allocate count contiguous physical pages. Returns physical address, or 0 on failure. */
uint64_t pmm_alloc_pages(uint64_t count);

/* Free count contiguous physical pages starting at phys_addr. */
void pmm_free_pages(uint64_t phys_addr, uint64_t count);

/* Get total physical memory in bytes */
uint64_t pmm_get_total_memory(void);

/* Get free physical memory in bytes */
uint64_t pmm_get_free_memory(void);

#endif /* NEXUS_MM_PMM_H */
