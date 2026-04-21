/* ============================================================================
 * pmm.c — NexusOS Physical Memory Manager
 * Purpose: Bitmap allocator for 4 KiB physical page frames
 * Author: NexusOS Kernel Team
 *
 * Strategy: Parse the Limine memory map, allocate a bitmap in the first
 * usable region large enough, then mark all non-usable pages as used.
 * The bitmap itself is stored at a physical address with HHDM access.
 * ============================================================================ */

#include "mm/pmm.h"
#include "boot/limine_requests.h"
#include "lib/bitmap.h"
#include "lib/string.h"
#include "lib/spinlock.h"
#include "lib/printf.h"
#include <limine.h>

/* ── State ──────────────────────────────────────────────────────────────── */

static uint8_t *bitmap = (void *)0;    /* Bitmap (HHDM virtual address) */
static uint64_t max_page_index = 0;     /* Highest physical page index for bitmap bounds */
static uint64_t usable_pages = 0;       /* Total usable RAM pages */
static uint64_t used_pages = 0;         /* Currently allocated RAM pages */
static uint64_t highest_address = 0;    /* Highest physical address */
static uint64_t hhdm_offset = 0;        /* HHDM offset from Limine */
static spinlock_t pmm_lock = SPINLOCK_INIT;

#define MAX_PAGES 262144                /* Max pages for 1 GB RAM (actually tracks refcounts up to 1GB phys) */
static uint16_t page_refcount[MAX_PAGES]; /* Refcount per physical page */

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Convert physical address to HHDM virtual address */
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void pmm_init(void) {
    struct limine_memmap_response *memmap = limine_get_memmap();
    struct limine_hhdm_response *hhdm = limine_get_hhdm();

    if (!memmap || !hhdm) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] PMM: No memory map or HHDM response\n");
        return;
    }

    hhdm_offset = hhdm->offset;

    /* Pass 1: Find the highest physical address to size the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        uint64_t top = entry->base + entry->length;
        if (top > highest_address) {
            highest_address = top;
        }
    }

    max_page_index = highest_address / PMM_PAGE_SIZE;
    uint64_t bitmap_size = (max_page_index + 7) / 8; /* Bytes for bitmap */

    /* Pass 2: Find a usable region large enough for the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap = (uint8_t *)phys_to_virt(entry->base);

            /* Mark all pages as used initially (so holes are non-allocatable) */
            memset(bitmap, 0xFF, bitmap_size);

            /* Adjust this entry: bitmap occupies the start */
            uint64_t bitmap_pages = (bitmap_size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
            entry->base += bitmap_pages * PMM_PAGE_SIZE;
            entry->length -= bitmap_pages * PMM_PAGE_SIZE;
            break;
        }
    }

    if (!bitmap) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] PMM: No region large enough for bitmap\n");
        return;
    }

    /* Pass 3: Mark usable pages as free in bitmap, and count them */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = entry->base / PMM_PAGE_SIZE;
            uint64_t page_count = entry->length / PMM_PAGE_SIZE;
            usable_pages += page_count;
            for (uint64_t p = 0; p < page_count; p++) {
                bitmap_clear(bitmap, start_page + p);
            }
        }
    }

    /* Never allocate page 0 (null page guard). 
       Since 0 might have been in a usable block, we must mark it used. 
       If it wasn't usable, it's already used. */
    if (!bitmap_test(bitmap, 0)) {
        bitmap_set(bitmap, 0);
        used_pages = 1;
    } else {
        used_pages = 0;
    }

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("PMM: %llu MiB usable, %llu MiB free (%llu/%llu pages)\n",
            (unsigned long long)(usable_pages * PMM_PAGE_SIZE / (1024 * 1024)),
            (unsigned long long)((usable_pages - used_pages) * PMM_PAGE_SIZE / (1024 * 1024)),
            (unsigned long long)(usable_pages - used_pages),
            (unsigned long long)usable_pages);
    
    memset(page_refcount, 0, sizeof(page_refcount));
}

uint64_t pmm_alloc_page(void) {
    spinlock_acquire(&pmm_lock);

    for (uint64_t i = 1; i < max_page_index; i++) {
        if (!bitmap_test(bitmap, i)) {
            bitmap_set(bitmap, i);
            used_pages++;
            if (i < MAX_PAGES) page_refcount[i] = 1;
            spinlock_release(&pmm_lock);
            uint64_t addr = i * PMM_PAGE_SIZE;
            /* Zero the page via HHDM */
            memset(phys_to_virt(addr), 0, PMM_PAGE_SIZE);
            return addr;
        }
    }

    spinlock_release(&pmm_lock);
    return 0; /* Out of memory */
}

void pmm_free_page(uint64_t phys_addr) {
    uint64_t page = phys_addr / PMM_PAGE_SIZE;
    if (page == 0 || page >= max_page_index) return;

    spinlock_acquire(&pmm_lock);
    if (bitmap_test(bitmap, page)) {
        bitmap_clear(bitmap, page);
        used_pages--;
        if (page < MAX_PAGES) page_refcount[page] = 0;
    }
    spinlock_release(&pmm_lock);
}

uint64_t pmm_alloc_pages(uint64_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    spinlock_acquire(&pmm_lock);

    for (uint64_t i = 1; i < max_page_index; i++) {
        /* Check if count contiguous pages starting at i are free */
        int found = 1;
        for (uint64_t j = 0; j < count && (i + j) < max_page_index; j++) {
            if (bitmap_test(bitmap, i + j)) {
                i += j; /* Skip ahead */
                found = 0;
                break;
            }
        }
        if (found && (i + count) <= max_page_index) {
            for (uint64_t j = 0; j < count; j++) {
                bitmap_set(bitmap, i + j);
                used_pages++;
                if ((i + j) < MAX_PAGES) page_refcount[i + j] = 1;
            }
            spinlock_release(&pmm_lock);
            uint64_t addr = i * PMM_PAGE_SIZE;
            memset(phys_to_virt(addr), 0, count * PMM_PAGE_SIZE);
            return addr;
        }
    }

    spinlock_release(&pmm_lock);
    return 0;
}

void pmm_free_pages(uint64_t phys_addr, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        pmm_free_page(phys_addr + i * PMM_PAGE_SIZE);
    }
}

uint64_t pmm_get_total_memory(void) {
    return usable_pages * PMM_PAGE_SIZE;
}

uint64_t pmm_get_free_memory(void) {
    return (usable_pages - used_pages) * PMM_PAGE_SIZE;
}

uint64_t pmm_get_total_page_count(void) {
    return usable_pages;
}

uint64_t pmm_get_free_page_count(void) {
    return usable_pages - used_pages;
}

void pmm_page_ref(uint64_t phys) {
    uint64_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_PAGES) return;
    spinlock_acquire(&pmm_lock);
    page_refcount[page]++;
    spinlock_release(&pmm_lock);
}

void pmm_page_unref(uint64_t phys) {
    uint64_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_PAGES || page == 0) return;
    spinlock_acquire(&pmm_lock);
    if (page_refcount[page] > 0) {
        page_refcount[page]--;
        if (page_refcount[page] == 0) {
            if (bitmap_test(bitmap, page)) {
                bitmap_clear(bitmap, page);
                used_pages--;
            }
        }
    }
    spinlock_release(&pmm_lock);
}

uint16_t pmm_page_refcount(uint64_t phys) {
    uint64_t page = phys / PMM_PAGE_SIZE;
    if (page >= MAX_PAGES) return 0;
    spinlock_acquire(&pmm_lock);
    uint16_t count = page_refcount[page];
    spinlock_release(&pmm_lock);
    return count;
}

