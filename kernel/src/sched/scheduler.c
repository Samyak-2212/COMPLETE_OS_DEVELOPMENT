/* ============================================================================
 * scheduler.c — NexusOS Preemptive Multi-Level Round-Robin Scheduler
 * Purpose: Timer IRQ-driven preemptive scheduler. 8 priority queues.
 *          Idle thread runs when all queues empty. TSS RSP0 updated per switch.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "sched/scheduler.h"
#include "sched/process.h"
#include "sched/thread.h"
#include "hal/isr.h"
#include "hal/pic.h"
#include "hal/gdt.h"
#include "hal/io.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "lib/debug.h"
#include "drivers/timer/pit.h"
#include "config/nexus_config.h"

/* ── External assembly state ────────────────────────────────────────────── */
extern void switch_context(uint64_t *old_rsp_out, uint64_t new_rsp);
extern uint64_t kernel_rsp_scratch;

/* ── Run queues: one FIFO per priority level ──────────────────────────────── */
/* Each queue is a singly-linked list via next_in_queue. */
static thread_t *ready_head[SCHED_NUM_PRIORITIES];
static thread_t *ready_tail[SCHED_NUM_PRIORITIES];

/* ── Scheduler state ──────────────────────────────────────────────────────── */
static volatile uint64_t tick_counter   = 0;  /* Quantum tick counter */
static volatile int      sched_active   = 0;  /* 1 after scheduler_start() */
volatile int             sched_lock     = 0;  /* Re-entrant guard (single CPU) */

/* Idle process and thread (PID 0, TID 0) */
static process_t *idle_process = NULL;
static thread_t  *idle_thread  = NULL;

/* ── Idle thread function ──────────────────────────────────────────────────── */
static void idle_thread_fn(uint64_t arg1, uint64_t arg2) {
    (void)arg1; (void)arg2;
    for (;;) {
        __asm__ volatile("sti; hlt; cli");
    }
}

/* (Test threads removed to stop serial spam, we now schedule the kernel shell) */

/* ── Queue helpers ────────────────────────────────────────────────────────── */

/* scheduler_enqueue — append thread to the tail of its priority queue. */
void scheduler_enqueue(thread_t *t) {
    if (!t) return;
    uint8_t pri = t->priority;
    if (pri >= SCHED_NUM_PRIORITIES) pri = SCHED_PRIORITY_NORMAL;

    t->next_in_queue = NULL;

    if (!ready_head[pri]) {
        ready_head[pri] = t;
        ready_tail[pri] = t;
    } else {
        ready_tail[pri]->next_in_queue = t;
        ready_tail[pri] = t;
    }
    t->state = THREAD_READY;
}

/* scheduler_dequeue — remove a specific thread from its priority queue. */
void scheduler_dequeue(thread_t *t) {
    if (!t) return;
    uint8_t pri = t->priority;
    if (pri >= SCHED_NUM_PRIORITIES) pri = SCHED_PRIORITY_NORMAL;

    thread_t *prev = NULL;
    thread_t *cur  = ready_head[pri];

    while (cur) {
        if (cur == t) {
            if (prev) {
                prev->next_in_queue = cur->next_in_queue;
            } else {
                ready_head[pri] = cur->next_in_queue;
            }
            if (ready_tail[pri] == cur) {
                ready_tail[pri] = prev;
            }
            cur->next_in_queue = NULL;
            return;
        }
        prev = cur;
        cur  = cur->next_in_queue;
    }
}

/* pick_next — select the highest-priority READY thread.
 * Uses a round-robin within each priority level by dequeuing from head
 * and re-enqueueing at tail after selection (happens in schedule()). */
static thread_t *pick_next(void) {
    for (int pri = SCHED_NUM_PRIORITIES - 1; pri >= 0; pri--) {
        /* Wake any sleeping threads whose timer has expired */
        thread_t *t = ready_head[pri];
        while (t) {
            thread_t *next = t->next_in_queue;
            if (t->state == THREAD_SLEEPING &&
                pit_get_ticks() >= t->sleep_until_ms) {
                scheduler_dequeue(t);
                t->state = THREAD_READY;
                scheduler_enqueue(t);
            }
            t = next;
        }

        if (ready_head[pri]) {
            /* Dequeue from head for round-robin fairness */
            thread_t *chosen = ready_head[pri];
            scheduler_dequeue(chosen);
            return chosen;
        }
    }
    return NULL; /* Only idle thread available */
}

