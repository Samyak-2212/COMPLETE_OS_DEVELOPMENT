/* ============================================================================
 * nexus_config.h — NexusOS Kernel Configuration Constants
 * Purpose: All tunable compile-time parameters. Edit this file to change
 *          kernel behavior without modifying source code. Every constant
 *          has a comment explaining valid range and effect.
 * Author: NexusOS Kernel Team
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

/* Kernel stack size per thread. 4 KB = 1 page.
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
 * EFFICIENCY: Expands automatically on demand. */
#define HEAP_INITIAL_PAGES         8
#define HEAP_GROW_PAGES            8

/* ── Memory ────────────────────────────────────────────────────────────────── */

/* User-space virtual address space lower bound */
#define PROC_USER_VIRT_BASE       0x0000000000400000ULL

/* User-space virtual address space upper bound */
#define PROC_USER_VIRT_END        0x00007FFFFFFFFFFFULL

/* Enable COW (copy-on-write) page sharing for fork().
 * EFFICIENCY: fork() copies page table entries only, not physical pages. */
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

/* Number of priority levels */
#define SCHED_NUM_PRIORITIES      8

/* ── Filesystem / Cache ────────────────────────────────────────────────────── */
/* Block cache page cap. 512 × 4 KB = 2 MB max. Prevents RAM exhaustion. */
#define FS_BLOCK_CACHE_MAX_PAGES   512

/* ── USB ───────────────────────────────────────────────────────────────────── */
#define XHCI_CMD_RING_SIZE         64     /* 64 × 16B = 1 KB */
#define XHCI_EVENT_RING_SIZE       64     /* 64 × 16B = 1 KB */
#define XHCI_TRANSFER_RING_SIZE    16     /* 16 × 16B = 256 B per endpoint */
#define USB_MAX_DEVICES            32

#endif /* NEXUS_CONFIG_H */
