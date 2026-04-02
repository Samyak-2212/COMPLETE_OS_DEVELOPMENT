/* ============================================================================
 * heap.c — NexusOS Kernel Heap
 * Purpose: Simple linked-list free-list allocator with splitting/coalescing
 * Author: NexusOS Kernel Team
 *
 * Design: Free blocks are stored in a sorted linked list. Allocation uses
 * first-fit with splitting. Free coalesces adjacent blocks. New pages are
 * mapped on demand via VMM when the heap needs to grow.
 * ============================================================================ */

#include "mm/heap.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "lib/string.h"
#include "lib/spinlock.h"
#include "lib/printf.h"

/* ── Block header (placed before each allocation) ───────────────────────── */

typedef struct block_header {
    uint64_t size;                  /* Size of the data area (excludes header) */
    int      free;                  /* Is this block free? */
    struct block_header *next;      /* Next block in the list */
} block_header_t;

#define HEADER_SIZE  sizeof(block_header_t)
#define MIN_BLOCK    32     /* Minimum data size to avoid tiny fragments */

/* ── State ──────────────────────────────────────────────────────────────── */

static block_header_t *heap_start = (void *)0;
static uint64_t heap_virt_base = 0;
static uint64_t heap_virt_end = 0;
static uint64_t heap_total_size = 0;
static spinlock_t heap_lock = SPINLOCK_INIT;

/* Fixed virtual address range for the kernel heap (in higher-half space) */
#define HEAP_VIRT_START  0xFFFFFFFF90000000ULL

/* ── Internal: grow the heap by mapping new pages ───────────────────────── */

static int heap_grow(uint64_t pages) {
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) return -1;

        vmm_map_page(heap_virt_end, phys,
                     PAGE_PRESENT | PAGE_WRITABLE | PAGE_NX);
        heap_virt_end += 4096;
    }
    return 0;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void heap_init(uint64_t initial_pages) {
    heap_virt_base = HEAP_VIRT_START;
    heap_virt_end = HEAP_VIRT_START;

    /* Map initial pages */
    if (heap_grow(initial_pages) != 0) {
        kprintf_set_color(0x00FF4444, 0x001A1A2E);
        kprintf("[FAIL] Heap: Could not allocate initial pages\n");
        return;
    }

    heap_total_size = initial_pages * 4096;

    /* Create one large free block spanning the whole heap */
    heap_start = (block_header_t *)heap_virt_base;
    heap_start->size = heap_total_size - HEADER_SIZE;
    heap_start->free = 1;
    heap_start->next = (void *)0;

    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("Heap: %llu KiB at virt 0x%016llx\n",
            (unsigned long long)(heap_total_size / 1024),
            (unsigned long long)heap_virt_base);
}

void *kmalloc(size_t size) {
    if (size == 0) return (void *)0;

    /* Align size to 16 bytes */
    size = (size + 15) & ~(size_t)15;

    spinlock_acquire(&heap_lock);

    block_header_t *curr = heap_start;

    /* First-fit search */
    while (curr) {
        if (curr->free && curr->size >= size) {
            /* Found a block — split if possible */
            if (curr->size >= size + HEADER_SIZE + MIN_BLOCK) {
                /* Split: create a new free block after the allocated portion */
                block_header_t *new_block = (block_header_t *)
                    ((uint8_t *)curr + HEADER_SIZE + size);
                new_block->size = curr->size - size - HEADER_SIZE;
                new_block->free = 1;
                new_block->next = curr->next;
                curr->next = new_block;
                curr->size = size;
            }

            curr->free = 0;
            spinlock_release(&heap_lock);
            return (void *)((uint8_t *)curr + HEADER_SIZE);
        }
        curr = curr->next;
    }

    /* No suitable block found — try to grow the heap */
    uint64_t needed = size + HEADER_SIZE;
    uint64_t pages = (needed + 4095) / 4096;
    if (pages < 4) pages = 4; /* Grow by at least 16 KiB */

    uint64_t old_end = heap_virt_end;
    if (heap_grow(pages) != 0) {
        spinlock_release(&heap_lock);
        return (void *)0;
    }

    /* Create a new free block in the grown region */
    block_header_t *new_block = (block_header_t *)old_end;
    new_block->size = pages * 4096 - HEADER_SIZE;
    new_block->free = 1;
    new_block->next = (void *)0;
    heap_total_size += pages * 4096;

    /* Append to the list */
    curr = heap_start;
    while (curr->next) {
        curr = curr->next;
    }
    curr->next = new_block;

    spinlock_release(&heap_lock);

    /* Retry allocation */
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;

    spinlock_acquire(&heap_lock);

    block_header_t *block = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    block->free = 1;

    /* Coalesce adjacent free blocks */
    block_header_t *curr = heap_start;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            /* Merge curr with curr->next */
            curr->size += HEADER_SIZE + curr->next->size;
            curr->next = curr->next->next;
            /* Don't advance — check if we can merge again */
            continue;
        }
        curr = curr->next;
    }

    spinlock_release(&heap_lock);
}

void *kmalloc_aligned(size_t size, size_t align) {
    if (align <= 16) return kmalloc(size);

    /* Over-allocate to guarantee alignment */
    void *raw = kmalloc(size + align + sizeof(void *));
    if (!raw) return (void *)0;

    /* Align the pointer */
    uintptr_t addr = (uintptr_t)raw + sizeof(void *);
    addr = (addr + align - 1) & ~(align - 1);

    /* Store the original pointer before the aligned address */
    ((void **)addr)[-1] = raw;

    return (void *)addr;
}

void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = kmalloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void heap_get_stats(uint64_t *total, uint64_t *used, uint64_t *free_bytes) {
    uint64_t t = 0, u = 0, f = 0;

    spinlock_acquire(&heap_lock);

    block_header_t *curr = heap_start;
    while (curr) {
        t += curr->size + HEADER_SIZE;
        if (curr->free) {
            f += curr->size;
        } else {
            u += curr->size;
        }
        curr = curr->next;
    }

    spinlock_release(&heap_lock);

    if (total) *total = t;
    if (used) *used = u;
    if (free_bytes) *free_bytes = f;
}