/* ── Core schedule ────────────────────────────────────────────────────────── */

/* schedule — perform a context switch to the next READY thread.
 * Re-entrant guard prevents nested switches on a single CPU.
 * TSS RSP0 is updated so interrupts from ring 3 land on the correct stack. */
void schedule(void) {
    /* Re-entrant guard */
    if (sched_lock) return;
    sched_lock = 1;

    thread_t *old = current_thread;
    thread_t *next = pick_next();

    if (!next) {
        /* No user thread ready — run idle */
        if (old != idle_thread) {
            next = idle_thread;
        } else {
            sched_lock = 0;
            return; /* Already idle, nothing to do */
        }
    }

    /* If the same thread won again, re-enqueue it and return */
    if (next == old) {
        scheduler_enqueue(next);
        sched_lock = 0;
        return;
    }

    /* Re-enqueue the old thread if it is still runnable */
    if (old && old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
        scheduler_enqueue(old);
    }

    /* Update scheduler globals */
    next->state    = THREAD_RUNNING;
    current_thread  = next;
    current_process = next->process;

    /* Update TSS RSP0 — kernel stack top for ring-3 → ring-0 transitions (for HW interrupts) */
    tss_set_rsp0(next->kstack_virt);

    /* Update kernel_rsp_scratch for SYSCALL fast-path */
    kernel_rsp_scratch = next->kstack_virt;

    /* Switch address space if the new thread belongs to a different process */
    if (old && old->process && next->process != old->process) {
        __asm__ volatile("mov %0, %%cr3" : : "r"(next->process->cr3) : "memory");
    }

    /*
    debug_log(DEBUG_LEVEL_INFO, "SCHED",
              "Switch TID %u->%u PID %d->%d",
              old  ? (unsigned int)old->tid  : 0u,
              (unsigned int)next->tid,
              old  ? (int)old->process->pid  : -1,
              (int)next->process->pid);
    */

    /* Perform the actual context switch */
    if (old) {
        switch_context(&old->kstack_rsp, next->kstack_rsp);
    } else {
        /* First ever switch — no old context to save */
        __asm__ volatile(
            "mov %0, %%rsp\n\t"
            "pop %%r15\n\t"
            "pop %%r14\n\t"
            "pop %%r13\n\t"
            "pop %%r12\n\t"
            "pop %%rbp\n\t"
            "pop %%rbx\n\t"
            "ret\n\t"
            : : "r"(next->kstack_rsp) : "memory"
        );
    }

    /* Execution returns here when THIS thread is re-scheduled */
    sched_lock = 0;
}

/* ── Timer IRQ handler ────────────────────────────────────────────────────── */

/* scheduler_timer_handler — replaces the PIT IRQ0 handler.
 * Must increment the PIT's tick counter (via pit_tick_increment) so that
 * pit_get_ticks() and pit_sleep_ms() remain functional. */
static void scheduler_timer_handler(registers_t *regs) {
    (void)regs;

    /* Keep the PIT tick counter running */
    pit_tick_increment();

    /* Send EOI before any potential context switch */
    pic_send_eoi(0);

    if (!sched_active) return;

    scheduler_tick();
}

/* ── scheduler_tick ────────────────────────────────────────────────────────── */

/* scheduler_tick — called every ms from timer IRQ.
 * Counts up to SCHED_QUANTUM_MS then calls schedule(). */
void scheduler_tick(void) {
    /* Wake sleeping threads (checked inside pick_next too, but preempt check here) */
    tick_counter++;
    if (tick_counter >= (uint64_t)SCHED_QUANTUM_MS) {
        tick_counter = 0;
        schedule();
    }
}

/* ── scheduler_block ─────────────────────────────────────────────────────── */

/* scheduler_block — block the current thread and reschedule. */
void scheduler_block(thread_state_t reason) {
    if (!current_thread) return;
    current_thread->state = reason;
    scheduler_dequeue(current_thread);
    schedule();
}

