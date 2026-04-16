# NexusOS Phase 4 — Agent A: Scheduler & Process Model

> **Role**: Kernel Agent  
> **Tasks**: 4.1 (Process/Thread model), 4.2 (Context switch + Scheduler)  
> **Files Owned**: `kernel/src/sched/`, `kernel/src/arch/x86_64/context.asm`  
> **Must Complete Before**: Agent B (Syscall) and App Agent start  
> **Estimated Complexity**: HIGH

---

## 0. Mandatory Pre-Reading

Before writing a single line of code, read these files in order:

1. `AGENTS.md` — onboarding protocol
2. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md` — rules, boundaries
3. `knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md` — stable APIs
4. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md` — what exists
5. `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md` — open bugs (esp. BUG-003)
6. `kernel/src/kernel.c` — current init sequence (DO NOT MODIFY without permission)
7. `kernel/src/hal/gdt.h` + `gdt.c` — TSS layout (you will update RSP0)
8. `kernel/src/mm/vmm.h` — vmm_map_page, vmm_unmap_page
9. `kernel/src/mm/heap.h` — kmalloc/kfree
10. `kernel/src/drivers/timer/pit.h` — pit_get_ticks (timer source)

---

## 1. Overview

You implement:
- `process_t`: Process Control Block (PCB)
- `thread_t`: Thread Control Block (TCB)  
- `scheduler.c`: Round-robin preemptive scheduler
- `context.asm`: Assembly context switch routine
- Config: `kernel/src/config/nexus_config.h`

