# NexusOS — Memory Efficiency Mandate (All Phases)

> **Authority**: Senior Systems Architect (Coordinator)  
> **Applies to**: All agents, all phases, all future development  
> **Requirement**: Full OS functional on 1 GB RAM with headroom to spare  
> **Last Updated**: 2026-04-17

---

## The 1 GB Budget Model

Target RAM budget breakdown for a fully running NexusOS:

| Component | Budget | Notes |
|---|---|---|
| Kernel code + data (read-only after init) | ≤ 4 MB | Text + BSS + rodata |
| Kernel heap (runtime) | ≤ 16 MB | kmalloc pool; grows on demand |
| PMM bitmap (1 GB RAM = 262144 pages) | 32 KB | 1 bit/page |
| Page tables (kernel space) | ≤ 2 MB | PML4 + 256 PDs mapped |
| Per-thread kernel stack × 64 threads | 512 KB | 8 KB × 64 |
| Scheduler data structures | ≤ 64 KB | PCBs + TCBs |
| VFS + ramfs (in-memory FS) | ≤ 8 MB | configurable |
| USB rings + DMA buffers | ≤ 256 KB | 4 rings × 4 KB |
| Filesystem driver caches | ≤ 4 MB | FAT32/ext4 block cache |
| User processes (total) | ≤ 64 MB | demand-paged + COW-shared |
| **TOTAL KERNEL OVERHEAD** | **≤ 35 MB** | |
| **USER HEADROOM in 1 GB** | **≥ 965 MB** | |

**Conclusion**: All features functional under 40 MB kernel footprint. User applications get ~965 MB.

---

## Core Memory Efficiency Principles

These rules are MANDATORY for all agents, all phases, forever:

### P1 — Lazy / Demand Allocation

**Rule**: Never allocate memory until it is actually needed.

- **User stacks**: Do NOT pre-allocate 8 MB. Map a single 4 KB guard page + 4 KB initial page. Grow on page fault.
- **User heap**: `brk()` extends virtual range only. Physical pages mapped on first write fault.
- **Kernel heap**: `heap_init(8)` — 8 pages initial (32 KB). Expands on demand via `pmm_alloc_pages`.
- **Process address space**: `process_create()` allocates PML4 (1 page = 4 KB). Copy kernel entries only. No user mappings until ELF load.
- **ELF segments**: Map virtual range on load; physical pages allocated on page fault (NOT pre-faulted).
- **Terminal scrollback buffer**: Allocate on first scroll, not at terminal_init time.
- **DMA rings**: Allocate only when device found. Deallocate on device disconnect.

### P2 — Slab Allocator for Hot Objects

**Rule**: Never use raw `kmalloc` for frequently allocated/freed fixed-size objects.

Add a slab allocator layer (`kernel/src/mm/slab.h`, `slab.c`):

```c
/* Slab cache — O(1) alloc/free for fixed-size objects */
typedef struct slab_cache slab_cache_t;

slab_cache_t *slab_cache_create(const char *name, uint64_t obj_size,
                                 uint64_t align, uint64_t objs_per_slab);
void         *slab_cache_alloc(slab_cache_t *cache);
void          slab_cache_free(slab_cache_t *cache, void *obj);
void          slab_cache_destroy(slab_cache_t *cache);
```

**Use slab for**:
- `process_t` allocations (fixed size, frequently created/destroyed)
- `thread_t` allocations (same reason)
- `vfs_node_t` allocations (many small FS nodes)
- Page table entries (when dynamically allocated)
- USB device descriptors

**Implementation**: Each slab = 1 physical page. Objects packed tightly. Free bitmap per slab. Cache holds N full slabs + 1 partial. Zero fragmentation overhead.

**Memory saved**: Raw `kmalloc` wastes up to 50% due to alignment + header overhead. Slab wastes < 5%.

### P3 — Copy-on-Write (COW) Fork

**Rule**: `sys_fork()` must use COW. Never deep-copy address space.

**Implementation**:
1. `fork()`: Clone page tables (PML4 → PDPT → PD → PT). Copy PT leaf entries.
2. Mark ALL writable user pages read-only in BOTH parent and child PTs.
3. Store original flags in a `cow_page_t` shadow table per process.
4. On write page fault (`#PF` with error code bit 1 = write, bit 2 = user):
   a. Check if page is COW (lookup shadow table)
   b. `ref_count[phys_page]--`
   c. If ref_count == 1: just make writable again (no copy needed, only reference)
   d. If ref_count > 1: allocate new page, copy content, update PT, restore writable
5. On process exit: `ref_count[phys_page]--` for all pages; free when ref_count == 0