/* ── scheduler_wake ──────────────────────────────────────────────────────── */

/* scheduler_wake — wake a blocked or sleeping thread. */
void scheduler_wake(thread_t *t) {
    if (!t) return;
    if (t->state == THREAD_BLOCKED || t->state == THREAD_SLEEPING) {
        t->state = THREAD_READY;
        scheduler_enqueue(t);
    }
}

/* ── scheduler_exit_current ──────────────────────────────────────────────── */

/* scheduler_exit_current — mark thread DEAD and schedule next. */
void scheduler_exit_current(int code) {
    process_exit(current_process, code);
}

/* ── scheduler_init ──────────────────────────────────────────────────────── */

/* scheduler_init — set up run queues, create idle thread, hook IRQ0,
 * create two test threads to verify interleaved execution. */
int scheduler_init(void) {
    /* Clear all run queues */
    for (int i = 0; i < SCHED_NUM_PRIORITIES; i++) {
        ready_head[i] = NULL;
        ready_tail[i] = NULL;
    }

    /* Initialize process subsystem */
    process_init();

    /* ── Create idle process (PID 0) ── */
    idle_process = process_create(-1);  /* ppid=-1 means no parent */
    if (!idle_process) return -1;

    idle_thread = thread_create(idle_process, (uint64_t)idle_thread_fn, 0, 0);
    if (!idle_thread) return -1;

    idle_thread->priority = SCHED_PRIORITY_IDLE;
    /* Idle thread is NOT enqueued — picked only when all queues empty */
    idle_thread->state = THREAD_READY;

    /* ── Create shell thread ── */
    extern void shell_run(void);
    process_t *shell_proc = process_create(0);
    if (shell_proc) {
        thread_t *shell_th = thread_create(shell_proc, (uint64_t)shell_run, 0, 0);
        if (shell_th) {
            shell_th->priority = SCHED_PRIORITY_NORMAL;
            scheduler_enqueue(shell_th);
        }
    }

    /* Replace PIT IRQ0 handler with scheduler timer handler */
    isr_register_handler(IRQ0, scheduler_timer_handler);
    pic_clear_mask(0);  /* Ensure IRQ0 is unmasked */

    debug_log(DEBUG_LEVEL_INFO, "SCHED",
              "Scheduler initialized: idle PID=%d idle TID=%u",
              (int)idle_process->pid, (unsigned int)idle_thread->tid);

    return 0;
}

/* ── scheduler_start ─────────────────────────────────────────────────────── */

/* scheduler_start — bootstrap: pick first thread, enable preemption.
 * Sets current_thread, updates TSS, jumps into first thread. Never returns. */
void scheduler_start(void) {
    thread_t *first = pick_next();
    if (!first) {
        /* No user threads — boot into idle */
        first = idle_thread;
    }

    first->state    = THREAD_RUNNING;
    current_thread  = first;
    current_process = first->process;

    /* Update TSS RSP0 and syscall fast-path stack */
    tss_set_rsp0(first->kstack_virt);
    kernel_rsp_scratch = first->kstack_virt;

    /* Load the new process's address space */
    __asm__ volatile("mov %0, %%cr3" : : "r"(first->process->cr3) : "memory");

    sched_active = 1;

    debug_log(DEBUG_LEVEL_INFO, "SCHED",
              "Scheduler started: first TID=%u PID=%d",
              (unsigned int)first->tid, (int)first->process->pid);

    /* Jump into first thread's kernel stack — unwind its pre-populated frame */
    __asm__ volatile(
        "mov %0, %%rsp\n\t"
        "pop %%r15\n\t"
        "pop %%r14\n\t"
        "pop %%r13\n\t"
        "pop %%r12\n\t"
        "pop %%rbp\n\t"
        "pop %%rbx\n\t"
        "sti\n\t"
        "ret\n\t"
        : : "r"(first->kstack_rsp) : "memory"
    );

    __builtin_unreachable();
}

/* ── Accessors ────────────────────────────────────────────────────────────── */

/* scheduler_current_thread — return the currently running thread. */
thread_t *scheduler_current_thread(void) {
    return current_thread;
}

/* scheduler_current_process — return the currently running process. */
process_t *scheduler_current_process(void) {
    return current_process;
}
