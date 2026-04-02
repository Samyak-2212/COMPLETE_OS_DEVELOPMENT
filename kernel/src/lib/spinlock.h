/* ============================================================================
 * spinlock.h — NexusOS Spinlock Primitives
 * Purpose: Simple spinlock with interrupt disable for single-CPU kernel
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_LIB_SPINLOCK_H
#define NEXUS_LIB_SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t lock;     /* 0 = unlocked, 1 = locked */
    volatile uint64_t flags;    /* Saved RFLAGS (interrupt state) */
} spinlock_t;

#define SPINLOCK_INIT   { .lock = 0, .flags = 0 }

/* Acquire the spinlock (disables interrupts, spins until acquired) */
void spinlock_acquire(spinlock_t *lock);

/* Release the spinlock (restores previous interrupt state) */
void spinlock_release(spinlock_t *lock);

#endif /* NEXUS_LIB_SPINLOCK_H */
