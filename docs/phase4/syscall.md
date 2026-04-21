# Phase 4: Syscall & User-Mode Transition

> **Author**: NexusOS Agent B / Coordinator Review (2026-04-21)
> **Phase**: 4 — Multitasking, USB, Userspace
> **Files**: `kernel/src/syscall/`, `kernel/src/arch/x86_64/syscall_entry.asm`

---

## Overview

NexusOS implements the Linux x86_64 system call ABI using the `SYSCALL`/`SYSRET` mechanism
via `IA32_LSTAR` MSR. Userspace ELF binaries compiled for Linux can invoke syscalls directly
without modification to their calling code.

---

## Entry Point: `syscall_entry.asm`

**File**: `kernel/src/arch/x86_64/syscall_entry.asm`

### Flow

```
syscall instruction (Ring 3)
  → CPU saves user RIP → RCX, RFLAGS → R11
  → CPU loads CS/SS from IA32_STAR, jumps to IA32_LSTAR
  → IA32_FMASK masks IF (interrupts off until kernel stack loaded)
  → syscall_entry: swap to kernel RSP (from kernel_rsp_scratch)
  → push user context frame on kernel stack
  → STI (re-enable interrupts — nested IRQs allowed during syscall)
  → remap Linux ABI regs → SysV C calling convention
  → call syscall_dispatch(num, a1, a2, a3, a4, a5, a6)
  → RAX = return value
  → CLI, restore regs, restore user RSP
  → o64 sysret → Ring 3 (RIP=RCX, RFLAGS=R11, CS/SS from STAR)
```

### Kernel Stack Frame Layout (on entry, top → bottom)

```
[rsp+0]  user RSP          (saved to kernel_rsp_scratch scratch on exit)
[rsp+8]  r15
[rsp+16] r14
[rsp+24] r13
[rsp+32] r12
[rsp+40] rbx
[rsp+48] rbp
[rsp+56] r11   (user RFLAGS — restored into R11 before sysretq)
[rsp+64] rcx   (user RIP   — restored into RCX before sysretq)
```

### Linux ABI → SysV C ABI Remapping

| Linux register | Holds | Mapped to SysV C arg |
|---|---|---|
| RAX | syscall number | RDI (1st C arg) |
| RDI | a1 | RSI (2nd) |
| RSI | a2 | RDX (3rd) |
| RDX | a3 | RCX (4th) |
| R10 | a4 | R8  (5th) — **R10, not RCX** |
| R8  | a5 | R9  (6th) |
| R9  | a6 | pushed on stack (7th) |

> **Critical for userspace libc**: The 4th argument uses `r10`, not `rcx`.
> `rcx` is clobbered by the `syscall` instruction (CPU saves user RIP there).

### RSP Scratch Variables (`.data` section)

```nasm
user_rsp_scratch:   dq 0   ; saves user RSP during syscall handling
kernel_rsp_scratch: dq 0   ; updated by scheduler on every context switch
```

All references use `default rel` (RIP-relative) to avoid 32-bit section-crossing
relocation warnings in ELF64 position-dependent code.

---

## MSR Configuration (`syscall.c`)

Called once from `syscall_init()` during boot:

| MSR | Address | Value | Effect |
|---|---|---|---|
| `IA32_EFER` | `0xC0000080` | bit 0 = 1 | Enable `SCE` (System Call Enable) |
| `IA32_STAR` | `0xC0000081` | `0x0013_0008_0000_0000` | SYSCALL CS=0x08, SYSRET SS/CS base=0x13 |
| `IA32_LSTAR` | `0xC0000082` | `&syscall_entry` | Entry point (64-bit) |
| `IA32_FMASK` | `0xC0000084` | `0x200` (bit 9) | Mask IF on entry |

**STAR breakdown:**
- `[47:32] = 0x0008` → SYSCALL: CS = 0x08 (kernel code), SS = 0x10 (kernel data)
- `[63:48] = 0x0013` → SYSRET: SS = 0x13+8 = **0x1B** (user data | RPL3), CS = 0x13+16 = **0x23** (user code | RPL3)

---

## Syscall Dispatch (`syscall_table.c`)

`syscall_dispatch(uint64_t num, ...)` dispatches via a function-pointer table indexed
by Linux x86_64 syscall number. Out-of-range numbers return `-ENOSYS`.

Signature of every handler:
```c
int64_t sys_xxx(uint64_t a1, uint64_t a2, uint64_t a3,
                uint64_t a4, uint64_t a5, uint64_t a6);
```

---

## Stub Suppression Pattern (`syscall_handlers.c`)

```c
#define UNUSED_ARGS6(a,b,c,d,e,f) \
    (void)(a);(void)(b);(void)(c);(void)(d);(void)(e);(void)(f)

int64_t sys_stub(uint64_t a1, uint64_t a2, uint64_t a3,
                 uint64_t a4, uint64_t a5, uint64_t a6) {
    UNUSED_ARGS6(a1,a2,a3,a4,a5,a6);
    return -ENOSYS;
}
```

Required to achieve 0 C warnings under `-Wunused-parameter` at all optimization levels.

---

## File Descriptor Table

The FD table is **lazily allocated** on first `open()` call:

```c
typedef struct { void *fds[64]; int open_count; } fd_table_t;
```

- FDs 0, 1, 2 (stdin/stdout/stderr) are pre-wired by `init_process.c` to devfs nodes.
- FDs 3–63 are assigned by `sys_open()` in first-free-slot order.
- `fork()` copies the entire `fd_table_t` (shallow copy — VFS nodes are shared).

