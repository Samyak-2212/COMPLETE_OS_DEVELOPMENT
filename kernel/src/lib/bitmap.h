/* ============================================================================
 * bitmap.h — NexusOS Bitmap Operations
 * Purpose: Bit manipulation operations for PMM bitmap allocator
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_LIB_BITMAP_H
#define NEXUS_LIB_BITMAP_H

#include <stdint.h>
#include <stddef.h>

/* Set bit n in bitmap */
static inline void bitmap_set(uint8_t *bitmap, uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

/* Clear bit n in bitmap */
static inline void bitmap_clear(uint8_t *bitmap, uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

/* Test bit n in bitmap (returns non-zero if set) */
static inline int bitmap_test(const uint8_t *bitmap, uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

#endif /* NEXUS_LIB_BITMAP_H */
