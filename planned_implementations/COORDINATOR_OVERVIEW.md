# NexusOS Phase 4 — Coordinator Overview

> **Coordinator**: Senior Systems Architect  
> **Phase**: 4 — Multitasking, USB, Userspace  
> **Date**: 2026-04-17  
> **Status**: PLANNING

---

## Phase 4 Goals (Expanded from task.md)

| # | Goal | Agent |
|---|---|---|
| 4.1 | Process + Thread model | **Kernel Agent A** |
| 4.2 | Context switch + Scheduler | **Kernel Agent A** |
| 4.3 | System calls (Linux ABI) | **Kernel Agent B** |
| 4.4 | User-mode transition | **Kernel Agent B** |
| 4.5 | Minimal libc (expandable to glibc) | **App Agent** |
| 4.6 | Init process + userspace shell | **App Agent** |
| 4.7 | xHCI host controller | **Driver Agent** |
| 4.8 | USB enumeration | **Driver Agent** |
| 4.9 | USB HID driver | **Driver Agent** |

---

## Agent Assignments & Files

```
Kernel Agent A  → kernel/src/sched/
                  kernel/src/arch/x86_64/context.asm
                  kernel/src/mm/vmm.c (process address space)

Kernel Agent B  → kernel/src/syscall/
                  kernel/src/arch/x86_64/syscall_entry.asm
                  kernel/src/sched/process.c (exec/fork handlers)
                  kernel/src/fs/procfs.c (optional, /proc)

App Agent       → userspace/libc/
                  userspace/init/
                  userspace/shell/
                  userspace/GNUmakefile

Driver Agent    → kernel/src/drivers/usb/
```

---

## Zero-Collision Guarantee

- **Kernel Agent A** owns `sched/` and `arch/x86_64/context.asm` exclusively.
- **Kernel Agent B** owns `syscall/` and `arch/x86_64/syscall_entry.asm` exclusively.
  - Kernel Agent B depends on Agent A completing `process_t` + `thread_t` structs first.
  - Agent B must read `kernel/src/sched/process.h` before coding.
- **App Agent** owns entire `userspace/` tree exclusively. Zero kernel source touches.
- **Driver Agent** owns `kernel/src/drivers/usb/` exclusively.
  - Reads PCI/AHCI code for reference only — does NOT modify it.

---

## Sequencing & Dependencies

```
Phase 3 (DONE)
    │
    ├── Kernel Agent A [FIRST] ──────────────────────────────────┐
    │   Tasks: 4.1, 4.2                                          │
    │   Output: process.h, thread.h, scheduler.h, context.asm   │
    │                                                            │
    ├── Driver Agent [PARALLEL with A] ──────────────────────────┤
    │   Tasks: 4.7, 4.8, 4.9                                     │
    │   No dependency on A or B                                  │
    │                                                            ▼
    ├── Kernel Agent B [AFTER A completes process.h] ────────────┐
    │   Tasks: 4.3, 4.4                                          │
    │   Output: syscall.h, syscall_table.c, syscall_entry.asm    │
    │                                                            │
    └── App Agent [AFTER B provides syscall.h] ──────────────────┘
        Tasks: 4.5, 4.6
        Output: libc/, init, shell
```

---

## Critical Design Mandates

### Linux ABI Compatibility
- Syscall numbers MUST match Linux x86_64 exactly:
  - read=0, write=1, open=2, close=3, stat=4, fstat=5, lstat=6
  - poll=7, mmap=9, mprotect=10, munmap=11, brk=12
  - rt_sigaction=13, rt_sigprocmask=14
  - ioctl=16, pread64=17, pwrite64=18
  - writev=20, access=21, pipe=22, dup=32, dup2=33
  - getpid=39, sendfile=40, socket=41, connect=42, accept=43
  - fork=57, vfork=58, execve=59, exit=60, wait4=61
  - kill=62, uname=63, fcntl=72, truncate=76, ftruncate=77
  - getcwd=79, chdir=80, mkdir=83, rmdir=84, creat=85
  - unlink=87, symlink=88, readlink=89, chmod=90, chown=92
  - gettimeofday=96, getrlimit=97, getrusage=98
  - getuid=102, getgid=104, geteuid=107, getegid=108
  - getgroups=115, setgroups=116
  - arch_prctl=158, gettid=186, set_tid_address=218
  - openat=257, getdents64=217, clock_gettime=228
  - exit_group=231, epoll_wait=232, epoll_ctl=233
  - tgkill=234, waitid=247, openat=257, mkdirat=258
- Argument convention: rdi, rsi, rdx, r10, r8, r9
- Return in rax; negative errno on error
- syscall clobbers rcx, r11

### No-Hardcode Rule
- All config: `kernel/src/config/nexus_config.h` (generated from `kernel/config/kernel.conf`)
- Userspace: `userspace/config/` directory
- Heap/stack sizes, scheduler quantum, USB timeouts — all via #defines in config headers
- Config files must be self-documenting (every key has a comment)

### Multi-User Framework (groundwork only, not full impl)
- `process_t` must include: uid, gid, euid, egid fields from day 1
- Syscalls getuid/getgid/geteuid/getegid must work
- File permission check stub: `vfs_check_permission(node, uid, gid, mode)` — returns 0 always for now
- Design allows future sudo/setuid implementation without refactor

### libc Expandability
- Directory: `userspace/libc/`
- Layout mirrors glibc: stdio/, stdlib/, string/, unistd/, sys/, signal/, math/, time/
- Every function has a TODO comment if not fully implemented: `/* TODO: full glibc impl */`
- `userspace/libc/README.md` documents every header, its status, and glibc compatibility notes

### Comprehensive Documentation
- Each agent writes `docs/phase4/<subsystem>.md` at task end
- Format: purpose, architecture, data structures, API, config keys, known limitations, future work
- Agents document their codebase so future agents can understand with minimal reads

---

## Implementation Plans (one per agent)

- `planned_implementations/AGENT_A_SCHEDULER.md` — Kernel Agent A
- `planned_implementations/AGENT_B_SYSCALL.md` — Kernel Agent B  
- `planned_implementations/AGENT_C_USERSPACE.md` — App Agent
- `planned_implementations/AGENT_D_USB.md` — Driver Agent

---

## Verification Checklist (4.V)

```
[ ] Two kernel threads run concurrently (interleaved timer ticks visible)
[ ] fork() + execve() tested: init spawns userspace shell
[ ] Userspace shell responds to keyboard input
[ ] write(1, "hello\n", 6) syscall works from Ring 3
[ ] mmap() allocates user pages
[ ] exit() / exit_group() cleans up process
[ ] USB keyboard in QEMU produces input (-device qemu-xhci -device usb-kbd)
[ ] Build: 0 errors, 0 C warnings across all agents' code
[ ] LODBUG serial output shows scheduler ticks + process switches
```

---

## AGENTS.md Update (post-Phase 4)

After all agents complete, the Coordinator (this agent/session) will:
1. Update `knowledge_items/nexusos-task-tracker/artifacts/task.md` — mark 4.x done
2. Update `knowledge_items/nexusos-progress-report/artifacts/progress_report.md`
3. Update `AGENTS.md` with Phase 4 subsystem knowledge
4. Ask user permission before touching `nexusos-bug-pool`