> **Known limitation**: `read`/`write` use offset=0 always. `lseek` is a stub.
> Per-FD seek tracking is deferred to Phase 5.

---

## Complete Syscall Table (Phase 4)

| Number | Name | Implementation | Notes |
|---|---|---|---|
| 0 | `read` | ✅ VFS-backed | offset hardcoded to 0 |
| 1 | `write` | ✅ VFS-backed | offset hardcoded to 0 |
| 2 | `open` | ✅ VFS + lazy FD table | `O_CREAT` supported |
| 3 | `close` | ✅ | decrements `open_count` |
| 4 | `stat` | Stub → `-ENOSYS` | Phase 5 |
| 5 | `fstat` | Stub → `-ENOSYS` | Phase 5 |
| 8 | `lseek` | Stub → `-ENOSYS` | Phase 5 |
| 9 | `mmap` | ✅ `MAP_ANONYMOUS` only | advances `heap_end` |
| 10 | `mprotect` | Stub → `0` | safe no-op |
| 11 | `munmap` | Stub → `0` | safe no-op |
| 12 | `brk` | ✅ | `brk(0)` = query; page-aligns new brk |
| 13 | `rt_sigaction` | Stub → `0` | safe no-op for libc init |
| 14 | `rt_sigprocmask` | Stub → `0` | safe no-op for libc init |
| 16 | `ioctl` | Stub → `-ENOSYS` | |
| 21 | `dup` | Stub → `-ENOSYS` | Phase 5 |
| 32 | `dup2` | Stub → `-ENOSYS` | Phase 5 |
| 33 | `pause` / `pipe` | Stub → `-ENOSYS` | Phase 5 |
| 39 | `getpid` | ✅ | reads `current_process->pid` |
| 57 | `fork` | ✅ Partial | COW clone done; child thread frame deferred Phase 5 |
| 59 | `execve` | Partial | ELF loaded; context replacement deferred Phase 5 |
| 60 | `exit` | ✅ | calls `scheduler_exit_current()` |
| 61 | `wait4` | ✅ | blocks with `schedule()`, supports `WNOHANG` |
| 62 | `waitid` | Stub → `-ENOSYS` | |
| 63 | `uname` | ✅ | fills `struct utsname` with NexusOS info |
| 79 | `getcwd` | ✅ | copies `current_process->cwd` |
| 80 | `chdir` | Stub → `-ENOSYS` | Phase 5 |
| 83 | `mkdir` | Stub → `-ENOSYS` | Phase 5 |
| 84 | `rmdir` | Stub → `-ENOSYS` | Phase 5 |
| 87 | `unlink` | Stub → `-ENOSYS` | Phase 5 |
| 102 | `getuid` | ✅ | reads `current_process->uid` |
| 104 | `getgid` | ✅ | reads `current_process->gid` |
| 107 | `geteuid` | ✅ | reads `current_process->euid` |
| 108 | `getegid` | ✅ | reads `current_process->egid` |
| 110 | `getppid` | ✅ | reads `current_process->ppid` |
| 158 | `arch_prctl` | ✅ `ARCH_SET_FS` | writes `IA32_FS_BASE` MSR (0xC0000100) |
| 186 | `gettid` | ✅ | reads `current_thread->tid` |
| 217 | `getdents64` | Stub → `-ENOSYS` | Phase 5 |
| 218 | `set_tid_address` | ✅ | returns `current_thread->tid` |
| 228 | `clock_gettime` | Stub → `-ENOSYS` | Phase 5 |
| 231 | `exit_group` | ✅ | same as `exit` — `scheduler_exit_current()` |
| 234 | `tgkill` | Stub → `-ENOSYS` | Phase 5 signal delivery |
| 257 | `openat` | Stub → `-ENOSYS` | Phase 5 |

---

## ELF Loader (`elf_loader.c`)

- Validates ELF64 magic, machine type (`EM_X86_64 = 0x3E`)
- Iterates `PT_LOAD` segments:
  - Allocates physical pages only for non-BSS data (file-backed)
  - Registers each segment as a VMA via `process_add_vma()`
  - BSS pages are **not pre-allocated** — faulted in by the `#PF` handler on first access
- Sets `proc->heap_base` = first page-aligned address past all loaded segments
- Returns entry point (`e_entry`) via out-param

---

## Init Process (`init_process.c`)

Creates PID 1 from a Limine kernel module:

1. `process_create(0)` → fresh PCB, new PML4 (kernel upper-half copied)
2. ELF module located via `limine_get_modules()` response
3. `elf_load(proc, module_data)` → maps segments, sets `heap_base`
4. Lazy FD table: stdin(0)/stdout(1)/stderr(2) wired to `/dev/tty0` devfs node
5. `thread_create(proc, entry_rip, 0, 0)` → kernel stack pre-populated with `iretq` frame
6. Thread enqueued at `SCHED_PRIORITY_NORMAL`; first `schedule()` IRETQ's to Ring 3

---

## Phase 5 Deferred Work

| Item | Reason deferred |
|---|---|
| `execve` context replacement | Needs overwriting saved RIP in syscall frame — requires scheduler cooperation |
| `fork` child thread frame | `thread_create_fork_child()` not yet implemented |
| `lseek` / per-FD seek | Needs seek offset field added to FD table entry |
| `dup` / `pipe` | Needs reference-counted VFS node sharing |
| Signal delivery | Needs sigaction table in PCB, signal injection on return to Ring 3 |
| `stat` / `fstat` | Needs `vfs_stat_t` populated by each FS driver |
