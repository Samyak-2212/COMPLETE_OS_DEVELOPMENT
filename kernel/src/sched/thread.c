/* ============================================================================
 * thread.c — NexusOS Thread Control Block Management
 * Purpose: Allocate/free TCBs, set up kernel stacks for new threads
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "sched/thread.h"
#include "sched/process.h"
#include "sched/scheduler.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/debug.h"
#include "drivers/timer/pit.h"

/* ── Static TID counter ────────────────────────────────────────────────────── */

static volatile uint32_t next_tid = 0;

/* ── Internal helpers ──────────────────────────────────────────────────────── */

/* Allocate the next unique TID. */
static tid_t alloc_tid(void) {
    return (tid_t)__atomic_fetch_add(&next_tid, 1, __ATOMIC_SEQ_CST);
}

/* ── Public API ────────────────────────────────────────────────────────────── */

/* thread_create — allocate a TCB and a kernel stack for the new thread.
 * Pre-populates the kernel stack so switch_context() will correctly
 * call thread_entry_stub on first schedule. Returns NULL on OOM. */
thread_t *thread_create(struct process *proc, uint64_t entry_rip,
                         uint64_t arg1, uint64_t arg2) {
    if (!proc) return NULL;

    /* Allocate TCB */
    thread_t *t = (thread_t *)kmalloc(sizeof(thread_t));
    if (!t) return NULL;
    memset(t, 0, sizeof(thread_t));

    t->tid      = alloc_tid();
    t->state    = THREAD_READY;
    t->priority = SCHED_PRIORITY_NORMAL;
    t->process  = proc;

    /* Allocate one physical page for the kernel stack */
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        kfree(t);
        return NULL;
    }

    /* Use the HHDM direct mapping as the virtual address of the stack page.
     * Stack grows downward; kstack_virt = top of the page (exclusive). */
    uint64_t hhdm = vmm_get_hhdm_offset();
    uint64_t stack_page_virt = phys + hhdm;

    t->kstack_phys = phys;
    t->kstack_virt = stack_page_virt + THREAD_KERNEL_STACK_SIZE; /* top of page */

    /* Zero the stack page */
    memset((void *)stack_page_virt, 0, THREAD_KERNEL_STACK_SIZE);

    /* Pre-populate the kernel stack layout so switch_context() → ret → thread_entry_stub.
     *
     * push order (SP decrements before each write):
     *   [top]  arg2           ← stub pops as rdx (3rd pop)
     *          arg1           ← stub pops as rsi (2nd pop)
     *          entry_rip      ← stub pops as entry fn ptr (1st pop)
     *          thread_entry_stub  ← ret target for switch_context
     *          0 (r15)
     *          0 (r14)
     *          0 (r13)
     *          0 (r12)
     *          0 (rbp)
     *   [rsp]  0 (rbx)        ← switch_context restores from here
     */
    uint64_t *sp = (uint64_t *)t->kstack_virt;

    *--sp = arg2;
    *--sp = arg1;
    *--sp = entry_rip;
    *--sp = (uint64_t)thread_entry_stub;  /* ret target */

    /* Callee-saved registers = 0 (switch_context pops r15..rbx in that order) */
    *--sp = 0; /* r15 */
    *--sp = 0; /* r14 */
    *--sp = 0; /* r13 */
    *--sp = 0; /* r12 */
    *--sp = 0; /* rbp */
    *--sp = 0; /* rbx */

    t->kstack_rsp = (uint64_t)sp;

    /* Append thread to process thread list */
    t->next_in_proc = proc->threads;
    proc->threads   = t;
    proc->thread_count++;

    debug_log(DEBUG_LEVEL_INFO, "THREAD", "Created TID %u entry=0x%llx RSP=0x%llx",
              (unsigned int)t->tid,
              (unsigned long long)entry_rip,
              (unsigned long long)t->kstack_rsp);

    return t;
}

/* thread_destroy — free the kernel stack physical page and the TCB.
 * Caller must ensure thread is not running and has been dequeued. */
void thread_destroy(thread_t *t) {
    if (!t) return;

    if (t->kstack_phys) {
        pmm_free_page(t->kstack_phys);
        t->kstack_phys = 0;
    }

    debug_log(DEBUG_LEVEL_INFO, "THREAD", "Destroyed TID %u", (unsigned int)t->tid);
    kfree(t);
}

/* thread_sleep_ms — block the calling thread for at least ms milliseconds.
 * Sets sleep_until_ms and calls scheduler_block(THREAD_SLEEPING). */
void thread_sleep_ms(uint64_t ms) {
    thread_t *t = scheduler_current_thread();
    if (!t) return;

    t->sleep_until_ms = pit_get_ticks() + ms;
    scheduler_block(THREAD_SLEEPING);
}
