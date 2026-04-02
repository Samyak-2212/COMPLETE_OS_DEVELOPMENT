/* ============================================================================
 * spinlock.c — NexusOS Spinlock Primitives
 * Purpose: Spinlock with cli/sti for single-CPU interrupt safety
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "lib/spinlock.h"

/* Read RFLAGS register */
static inline uint64_t read_rflags(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(flags));
    return flags;
}

/* Acquire the spinlock — disables interrupts first */
void spinlock_acquire(spinlock_t *lock) {
    /* Save current interrupt state */
    uint64_t flags = read_rflags();

    /* Disable interrupts */
    __asm__ volatile ("cli");

    /* Spin until we acquire the lock (test-and-set) */
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        /* Spin — on single CPU this should never happen if used correctly */
        __asm__ volatile ("pause");
    }

    /* Save the flags so release can restore them */
    lock->flags = flags;
}

/* Release the spinlock — restores previous interrupt state */
void spinlock_release(spinlock_t *lock) {
    uint64_t flags = lock->flags;

    /* Release the lock */
    __sync_lock_release(&lock->lock);

    /* Restore interrupt flag if it was previously enabled */
    if (flags & (1 << 9)) {  /* IF bit in RFLAGS */
        __asm__ volatile ("sti");
    }
}