**Data structures**:
```c
/* Per-physical-page reference counter (in mm/pmm.c) */
/* Array indexed by page_number = phys / 4096 */
static uint16_t page_refcount[MAX_PAGES];  /* 2 bytes × 262144 = 512 KB */
/* MAX_PAGES for 1 GB: 262144 pages. 512 KB refcount array. Acceptable. */

/* PMM API additions */
void     pmm_page_ref(uint64_t phys);   /* increment refcount */
void     pmm_page_unref(uint64_t phys); /* decrement; free if 0 */
uint16_t pmm_page_refcount(uint64_t phys);
```

**Memory saved at fork()**: A 64 MB process forks → no physical memory used until writes occur. Shell + child share code pages forever (code is read-only, never needs copy).

### P4 — Demand Paging with Page Fault Handler

**Rule**: The page fault handler (`#PF = INT14`) must handle:
1. **COW faults** (write to COW page) → allocate + copy
2. **Stack growth faults** (write below current stack limit) → map new stack page
3. **Heap demand faults** (access to brk-extended range) → map new anon page
4. **ELF segment faults** (access to mapped but not loaded segment) → map page, zero

**Add to `kernel/src/hal/isr.h` handler registration**:
```c
/* INT14 = Page Fault. Must be registered in idt.c */
/* Error code layout:
 *   bit 0 = P (0=not present, 1=protection violation)
 *   bit 1 = W (0=read, 1=write)
 *   bit 2 = U (0=kernel, 1=user)
 *   bit 3 = RSVD
 *   bit 4 = I (instruction fetch)
 ** CR2 = faulting virtual address */
```

Implement `page_fault_handler()` in `kernel/src/mm/vmm.c` (or `fault.c`):
```c
void page_fault_handler(registers_t *regs) {
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
    uint32_t error = regs->error_code;
    bool write = (error >> 1) & 1;
    bool user  = (error >> 2) & 1;

    if (!user) kpanic("Kernel page fault"); /* kernel faults are bugs */

    process_t *proc = current_process;

    /* 1. COW fault */
    if (write && is_cow_page(proc, fault_addr)) {
        cow_handle_fault(proc, fault_addr);
        return;
    }
    /* 2. Stack growth */
    if (fault_addr >= proc->stack_limit_virt - STACK_GROW_LIMIT &&
        fault_addr < proc->stack_top_virt) {
        map_user_page(proc, fault_addr & ~0xFFF, PAGE_PRESENT|PAGE_WRITABLE|PAGE_USER);
        proc->stack_limit_virt = fault_addr & ~0xFFF;
        return;
    }
    /* 3. Anonymous heap */
    if (fault_addr >= proc->heap_base && fault_addr < proc->heap_end) {
        map_user_page(proc, fault_addr & ~0xFFF, PAGE_PRESENT|PAGE_WRITABLE|PAGE_USER);
        return;
    }
    /* 4. Unmapped → SIGSEGV (Phase 5) or kill for now */
    process_exit(proc, 139);  /* 128 + SIGSEGV(11) */
}
```

### P5 — Kernel Stack per Thread: 4 KB, Not 8 KB

**Rule**: Default to 4 KB kernel stacks. Allow 8 KB only for threads that need deep call chains.

- 4 KB = 1 page = sufficient for 5–8 levels of function calls
- 64 threads × 4 KB = 256 KB (vs 512 KB with 8 KB stacks)
- IST stacks (double fault, NMI) stay at 4 KB (pre-existing in TSS)

Update in `nexus_config.h`:
```c
#define THREAD_KERNEL_STACK_SIZE  4096  /* 4 KB — 1 page */
#define THREAD_KERNEL_STACK_SIZE_LARGE 8192  /* for deep-call threads only */
```

Stack overflow detection: Place a guard page (unmapped) below the kernel stack.
On kernel stack overflow → page fault → kpanic with "kernel stack overflow".

### P6 — Minimal PCB / TCB Footprint

**Rule**: `process_t` ≤ 512 bytes. `thread_t` ≤ 256 bytes.

Achieve by:
- FD table: NOT inline array. Use a separate kmalloc'd structure ONLY for processes with open files.
  ```c
  typedef struct fd_table {
      void *fds[64];     /* vfs_node_t* */
      uint32_t open_count;
  } fd_table_t;
  /* In process_t: */
  fd_table_t *fd_table;  /* NULL until first open() call */
  ```
- Signal table: NOT inline. Allocate on first `sigaction()` call.
- Environment: NOT inline. Pointer to separate page.
- CWD: NOT inline. Pointer to a shared interned string. Most processes share `/`.

### P7 — Shared Kernel Mappings (No Redundant Page Tables)

**Rule**: All processes share kernel PML4 entries (indices 256–511). Never duplicate.

- At boot: kernel maps itself. Those PML4 entries are gold.
- `process_create()`: Copy ONLY entries 256–511 from kernel's PML4 into new process PML4.
- Entries 0–255: user space — child-specific, start empty.
- On kernel vmm_map_page for kernel addresses: must also update ALL process PML4s.
  Simplest: maintain a global list of all PML4 physical addresses; on each kernel mapping, update all.
  OR: use a single shared kernel PDPT/PD for kernel entries (all PML4s point to same PDPT).

