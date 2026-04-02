/* ============================================================================
 * heap.h — NexusOS Kernel Heap
 * Purpose: Dynamic memory allocation for kernel (kmalloc / kfree)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_MM_HEAP_H
#define NEXUS_MM_HEAP_H

#include <stdint.h>
#include <stddef.h>

/* Initialize the kernel heap with initial_pages pages */
void heap_init(uint64_t initial_pages);

/* Allocate size bytes from the kernel heap */
void *kmalloc(size_t size);

/* Free a previously allocated block */
void kfree(void *ptr);

/* Allocate size bytes with the given alignment */
void *kmalloc_aligned(size_t size, size_t align);

/* Allocate and zero-initialize */
void *kcalloc(size_t count, size_t size);

/* Get heap usage statistics */
void heap_get_stats(uint64_t *total, uint64_t *used, uint64_t *free_bytes);

#endif /* NEXUS_MM_HEAP_H */