**You do NOT touch**:
- `kernel.c` (protected — write init stub call, coordinate with user)
- `hal/` (GDT TSS update via exported function only)
- `mm/` (use API only)
- `syscall/` (Agent B's domain)

---

## 2. File Layout to Create

```
kernel/src/sched/
├── process.h          ← PCB struct, PID management, API
├── process.c          ← PCB alloc/free, PID counter
├── thread.h           ← TCB struct, kernel stack, API  
├── thread.c           ← TCB alloc/free, kernel stack alloc
├── scheduler.h        ← scheduler API (public)
└── scheduler.c        ← round-robin, timer integration, TSS update

kernel/src/arch/x86_64/
└── context.asm        ← switch_context(old_rsp**, new_rsp*)

kernel/src/config/
└── nexus_config.h     ← all compile-time tunable constants

docs/phase4/
└── scheduler.md       ← your documentation (write LAST)
```

---

## 3. Configuration System

**Create this file first**: `kernel/src/config/nexus_config.h`

```c
/* ============================================================================
 * nexus_config.h — NexusOS Kernel Configuration Constants
 * Purpose: All tunable compile-time parameters. Edit this file to change
 *          kernel behavior without modifying source code. Every constant
 *          has a comment explaining valid range and effect.
 * ============================================================================ */

#ifndef NEXUS_CONFIG_H
#define NEXUS_CONFIG_H

/* ── Scheduler ─────────────────────────────────────────────────────────────── */

/* Scheduler quantum in milliseconds (PIT ticks). Range: 1–100.
 * Lower = more responsive but higher overhead. Default: 10ms */
#define SCHED_QUANTUM_MS          10

/* Maximum number of simultaneous processes. Range: 16–4096. Default: 256 */
#define SCHED_MAX_PROCESSES       256

/* Maximum threads per process. Range: 1–16. Default: 8
 * EFFICIENCY: reduced from 16. Most processes need 1–2 threads. */
#define SCHED_MAX_THREADS_PER_PROC 8

/* Kernel stack size per thread. 4 KB = 1 page (was 8 KB).
 * EFFICIENCY: 4 KB sufficient for 5–8 call levels. Guard page detects overflow.
 * Use THREAD_KERNEL_STACK_SIZE_LARGE for deep-call kernel threads. */
#define THREAD_KERNEL_STACK_SIZE        4096
#define THREAD_KERNEL_STACK_SIZE_LARGE  8192

/* Enable guard page (1 unmapped page) below every kernel stack.
 * Cost: 4 KB virtual (no physical). Benefit: catches stack overflows. */
#define THREAD_KERNEL_STACK_GUARD  1

/* User stack INITIAL size in bytes — demand-paged, grows on #PF.
 * EFFICIENCY: Only 4 KB allocated at exec time. Not 8 MB upfront.
 * Grows automatically via page fault handler up to PROC_USER_STACK_MAX. */
#define PROC_USER_STACK_INITIAL    4096
#define PROC_USER_STACK_MAX        (8 * 1024 * 1024)   /* 8 MB virtual limit */

/* User stack virtual top (grows DOWN from here) */
#define PROC_USER_STACK_TOP        0x00007FFFFFFFFFFF

/* Heap initial pages for KERNEL heap. 8 pages = 32 KB initial.
 * EFFICIENCY: was 64 pages (256 KB). Expands automatically on demand.
 * Pass to heap_init(HEAP_INITIAL_PAGES) */
#define HEAP_INITIAL_PAGES         8
#define HEAP_GROW_PAGES            8

/* ── Memory ────────────────────────────────────────────────────────────────── */

/* User-space virtual address space lower bound */
#define PROC_USER_VIRT_BASE       0x0000000000400000ULL

/* User-space virtual address space upper bound */
#define PROC_USER_VIRT_END        0x00007FFFFFFFFFFFULL

/* Enable COW (copy-on-write) page sharing for fork().
 * EFFICIENCY: fork() copies page table entries only, not physical pages.
 * Physical pages copied only on write. Identical code/data pages shared. */
#define MM_COW_ENABLED             1

/* ── Process IDs ───────────────────────────────────────────────────────────── */

/* PID 0 = idle kernel thread. PID 1 = init. First user PID = 2. */
#define PID_IDLE                  0
#define PID_INIT                  1
#define PID_FIRST_USER            2

/* ── Priority ──────────────────────────────────────────────────────────────── */

/* Priority levels (higher = more CPU time). Range 0–7. */
#define SCHED_PRIORITY_IDLE       0
#define SCHED_PRIORITY_LOW        2
#define SCHED_PRIORITY_NORMAL     4
#define SCHED_PRIORITY_HIGH       6
#define SCHED_PRIORITY_RT         7

/* ── Filesystem / Cache ────────────────────────────────────────────────────── */
/* Block cache page cap. 512 × 4 KB = 2 MB max. Prevents RAM exhaustion. */
#define FS_BLOCK_CACHE_MAX_PAGES   512

/* ── USB ───────────────────────────────────────────────────────────────────── */
#define XHCI_CMD_RING_SIZE         64     /* 64 × 16B = 1 KB */
#define XHCI_EVENT_RING_SIZE       64     /* 64 × 16B = 1 KB */
#define XHCI_TRANSFER_RING_SIZE    16     /* 16 × 16B = 256 B per endpoint */
#define USB_MAX_DEVICES            32

#endif /* NEXUS_CONFIG_H */
```

---

## 4. Data Structures

### 4.1 Thread States

```c
typedef enum {
    THREAD_READY   = 0,   /* In run queue, waiting for CPU */
    THREAD_RUNNING = 1,   /* Currently on CPU */
    THREAD_BLOCKED = 2,   /* Waiting for I/O or event */
    THREAD_SLEEPING= 3,   /* Sleeping for N ms */
    THREAD_DEAD    = 4,   /* Exited, waiting for cleanup */
} thread_state_t;
```

### 4.2 Thread Control Block (`thread.h`)

```c
#ifndef SCHED_THREAD_H
#define SCHED_THREAD_H

#include <stdint.h>
#include <stddef.h>
#include "../../config/nexus_config.h"

typedef uint32_t tid_t;

/* Saved CPU context — layout MUST match context.asm offsets */
typedef struct cpu_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;   /* return address pushed by switch_context call */
} __attribute__((packed)) cpu_context_t;

typedef struct thread {
    tid_t              tid;
    thread_state_t     state;
    uint8_t            priority;       /* SCHED_PRIORITY_* */

    /* Kernel stack — allocated by thread_create */
    uint64_t           kstack_phys;    /* Physical base of kernel stack */
    uint64_t           kstack_virt;    /* Virtual top of kernel stack (stack grows down) */
    uint64_t           kstack_rsp;     /* Saved RSP (updated on context switch) */

    /* User stack (set by exec, NULL for kernel threads) */
    uint64_t           ustack_virt_top;

    /* Sleep support */
    uint64_t           sleep_until_ms; /* Wake when pit_get_ticks() >= this */

    /* Linked list — next thread in process, and next in run queue */
    struct thread     *next_in_proc;   /* Sibling threads in same process */
    struct thread     *next_in_queue;  /* Scheduler run queue link */

    /* Back-pointer to owning process */
    struct process    *process;
} thread_t;

/* API */
thread_t *thread_create(struct process *proc, uint64_t entry_rip,
                        uint64_t arg1, uint64_t arg2);
void      thread_destroy(thread_t *t);
void      thread_sleep_ms(uint64_t ms);   /* Blocks caller */

#endif
```

### 4.3 Process Control Block (`process.h`)

```c
#ifndef SCHED_PROCESS_H
#define SCHED_PROCESS_H

#include <stdint.h>
#include <stddef.h>
#include "thread.h"
#include "../../config/nexus_config.h"

typedef int32_t pid_t;

/* Process state */
typedef enum {
    PROC_ALIVE  = 0,
    PROC_ZOMBIE = 1,   /* Exited, parent not yet waited */
    PROC_DEAD   = 2,   /* Fully cleaned up */
} proc_state_t;

typedef struct process {
    pid_t          pid;
    pid_t          ppid;          /* Parent PID */
    proc_state_t   state;

    /* Credentials — foundation for multi-user support */
    uint32_t       uid;           /* Real user ID */
    uint32_t       gid;           /* Real group ID */
    uint32_t       euid;          /* Effective user ID */
    uint32_t       egid;          /* Effective group ID */

    /* Virtual address space */
    uint64_t       cr3;           /* Physical address of PML4 page table */

    /* Thread list */
    thread_t      *threads;       /* Linked list head */
    uint32_t       thread_count;

    /* File descriptors — array of vfs_node_t* (NULL = closed) */
    /* Phase 4: fd[0]=stdin, fd[1]=stdout, fd[2]=stderr */
    void          *fds[64];       /* Cast to vfs_node_t* — avoid circular include */
    uint32_t       fd_count;

    /* Working directory */
    char           cwd[256];

    /* Exit code (valid when PROC_ZOMBIE) */
    int            exit_code;

    /* Memory layout */
    uint64_t       heap_base;     /* Current brk start */
    uint64_t       heap_end;      /* Current brk end */

    /* Linked list for process table */
    struct process *next;
} process_t;

/* API */
int        process_init(void);                  /* Init process subsystem */
process_t *process_create(pid_t ppid);          /* Allocate new PCB */
process_t *process_get(pid_t pid);              /* Lookup by PID */
void       process_destroy(process_t *proc);    /* Free PCB + all threads */
int        process_exit(process_t *proc, int code);
pid_t      process_get_next_pid(void);

/* Credential helpers (Phase 5: add setuid/setgid here) */
uint32_t   process_get_uid(pid_t pid);
uint32_t   process_get_gid(pid_t pid);

extern process_t *current_process;  /* Currently executing process */
extern thread_t  *current_thread;   /* Currently executing thread */

#endif
```

---

## 5. Context Switch (`context.asm`)

> **Critical**: This file is the lowest-level code in the scheduler. Any mistake causes silent corruption or triple-fault. Study carefully.

```nasm
; context.asm — CPU Context Switch
; Purpose: switch_context(old_rsp_ptr, new_rsp)
;   Saves callee-saved registers + RIP onto old thread's kernel stack.
;   Restores from new thread's kernel stack.
;   Called from scheduler.c via: switch_context(&old->kstack_rsp, new->kstack_rsp)

section .text
global switch_context

; void switch_context(uint64_t *old_rsp_out, uint64_t new_rsp)
;   rdi = pointer to old thread's kstack_rsp field (OUT: we write current RSP here)
;   rsi = new thread's saved RSP (IN: we load this into RSP)
switch_context:
    ; Save callee-saved registers onto CURRENT (old) kernel stack
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save current RSP into old->kstack_rsp
    mov     [rdi], rsp

    ; Switch to new thread's kernel stack
    mov     rsp, rsi

    ; Restore callee-saved registers from NEW kernel stack
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx

    ; Return to wherever new thread was interrupted
    ; (this is the RIP that was pushed when new thread last called switch_context,
    ;  OR it is the entry stub for a brand-new thread)
    ret
```

**New Thread Entry Convention** (IMPORTANT):
When creating a brand-new thread, you must pre-populate its kernel stack so that
`switch_context` returning from it will jump to `thread_entry_stub`:

```nasm
global thread_entry_stub
thread_entry_stub:
    ; rdi = entry function pointer, rsi = arg1, rdx = arg2
    ; These were pushed onto the stack during thread_create
    ; Pop them and call:
    pop     rdi    ; entry_fn
    pop     rsi    ; arg1
    pop     rdx    ; arg2
    call    rdi    ; call entry function
    ; If entry returns, call process_exit
    xor     rdi, rdi
    call    process_exit_current
    ud2     ; Should never reach here
```

In `thread_create()`, pre-populate the new thread's kstack:
```c
/* Pre-populate kernel stack LIFO: r15, r14, r13, r12, rbp, rbx, rip */
uint64_t *sp = (uint64_t *)(kstack_virt);  /* top of stack */
*--sp = arg2;                  /* rdx argument */
*--sp = arg1;                  /* rsi argument */
*--sp = entry_rip;             /* entry function */
*--sp = (uint64_t)thread_entry_stub;  /* "rip" = where ret lands */
/* callee-saved regs = 0 */
*--sp = 0; /* r15 */
*--sp = 0; /* r14 */
*--sp = 0; /* r13 */
*--sp = 0; /* r12 */
*--sp = 0; /* rbp */
*--sp = 0; /* rbx */
t->kstack_rsp = (uint64_t)sp;
```

---

## 6. Scheduler Implementation (`scheduler.c`)

### Algorithm: Multi-level Round-Robin

```
Priority queues: READY_QUEUES[8] (one per priority level 0–7)
Current queue index: iterate highest first (7 → 0)
Each thread gets SCHED_QUANTUM_MS before preemption

On timer IRQ (pit IRQ0, INT32):
  scheduler_tick() called
  tick_counter++
  if (tick_counter >= SCHED_QUANTUM_MS)
    tick_counter = 0
    schedule()   ← pick next thread

schedule():
  Save current thread (already done by timer ISR pushing registers)
  Find next READY thread in priority queues
  If none: run idle thread
  If same thread: return (no switch needed)
  Update TSS.RSP0 = next_thread->kstack_virt (top)
  If next_thread->process != current_process:
    write_cr3(next_thread->process->cr3)
  switch_context(&current_thread->kstack_rsp, next_thread->kstack_rsp)
```

### TSS RSP0 Update

The GDT module already has a TSS. You need to update `tss.rsp0` on every context switch. Export a function from `gdt.c`:

```c
/* In gdt.h — add this declaration */
void gdt_set_kernel_stack(uint64_t rsp0);

/* In gdt.c — implement */
void gdt_set_kernel_stack(uint64_t rsp0) {
    extern tss_t kernel_tss;  /* already defined in gdt.c */
    kernel_tss.rsp0 = rsp0;
}
```

Then call `gdt_set_kernel_stack(next_thread->kstack_virt)` in `schedule()`.

### Idle Thread

- PID 0, TID 0, priority 0
- Entry: `idle_thread_fn()` — loops with `hlt` instruction
- Always READY, never BLOCKED
- Created at `scheduler_init()`

```c
static void idle_thread_fn(uint64_t arg1, uint64_t arg2) {
    (void)arg1; (void)arg2;
    for (;;) {
        __asm__ volatile("sti; hlt; cli");
    }
}
```

---

## 7. Scheduler API (`scheduler.h`)

```c
#ifndef SCHED_SCHEDULER_H
#define SCHED_SCHEDULER_H

#include "process.h"
#include "thread.h"

/* Initialize scheduler, create idle thread (PID 0) */
int  scheduler_init(void);

/* Called from timer IRQ handler (INT32) every ms */
void scheduler_tick(void);

/* Force an immediate reschedule */
void schedule(void);

/* Add thread to run queue */
void scheduler_enqueue(thread_t *t);

/* Remove thread from run queue (when blocking) */
void scheduler_dequeue(thread_t *t);

/* Block current thread until woken */
void scheduler_block(thread_state_t reason);

/* Wake a specific thread */
void scheduler_wake(thread_t *t);

/* Start the scheduler — loads first thread, enables preemption, never returns */
void scheduler_start(void);

/* Called by process on exit — marks thread DEAD, schedules next */
void scheduler_exit_current(int code);

/* Get current running thread/process */
thread_t  *scheduler_current_thread(void);
process_t *scheduler_current_process(void);

#endif
```

---

## 8. Integration with `kernel.c`

**You do NOT modify kernel.c directly.** Instead:

1. After `ata_init_subsystem()` / `ahci_init_subsystem()` / `pci_init()`, the kernel calls `shell_run()` which currently blocks forever.
2. Phase 4 changes this sequence: `shell_run()` is replaced with:
   ```
   scheduler_init()     ← your function
   init_process_start() ← Agent B's function (loads /init from VFS)
   scheduler_start()    ← never returns
   ```
3. Coordinate with user before touching `kernel.c`.

**For testing WITHOUT kernel.c changes**, add a test block inside `scheduler_init()`:
```c
/* Create two test kernel threads that print interleaved */
process_t *p1 = process_create(0);
thread_create(p1, (uint64_t)test_thread_a, 0, 0);
process_t *p2 = process_create(0);
thread_create(p2, (uint64_t)test_thread_b, 0, 0);
```
This lets you verify the scheduler works before Agent B is done.

---

## 9. Timer IRQ Hook

The PIT timer already fires IRQ0 (INT32). You need to hook it:

In `scheduler.c`:
```c
#include "../../hal/isr.h"
#include "../../hal/pic.h"

static void scheduler_timer_handler(registers_t *regs) {
    (void)regs;
    pic_send_eoi(0);   /* IRQ 0 */
    scheduler_tick();
}

int scheduler_init(void) {
    isr_register_handler(32, scheduler_timer_handler); /* INT32 = IRQ0 */
    /* ... rest of init ... */
}
```

> **Note**: The PIT's existing handler in `pit.c` only increments a tick counter. You are REPLACING the IRQ0 handler. After replacement, `pit_get_ticks()` may stop working — you must call `pit_tick_increment()` (or maintain your own counter) inside `scheduler_timer_handler`. Check `pit.c` for how `pit_ticks` is incremented and either keep both calls or expose `pit_tick_increment()`.

---

## 10. Process Address Space

Each process needs its own page table (PML4). You must:

1. On `process_create()`: allocate a new PML4 page via `pmm_alloc_page()`
2. Copy the kernel mappings (upper half, above `0xFFFF800000000000`) from the current CR3 into the new PML4
3. Store the PML4 physical address in `proc->cr3`
4. On `process_destroy()`: free all user-space pages, then free PML4

Helper function to add to `vmm.c` (coordinate with Coordinator — this is shared):
```c
/* Clone kernel PML4 entries into a new page table for a new process */
uint64_t vmm_clone_kernel_space(void);
```
OR: implement entirely in `process.c` using raw page table manipulation. Preferred approach: do it in `process.c` since you own that file.

The kernel upper half is already mapped by Limine. You just need to copy entries 256–511 of the current PML4 into the new one. These indices cover `0xFFFF800000000000` – `0xFFFFFFFFFFFFFFFF`.

---

## 11. Build & Test Protocol

```bash
# After each significant change:
make clean && make all
# Must produce 0 errors, 0 C warnings

# Test scheduler:
make run   # QEMU UEFI
# Or:
make run-bios

# LODBUG serial debug (recommended for scheduler visibility):
make all lodbug && make run lodbug
# Serial output shows scheduler_tick() and switch events
```

### Debugging Scheduler Issues

Use the LODBUG serial debugger (`debugger/`):
```c
#include "../../lib/debug.h"

/* In schedule(): */
debug_log(DEBUG_LEVEL_INFO, "SCHED", "Switch: TID %u -> TID %u (PID %d -> %d)",
    current_thread->tid, next->tid,
    current_process->pid, next->process->pid);
```

If you get a triple-fault:
1. Check kernel stack alignment (RSP must be 16-byte aligned at `call`)
2. Check `kstack_rsp` initialization — verify `thread_create` pre-populate math
3. Use `nxdbg>` console: `regs` command shows register state at fault
4. Add `debug_log` calls before/after `switch_context`

---

## 12. Bug Reporting

If you encounter bugs you cannot solve in this session:
1. Ask user for permission to modify `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md`
2. Log the bug with full detail (up to 3000 words per entry)
3. Keep verbal explanation to user ≤ 15 words

---

## 13. Memory Efficiency Requirements (MANDATORY)

> Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` BEFORE implementing.

### Specific requirements for this agent:

**PCB/TCB slab allocation**:
```c
/* In process.c — create slab caches at subsystem init */
static slab_cache_t *pcb_cache = NULL;
static slab_cache_t *tcb_cache = NULL;

int process_init(void) {
    /* Create kernel/src/mm/slab.h + slab.c if not yet created */
    /* Slab for process_t — 512 bytes obj size, 8-byte align */
    pcb_cache = slab_cache_create("process_t", sizeof(process_t), 8, 8);
    tcb_cache = slab_cache_create("thread_t",  sizeof(thread_t),  8, 16);
    return (pcb_cache && tcb_cache) ? 0 : -1;
}

process_t *process_create(pid_t ppid) {
    process_t *p = slab_cache_alloc(pcb_cache); /* O(1), no fragmentation */
    if (!p) return NULL;
    memset(p, 0, sizeof(process_t));
    /* lazy fd_table: do NOT allocate fd_table here */
    /* allocate only when first open() is called */
    /* p->fd_table = NULL; (already zeroed) */
    return p;
}
```

**Lazy FD table** — add to `process.h`:
```c
typedef struct fd_table {
    void     *fds[64];      /* vfs_node_t* array */
    uint32_t  open_count;
} fd_table_t;

typedef struct process {
    /* ... existing fields ... */
    fd_table_t *fd_table;   /* NULL until first open() call — saves 512+ bytes per process */
    char       *cwd;        /* pointer, NOT inline array — most processes share "/" */
    /* ... */
} process_t;
```

**Kernel stack guard page**:
```c
/* In thread_create(): */
uint64_t phys = pmm_alloc_page();           /* stack physical page */
uint64_t guard_virt = alloc_virt_range(2);  /* 2 virtual pages */
/* guard_virt page 0: NOT mapped (guard) */
vmm_map_page(guard_virt + 4096, phys, PAGE_PRESENT | PAGE_WRITABLE); /* stack */
t->kstack_virt = guard_virt + 4096 + 4096;  /* top of stack = end of mapped page */
```

**Demand-paged user stack**:
```c
/* In exec/init_process_start(): */
/* DO NOT map 8 MB of stack upfront */
/* Map only the initial 4 KB at PROC_USER_STACK_TOP - 4096 */
uint64_t phys = pmm_alloc_page();
vmm_map_page(PROC_USER_STACK_TOP - 4095, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
proc->stack_top_virt  = PROC_USER_STACK_TOP;
proc->stack_limit_virt = PROC_USER_STACK_TOP - 4096;
/* stack_limit_virt decrements each time a stack page fault extends the stack */
```

**COW fork address space**:
```c
/* In sys_fork() (or wherever you implement fork): */
/* 1. Allocate new PML4 (1 page) */
/* 2. Copy kernel half (entries 256-511) from parent PML4 */
/* 3. For user half (entries 0-255): walk parent's PT tree */
/*    - For each mapped user page: */
/*      a. Add refcount: pmm_page_ref(phys) */
/*      b. Mark BOTH parent AND child PTE as read-only (remove PAGE_WRITABLE) */
/*      c. Set COW bit in a shadow structure */
/*    - This means NO physical pages are copied — only PT entries cloned */
/* Result: fork() of a 64 MB process uses ~4 KB additional RAM (one PML4) */
```

**PMM refcount** — add to `kernel/src/mm/pmm.h`:
```c
/* Reference counting for COW pages */
/* Call pmm_page_ref when a page is shared between two PTs */
/* Call pmm_page_unref on PT entry removal; frees when refcount reaches 0 */
void     pmm_page_ref(uint64_t phys);
void     pmm_page_unref(uint64_t phys);
uint16_t pmm_page_refcount(uint64_t phys);
/* Implementation: static uint16_t refcount[MAX_PHYS_PAGES] in pmm.c */
/* For 1 GB: 262144 pages × 2 bytes = 512 KB array. Acceptable. */
```

**Page fault handler** — register in `idt.c` (coordinate with user):
```c
/* INT14 = Page Fault */
isr_register_handler(14, page_fault_handler);

/* page_fault_handler in mm/vmm.c or mm/fault.c: */
void page_fault_handler(registers_t *regs) {
    uint64_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    bool write = (regs->error_code >> 1) & 1;
    bool user  = (regs->error_code >> 2) & 1;

    if (!user) kpanic("Kernel page fault (kernel bug)");

    /* COW: write fault on read-only page? */
    if (write && pmm_page_refcount(vmm_get_phys(cr2)) > 1) {
        cow_handle(current_process, cr2);
        return;
    }
    /* Stack growth: write below current stack bottom */
    if (cr2 >= current_process->stack_limit_virt - PROC_USER_STACK_MAX
        && cr2 < current_process->stack_limit_virt) {
        uint64_t new_page = cr2 & ~0xFFFULL;
        uint64_t phys = pmm_alloc_page();
        vmm_map_page(new_page, phys, PAGE_PRESENT|PAGE_WRITABLE|PAGE_USER);
        current_process->stack_limit_virt = new_page;
        return;
    }
    /* Heap demand: within brk range */
    if (cr2 >= current_process->heap_base && cr2 < current_process->heap_end) {
        uint64_t phys = pmm_alloc_page();
        vmm_map_page(cr2 & ~0xFFFULL, phys, PAGE_PRESENT|PAGE_WRITABLE|PAGE_USER);
        memset((void *)(phys + hhdm_offset), 0, 4096);
        return;
    }
    /* Segfault: kill process */
    debug_log(DEBUG_LEVEL_WARN, "FAULT", "SIGSEGV pid=%d addr=%p",
              current_process->pid, (void*)cr2);
    process_exit(current_process, 139);
}
```

### Memory Budget (document in scheduler.md):

| Allocation | Size | When | Freed |
|---|---|---|---|
| PML4 (per process) | 4 KB | process_create | process_destroy |
| PCB slab | ~512 B avg | process_create | process_destroy |
| TCB slab | ~256 B avg | thread_create | thread_destroy |
| Kernel stack (per thread) | 4 KB + 4 KB guard virtual | thread_create | thread_destroy |
| PMM refcount array | 512 KB static | boot | never |
| Scheduler run queues | 8 × pointer (64 B) | boot | never |
| **Total per process** | **≤ 8 KB physical** | | |
| **Total per thread** | **≤ 5 KB physical** | | |

---

## 14. Documentation Required (write LAST)

Create `docs/phase4/scheduler.md`:
- Architecture overview
- `process_t` fields explained
- `thread_t` fields + kernel stack layout diagram
- `switch_context` register-by-register walkthrough
- Scheduler algorithm (queues, preemption, idle)
- Config keys and their effects
- TSS RSP0 update flow
- Future: SMP (per-CPU run queues), MLFQ, real-time threads
- Known limitations

---

## 14. Starting Agent Prompt

```
You are a Kernel Agent implementing NexusOS Phase 4 Task 4.1 and 4.2.

READ FIRST (mandatory, in order):
1. planned_implementations/AGENT_A_SCHEDULER.md (this document — full spec)
2. AGENTS.md
3. knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md
4. knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md
5. knowledge_items/nexusos-progress-report/artifacts/progress_report.md
6. kernel/src/kernel.c (init sequence, DO NOT MODIFY)
7. kernel/src/hal/gdt.c + gdt.h (TSS — you will add gdt_set_kernel_stack)
8. kernel/src/drivers/timer/pit.c + pit.h
9. kernel/src/hal/idt.c + isr.h (isr_register_handler)
10. kernel/src/mm/vmm.h, heap.h, pmm.h

YOUR TASK:
Implement the NexusOS process/thread model and preemptive round-robin scheduler
as specified in planned_implementations/AGENT_A_SCHEDULER.md.

Create these files (spec in AGENT_A_SCHEDULER.md):
- kernel/src/config/nexus_config.h
- kernel/src/sched/process.h + process.c
- kernel/src/sched/thread.h + thread.c
- kernel/src/sched/scheduler.h + scheduler.c
- kernel/src/arch/x86_64/context.asm (context switch)
- kernel/src/hal/gdt.h (add gdt_set_kernel_stack declaration)
- kernel/src/hal/gdt.c (implement gdt_set_kernel_stack)

RULES:
- Build must pass 0 errors, 0 C warnings after every file you create
- Use kmalloc/kfree only. No standard library.
- All hardware structs: __attribute__((packed))
- All MMIO: volatile pointers
- 4-space indent, snake_case, UPPER_CASE macros, _t suffix typedefs
- Every file: block comment header (filename, purpose, author)
- Every public function: brief doc comment
- Use LODBUG debugger (debug_log) for scheduler events
- DO NOT modify: kernel.c, hal/idt.c, hal/pic.c, mm/*.c
- DO NOT start kernel.c integration — Agent B handles syscall, user asks for sequence update

VERIFICATION:
Create two test kernel threads in a test function. Hook the timer IRQ.
Verify interleaved execution via serial debug output.
Build: 0 errors, 0 warnings.

WHEN DONE:
1. Write docs/phase4/scheduler.md
2. Report: files created, build status, test results
3. Signal: "AGENT A COMPLETE — process.h and thread.h are final and stable"
   (Agent B depends on these headers)
```