**Recommended**: "Recursive shared kernel page table" — one PDPT for upper half, shared via pointer. Cost: 1 PDPT page (4 KB) shared among unlimited processes. Zero duplication.

### P8 — USB DMA Rings: Minimum Viable Size

**Rule**: Use minimum functional ring sizes. Grow only under load.

```c
/* In nexus_config.h — reduced from previous spec: */
#define XHCI_CMD_RING_SIZE        64    /* 64 × 16B = 1 KB (was 256 TRBs) */
#define XHCI_EVENT_RING_SIZE      64    /* 64 × 16B = 1 KB */
#define XHCI_TRANSFER_RING_SIZE   16    /* 16 × 16B = 256 B per endpoint */
/* Total USB DMA per controller: ~10 KB (was ~16 KB) */
/* Rings share one 4 KB page each due to alignment requirements */
```

**Pack multiple rings into one allocated page** where possible:
- Command ring (1 KB) + Event ring (1 KB) → 1 shared page (4 KB, 2 KB used)
- Per-endpoint transfer ring (256 B) → pack 16 per page

### P9 — libc Static Size Optimization

**Rule**: Userspace binaries must be link-time-optimized for size.

Add to `userspace/libc/GNUmakefile`:
```makefile
CFLAGS += -Os                          # Optimize for size
CFLAGS += -ffunction-sections          # One section per function
CFLAGS += -fdata-sections              # One section per variable
LDFLAGS += --gc-sections               # Remove unused functions/data
LDFLAGS += -s                          # Strip symbol table from final binary
```

**musl inspiration**: libc headers structured one-function-per-object-file. The linker only pulls in code actually called. Target: `init` binary ≤ 50 KB, `nsh` shell ≤ 100 KB.

**Do NOT** implement unused libc functions just to be complete. Every function added must be called by something. Dead code is dead RAM.

### P10 — Kernel Heap: Slab + Boundary Tag Allocator

**Rule**: Replace current free-list heap with a two-tier allocator.

**Tier 1 — Slab allocator** (for objects ≤ 256 bytes):
- Manages caches for sizes 8, 16, 32, 64, 128, 256 bytes
- O(1) alloc/free, zero fragmentation
- Each cache backed by power-of-2 number of pages

**Tier 2 — Boundary tag allocator** (for objects 257 B – 2 MB):
- Classic malloc with coalescing (already partially implemented in `heap.c`)
- Enhance with: immediate coalescing on free, boundary tags for O(1) coalesce

**Tier 3 — Page allocator** (for objects > 2 MB):
- Direct `pmm_alloc_pages()` + `vmm_map_page()`, returned as virtual

**Memory saved**: A naive `kmalloc(24)` wastes 40 bytes of alignment in current heap. Slab wastes 0.

### P11 — Filesystem Block Cache: Bounded LRU

**Rule**: Block cache for disk filesystems is bounded, not unlimited.

```c
/* In nexus_config.h */
#define FS_BLOCK_CACHE_MAX_PAGES  512    /* 512 × 4 KB = 2 MB max cache */
#define FS_BLOCK_CACHE_POLICY     LRU    /* Evict least recently used */
```

Without a bound, a large disk read can fill all RAM with cached blocks.
With 2 MB cap on 1 GB system: cache is effective for small workloads, bounded for large ones.

Implement as a fixed-size hash table + LRU doubly-linked list:
- Hash by (device_id, block_number) → O(1) lookup
- On miss: evict LRU entry if cache full, load new block
- On hit: move to MRU position

### P12 — No Memory Leaks Policy

**Rule**: Every allocation has an owner. Owner frees on exit/error.

- Every `kmalloc` → paired with `kfree` in the same module
- Every `pmm_alloc_page` → tracked in process `mm_region_t` list; freed on `process_destroy`
- Page tables: on process exit, walk all PT levels and free physical pages
- VFS nodes: reference-counted; freed when refcount reaches 0
- DMA buffers: freed in `xhci_deinit()` or device disconnect handler

Add `mm_debug_check_leaks()` to LODBUG console as a command: `nxdbg> leaks`

### P13 — Kernel BSS and Heap Initialization

**Rule**: BSS is zeroed by the linker script. Heap starts minimal. Neither wastes RAM.

Current `heap_init(64)` (64 pages = 256 KB) → reduce to `heap_init(8)` (8 pages = 32 KB).
The heap expands automatically when needed. Initial size only matters for boot:
- GDT/IDT/PMM/VMM don't use heap (they use BSS or static storage)
- First heap use is `ramfs_init()` — only needs a few dozen KB

### P14 — procfs: Virtual, Zero Physical Cost

