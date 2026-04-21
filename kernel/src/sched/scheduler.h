/* ============================================================================
 * scheduler.h — NexusOS Preemptive Round-Robin Scheduler
 * Purpose: Public API for the multi-level priority round-robin scheduler
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_SCHED_SCHEDULER_H
#define NEXUS_SCHED_SCHEDULER_H

#include "process.h"
#include "thread.h"

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialize scheduler: create idle process (PID 0), hook timer IRQ.
 * Also creates two test threads to verify interleaved execution.
 * Returns 0 on success, -1 on failure. */
int  scheduler_init(void);

/* Called from timer IRQ handler every ms. Decrements quantum; calls
 * schedule() when quantum expires. */
void scheduler_tick(void);

/* Force an immediate reschedule: find next READY thread and switch. */
void schedule(void);

/* Add a thread to the appropriate priority run queue. */
void scheduler_enqueue(thread_t *t);

/* Remove a thread from its run queue (used when blocking). */
void scheduler_dequeue(thread_t *t);

/* Block the current thread with the given reason state. Calls schedule(). */
void scheduler_block(thread_state_t reason);

/* Wake a blocked/sleeping thread: set READY and enqueue. */
void scheduler_wake(thread_t *t);

/* Start the scheduler: load first thread, enable preemption.
 * This function NEVER returns. */
void scheduler_start(void);

/* Called when current thread exits: mark DEAD, schedule next. */
void scheduler_exit_current(int code);

/* Accessors for currently running thread/process. */
thread_t  *scheduler_current_thread(void);
process_t *scheduler_current_process(void);

#endif /* NEXUS_SCHED_SCHEDULER_H */
