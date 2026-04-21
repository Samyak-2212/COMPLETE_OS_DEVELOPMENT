/* ============================================================================
 * process.c — NexusOS Process Control Block Management
 * Purpose: PCB alloc/free, PID counter, process table, address space setup
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "sched/process.h"
#include "sched/thread.h"
#include "sched/scheduler.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/debug.h"

/* ── Globals ────────────────────────────────────────────────────────────────── */

/* Currently executing process and thread (updated by scheduler on every switch) */
process_t *current_process = NULL;
thread_t  *current_thread  = NULL;

/* ── Process table ─────────────────────────────────────────────────────────── */

static process_t *proc_table_head = NULL;

/* Monotonically increasing PID counter */
static volatile int32_t next_pid = 0;

/* Shared root CWD string — avoids 256-byte inline in every PCB */
static char cwd_root[] = "/";

/* ── Helpers ───────────────────────────────────────────────────────────────── */

/* Clone the kernel-half PML4 entries (indices 256–511) into a fresh PML4.
 * Implemented here in process.c — avoids touching vmm.c (protected later). */
static uint64_t process_alloc_address_space(void) {
    uint64_t hhdm = vmm_get_hhdm_offset();

    /* Allocate a new PML4 page */
    uint64_t new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) return 0;

    uint64_t *new_pml4 = (uint64_t *)(new_pml4_phys + hhdm);
    memset(new_pml4, 0, 4096);

    /* Read current CR3 to find kernel PML4 */
    uint64_t cur_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cur_cr3));
    uint64_t *cur_pml4 = (uint64_t *)((cur_cr3 & 0x000FFFFFFFFFF000ULL) + hhdm);

    /* Copy kernel upper-half entries (indices 256–511) into the new PML4.
     * This ensures the new process can access kernel code/data/heap. */
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = cur_pml4[i];
    }

    return new_pml4_phys;
}

/* ── Public API ────────────────────────────────────────────────────────────── */

/* process_init — initialize the process subsystem.
 * Must be called once at boot before any process_create(). */
int process_init(void) {
    proc_table_head = NULL;
    next_pid        = 0;
    current_process = NULL;
    current_thread  = NULL;

    debug_log(DEBUG_LEVEL_INFO, "PROC", "Process subsystem initialized.");
    return 0;
}

/* process_get_next_pid — return and increment the global PID counter. */
pid_t process_get_next_pid(void) {
    return (pid_t)__atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
}

/* process_create — allocate a new PCB with given parent PID.
 * Sets up a new PML4 with kernel mappings. Returns NULL on OOM. */
process_t *process_create(pid_t ppid) {
    process_t *p = (process_t *)kmalloc(sizeof(process_t));
    if (!p) return NULL;
    memset(p, 0, sizeof(process_t));

    p->pid   = process_get_next_pid();
    p->ppid  = ppid;
    p->state = PROC_ALIVE;

    /* Credentials: default to root (uid=0) for kernel processes */
    p->uid  = 0;
    p->gid  = 0;
    p->euid = 0;
    p->egid = 0;

    /* Set up address space — clone kernel PML4 upper half */
    p->cr3 = process_alloc_address_space();
    if (!p->cr3) {
        kfree(p);
        return NULL;
    }

    /* Lazy FD table — allocated on first open() call */
    p->fd_table = NULL;

    /* CWD — shared root string, no per-process 256-byte array */
    p->cwd = cwd_root;

    /* No threads yet */
    p->threads      = NULL;
    p->thread_count = 0;

    /* Add to process table */
    p->next         = proc_table_head;
    proc_table_head = p;

    debug_log(DEBUG_LEVEL_INFO, "PROC", "Created PID %d (ppid=%d) cr3=0x%llx",
              (int)p->pid, (int)p->ppid, (unsigned long long)p->cr3);

    return p;
}

/* process_get — look up a process by PID. O(n) scan. */
process_t *process_get(pid_t pid) {
    for (process_t *p = proc_table_head; p; p = p->next) {
        if (p->pid == pid) return p;
    }
    return NULL;
}

/* process_destroy — free all threads, VMAs, address space, FD table, and the PCB.
 * The process must not be current_process when this is called. */
void process_destroy(process_t *proc) {
    if (!proc) return;

    /* Destroy all threads */
    thread_t *t = proc->threads;
    while (t) {
        thread_t *next = t->next_in_proc;
        thread_destroy(t);
        t = next;
    }
    proc->threads      = NULL;
    proc->thread_count = 0;

    /* Free all user VMAs */
    vma_t *vma = proc->vmas;
    while (vma) {
        vma_t *next = vma->next;
        kfree(vma);
        vma = next;
    }
    proc->vmas = NULL;

    /* Free entire user address space (pages + page tables), COW-safe */
    if (proc->cr3) {
        vmm_destroy_address_space(proc->cr3);
        proc->cr3 = 0;
    }

    /* Free lazy FD table if allocated */
    if (proc->fd_table) {
        kfree(proc->fd_table);
        proc->fd_table = NULL;
    }

    /* Remove from process table */
    if (proc_table_head == proc) {
        proc_table_head = proc->next;
    } else {
        for (process_t *p = proc_table_head; p && p->next; p = p->next) {
            if (p->next == proc) {
                p->next = proc->next;
                break;
            }
        }
    }

    debug_log(DEBUG_LEVEL_INFO, "PROC", "Destroyed PID %d", (int)proc->pid);
    kfree(proc);
}

/* process_exit — mark process as ZOMBIE with given exit code.
 * All threads are marked DEAD and dequeued. schedule() is called. */
int process_exit(process_t *proc, int code) {
    if (!proc) return -1;

    proc->exit_code = code;
    proc->state     = PROC_ZOMBIE;

    /* Mark all threads dead and dequeue them */
    for (thread_t *t = proc->threads; t; t = t->next_in_proc) {
        if (t->state != THREAD_DEAD) {
            scheduler_dequeue(t);
            t->state = THREAD_DEAD;
        }
    }

    debug_log(DEBUG_LEVEL_INFO, "PROC", "PID %d exited code=%d", (int)proc->pid, code);

    /* If we just killed the current thread, schedule something else */
    if (proc == current_process) {
        schedule();
    }

    return 0;
}

/* process_exit_current — exit the current process with given code. */
void process_exit_current(int code) {
    process_exit(current_process, code);
}

/* process_get_uid — return the real UID of a process. */
uint32_t process_get_uid(pid_t pid) {
    process_t *p = process_get(pid);
    return p ? p->uid : (uint32_t)-1;
}

/* process_get_gid — return the real GID of a process. */
uint32_t process_get_gid(pid_t pid) {
    process_t *p = process_get(pid);
    return p ? p->gid : (uint32_t)-1;
}

/* process_waitpid — Reap zombie children.
 * Returns reaped PID if successful, 0 if children exist but none are zombies, -1 if no children.
 */
pid_t process_waitpid(pid_t parent_pid, pid_t target_pid, int *status) {
    int has_children = 0;
    for (process_t *p = proc_table_head; p; p = p->next) {
        if (p->ppid == parent_pid) {
            if (target_pid == -1 || p->pid == target_pid) {
                has_children = 1;
                if (p->state == PROC_ZOMBIE) {
                    pid_t zpid = p->pid;
                    if (status) {
                        /* WEXITSTATUS format: (code & 0xFF) << 8 */
                        *status = (p->exit_code & 0xFF) << 8;
                    }
                    /* process_destroy does safe unlinking */
                    process_destroy(p);
                    return zpid;
                }
            }
        }
    }
    return has_children ? 0 : -1;
}