**Rule**: `/proc` files must generate content on-read only. Never cache.

Every read to `/proc/N/status` recomputes from current PCB. No static storage.
Cost: 0 bytes of RAM for procfs file content. Only the directory tree nodes exist.

---

## Memory Budget Accounting Rule (Enforced)

Every agent must add to their documentation (`docs/phase4/<subsystem>.md`):

### Memory Budget Section
```markdown
## Memory Usage Budget

| Allocation | Size | When | Freed When |
|---|---|---|---|
| slab_cache process_t | 1 page (4KB) per 16 processes | process_create | process_destroy |
| ... | ... | ... | ... |

Total worst-case: X KB static + Y KB per process
```

---

## Updated nexus_config.h (Efficiency-Tuned Defaults)

```c
/* ── Scheduler ─────────────────────────────────────────────────────────────── */
#define SCHED_QUANTUM_MS           10
#define SCHED_MAX_PROCESSES        256    /* 256 PCBs = 256 × 512B = 128 KB */
#define SCHED_MAX_THREADS_PER_PROC 8      /* reduced from 16 */
#define THREAD_KERNEL_STACK_SIZE   4096   /* 4 KB (was 8 KB) */
#define THREAD_KERNEL_STACK_GUARD  1      /* 1=enable guard page below stack */
#define PROC_USER_STACK_INITIAL    4096   /* 4 KB initial — grows on #PF */
#define PROC_USER_STACK_MAX        (8 * 1024 * 1024)   /* 8 MB virtual limit */

/* ── Memory ────────────────────────────────────────────────────────────────── */
#define HEAP_INITIAL_PAGES         8      /* 32 KB initial (was 64 pages = 256 KB) */
#define HEAP_GROW_PAGES            8      /* Grow by 32 KB at a time */
#define FS_BLOCK_CACHE_MAX_PAGES   512    /* 2 MB block cache cap */
#define PMM_REFCOUNT_ENABLE        1      /* Enable COW reference counting */

/* ── USB ───────────────────────────────────────────────────────────────────── */
#define XHCI_CMD_RING_SIZE         64     /* 1 KB */
#define XHCI_EVENT_RING_SIZE       64     /* 1 KB */
#define XHCI_TRANSFER_RING_SIZE    16     /* 256 B per endpoint */
#define USB_MAX_DEVICES            32     /* was 64 */

/* ── libc ──────────────────────────────────────────────────────────────────── */
#define LIBC_MALLOC_INITIAL_HEAP   4096   /* 4 KB initial brk */
#define LIBC_STDIO_BUFFER_SIZE     1024   /* Buffered I/O per FILE* */

/* ── Filesystem ────────────────────────────────────────────────────────────── */
#define VFS_MAX_OPEN_FILES         256    /* global FD limit */
#define RAMFS_MAX_NODES            512    /* in-memory FS nodes */
```

---

## Compiler Flags for Size + Speed

Add to `kernel/GNUmakefile` (for release builds):
```makefile
# Size-optimized release build flags (in addition to existing flags)
CFLAGS += -Os                # Optimize for size (not -O2 which may bloat)
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
LDFLAGS += --gc-sections     # Already present; ensures dead code stripped
```

For userspace (`userspace/libc/GNUmakefile`):
```makefile
CFLAGS += -Os -ffunction-sections -fdata-sections
LDFLAGS += --gc-sections -s  # -s strips debug symbols from binaries
```

**Note**: `-Os` vs `-O2`: `-Os` applies all `-O2` optimizations except those that increase code size. For a kernel aiming for minimal footprint, `-Os` is correct.

---

## Future Development Requirements

All future agents (Phase 5, Phase 6, etc.) MUST:

1. **Budget before building**: Write a memory budget table before implementing anything.
2. **Lazy by default**: No pre-allocation without documented justification.
3. **Slab for objects < 256B**: Use `slab_cache_alloc`, never raw `kmalloc` for frequently allocated fixed-size objects.
4. **Bound all caches**: Every cache/pool must have a configured maximum size.
5. **No static arrays for variable data**: `static vfs_node_t nodes[1024]` is forbidden. Use dynamic allocation.
6. **musl philosophy for libc**: One function per translation unit. Dead code eliminated at link time.
7. **COW for all process clones**: Never memcpy an address space.
8. **Prove it**: Every phase ends with a `free` command output showing total memory consumption at idle.

---

## Expected Memory at Boot (Target)

After full Phase 4 with OS idle (init + shell running):

```
$ free
              total        used        free
Mem:       1048576 KB    35840 KB   1012736 KB
Swap:            0 KB        0 KB         0 KB
```

- Kernel: ~20 MB  
- init + nsh: ~2 MB (shared code pages, COW data)  
- Buffers/cache: ~8 MB (filesystem blocks)  
- **Free: ~970 MB**
