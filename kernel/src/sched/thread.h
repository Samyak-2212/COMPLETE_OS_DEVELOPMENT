/* ============================================================================
 * thread.h — NexusOS Thread Control Block
 * Purpose: TCB struct, kernel stack management, thread lifecycle API
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_SCHED_THREAD_H
#define NEXUS_SCHED_THREAD_H

#include <stdint.h>
#include <stddef.h>
#include "config/nexus_config.h"

/* Forward declaration — process.h includes thread.h, break cycle */
struct process;

typedef uint32_t tid_t;

/* ── Thread states ────────────────────────────────────────────────────────── */

typedef enum {
    THREAD_READY    = 0,   /* In run queue, waiting for CPU */
    THREAD_RUNNING  = 1,   /* Currently on CPU */
    THREAD_BLOCKED  = 2,   /* Waiting for I/O or event */
    THREAD_SLEEPING = 3,   /* Sleeping for N ms */
    THREAD_DEAD     = 4,   /* Exited, waiting for cleanup */
} thread_state_t;

/* ── Saved CPU context ─────────────────────────────────────────────────────
 * Layout MUST match offsets in context.asm.
 * Only callee-saved registers are stored here; the return address (RIP)
 * is the topmost value on the kernel stack when switch_context is called. */
typedef struct cpu_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} __attribute__((packed)) cpu_context_t;

/* ── Thread Control Block ─────────────────────────────────────────────────── */

typedef struct thread {
    tid_t              tid;
    thread_state_t     state;
    uint8_t            priority;        /* SCHED_PRIORITY_* */

    /* Kernel stack — allocated by thread_create */
    uint64_t           kstack_phys;     /* Physical base of kernel stack page */
    uint64_t           kstack_virt;     /* Virtual top of kernel stack (stack grows ↓) */
    uint64_t           kstack_rsp;      /* Saved RSP (updated on each context switch) */

    /* User stack (NULL for kernel threads) */
    uint64_t           ustack_virt_top;

    /* Sleep support */
    uint64_t           sleep_until_ms;  /* Wake when pit_get_ticks() >= this */

    /* Linked list pointers */
    struct thread     *next_in_proc;    /* Sibling threads in same process */
    struct thread     *next_in_queue;   /* Scheduler run-queue link */

    /* Back-pointer to owning process */
    struct process    *process;
} thread_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Create a kernel thread in proc with given entry point and two arguments.
 * Allocates and pre-populates the kernel stack. Returns NULL on OOM. */
thread_t *thread_create(struct process *proc, uint64_t entry_rip,
                        uint64_t arg1, uint64_t arg2);

/* Destroy a thread: frees kernel stack and TCB. Must not be running. */
void      thread_destroy(thread_t *t);

/* Block the calling thread for ms milliseconds (calls schedule() inside). */
void      thread_sleep_ms(uint64_t ms);

/* Entry stub declared here so context.asm can export it and scheduler.c
 * can reference the symbol for pre-populating new thread stacks. */
extern void thread_entry_stub(void);

#endif /* NEXUS_SCHED_THREAD_H */
