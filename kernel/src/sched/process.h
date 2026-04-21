/* ============================================================================
 * process.h — NexusOS Process Control Block
 * Purpose: PCB struct with credentials, address space, thread list, FD table
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_SCHED_PROCESS_H
#define NEXUS_SCHED_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "thread.h"
#include "config/nexus_config.h"

typedef int32_t pid_t;

/* ── Virtual Memory Area (VMA) ─────────────────────────────────────────────── */

typedef struct vma {
    uint64_t       virt_start;
    uint64_t       virt_end;
    uint32_t       flags;          /* PAGE_WRITABLE, etc. */
    struct vma    *next;
} vma_t;

/* ── Process state ────────────────────────────────────────────────────────── */

typedef enum {
    PROC_ALIVE  = 0,
    PROC_ZOMBIE = 1,   /* Exited, parent not yet waited */
    PROC_DEAD   = 2,   /* Fully cleaned up */
} proc_state_t;

/* ── Lazy file-descriptor table ───────────────────────────────────────────── */

typedef struct fd_table {
    void     *fds[64];      /* vfs_node_t* — NULL = closed slot */
    uint32_t  open_count;
} fd_table_t;

/* ── Process Control Block ────────────────────────────────────────────────── */

typedef struct process {
    pid_t          pid;
    pid_t          ppid;           /* Parent PID */
    proc_state_t   state;

    /* Credentials — foundation for multi-user support */
    uint32_t       uid;            /* Real user ID */
    uint32_t       gid;            /* Real group ID */
    uint32_t       euid;           /* Effective user ID */
    uint32_t       egid;           /* Effective group ID */

    /* Virtual address space */
    uint64_t       cr3;            /* Physical address of PML4 page table */

    /* Thread list */
    thread_t      *threads;        /* Linked list head */
    uint32_t       thread_count;

    /* Lazy FD table — NULL until first open() call (saves 512+ B per process) */
    fd_table_t    *fd_table;

    /* Working directory — pointer, shared "/" string (saves 256 B per process) */
    char          *cwd;

    /* Exit code (valid when PROC_ZOMBIE) */
    int            exit_code;

    /* Heap layout (set by exec/brk) */
    uint64_t       heap_base;      /* Current brk start */
    uint64_t       heap_end;       /* Current brk end */

    /* User stack limits (for demand-paged stack growth) */
    uint64_t       stack_top_virt;   /* Top of user stack virtual range */
    uint64_t       stack_limit_virt; /* Current bottom of mapped stack */

    /* Virtual Memory Areas (demand-paged segments) */
    vma_t         *vmas;

    /* Linked list — kernel process table */
    struct process *next;
} process_t;

/* ── Globals ──────────────────────────────────────────────────────────────── */

extern process_t *current_process;  /* Currently executing process */
extern thread_t  *current_thread;   /* Currently executing thread */

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Initialize the process subsystem. Must be called before process_create(). */
int        process_init(void);

/* Allocate a new PCB with the given parent PID. Returns NULL on OOM. */
process_t *process_create(pid_t ppid);

/* Look up a process by PID. Returns NULL if not found. */
process_t *process_get(pid_t pid);

/* Destroy a PCB: kills all threads, frees address space and PCB. */
void       process_destroy(process_t *proc);

/* Mark process as ZOMBIE with given exit code; schedule next thread. */
int        process_exit(process_t *proc, int code);

/* Convenience: exit current_process */
void       process_exit_current(int code);

/* Allocate the next unique PID (atomic-safe on single CPU). */
pid_t      process_get_next_pid(void);

pid_t      process_waitpid(pid_t parent_pid, pid_t target_pid, int *status);

/* Credential accessors */
uint32_t   process_get_uid(pid_t pid);
uint32_t   process_get_gid(pid_t pid);

/* VMA management */
void       process_add_vma(process_t *proc, uint64_t start, uint64_t end, uint32_t flags);
vma_t     *process_find_vma(process_t *proc, uint64_t addr);

#endif /* NEXUS_SCHED_PROCESS_H */
