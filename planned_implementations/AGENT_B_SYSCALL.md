# NexusOS Phase 4 — Agent B: System Calls & User-Mode Transition

> **Role**: Kernel Agent  
> **Tasks**: 4.3 (System calls), 4.4 (User-mode transition)  
> **Files Owned**: `kernel/src/syscall/`, `kernel/src/arch/x86_64/syscall_entry.asm`  
> **Depends On**: Agent A completing `process.h` + `thread.h` (wait for "AGENT A COMPLETE" signal)  
> **Estimated Complexity**: HIGH  
> **Efficiency Mandate**: Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` FIRST

---

## 0. Mandatory Pre-Reading

In order:
1. `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` ← **READ THIS FIRST**
2. `AGENTS.md`
3. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md`
4. `knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md`
5. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md`
6. `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md`
7. `planned_implementations/AGENT_A_SCHEDULER.md` — understand process_t/thread_t
8. `kernel/src/sched/process.h` — PCB layout (from Agent A)
9. `kernel/src/sched/thread.h` — TCB layout (from Agent A)
10. `kernel/src/sched/scheduler.h` — schedule(), block(), wake()
11. `kernel/src/fs/vfs.h` — VFS API (open, read, write, close)
12. `kernel/src/mm/vmm.h` — vmm_map_page for mmap
13. `kernel/src/mm/heap.h` — kmalloc
14. `kernel/src/hal/gdt.h` — segment selectors
15. `kernel/src/kernel.c` — current init (DO NOT MODIFY without user approval)

---

## 1. Overview

You implement:
- Linux-compatible x86_64 syscall ABI using `syscall`/`sysret` instruction pair
- MSR setup: `IA32_STAR`, `IA32_LSTAR`, `IA32_FMASK`
- Assembly entry/exit: `syscall_entry.asm`
- Syscall dispatch table: `syscall_table.c`
- Individual syscall implementations: `syscall_handlers.c`
- ELF64 loader: `elf_loader.c` (used by execve)
- Process creation: `fork` + `execve`
- procfs stubs: `/proc/self`, `/proc/<pid>/` (optional, read spec)

**You do NOT touch**:
- `kernel.c` (protected)
- `hal/` (only read gdt.h for segment values)
- `sched/` (read only — Agent A owns it)
- `userspace/` (App Agent owns it)
- `drivers/usb/` (Driver Agent owns it)

---

## 2. File Layout to Create

```
kernel/src/syscall/
├── syscall.h              ← syscall numbers (Linux ABI), argument macros
├── syscall.c              ← MSR setup, syscall_init()
├── syscall_table.c        ← dispatch table (fn pointers indexed by number)
├── syscall_handlers.c     ← individual handler implementations
├── elf_loader.h           ← ELF64 loading API
└── elf_loader.c           ← ELF64 parser + loader (for execve)

kernel/src/arch/x86_64/
└── syscall_entry.asm      ← low-level syscall/sysret entry point

kernel/src/fs/
└── procfs.c + procfs.h    ← /proc virtual filesystem (optional but recommended)

docs/phase4/
└── syscall.md             ← your documentation (write LAST)
```

---

## 3. Linux Syscall ABI Reference

### Register Convention

| Register | syscall direction | sysret direction |
|---|---|---|
| `rax` | syscall number (IN) | return value (OUT) |
| `rdi` | arg1 | unchanged |
| `rsi` | arg2 | unchanged |
| `rdx` | arg3 | unchanged |
| `r10` | arg4 (NOT rcx!) | unchanged |
| `r8`  | arg5 | unchanged |
| `r9`  | arg6 | unchanged |
| `rcx` | clobbered by CPU (saves RIP) | restored from kernel |
| `r11` | clobbered by CPU (saves RFLAGS) | restored from kernel |

### Errno Convention
- Return value ≥ 0 = success
- Return value = -errno = error (e.g., -2 = ENOENT, -9 = EBADF, -22 = EINVAL)

### Error Constants (in `syscall.h`)
```c
#define EPERM     1    /* Operation not permitted */
#define ENOENT    2    /* No such file or directory */
#define ESRCH     3    /* No such process */
#define EINTR     4    /* Interrupted system call */
#define EIO       5    /* I/O error */
#define EBADF     9    /* Bad file descriptor */
#define ECHILD    10   /* No child processes */
#define EAGAIN    11   /* Try again */
#define ENOMEM    12   /* Out of memory */
#define EACCES    13   /* Permission denied */
#define EFAULT    14   /* Bad address */
#define ENOTDIR   20   /* Not a directory */
#define EISDIR    21   /* Is a directory */
#define EINVAL    22   /* Invalid argument */
#define EMFILE    24   /* Too many open files */
#define ENOSPC    28   /* No space left on device */
#define ENOSYS    38   /* Function not implemented */
#define ENOTEMPTY 39   /* Directory not empty */
```

---

## 4. Syscall Numbers (Linux x86_64 ABI — EXACT)

```c
/* ===================================================================
 * syscall.h — NexusOS System Call Numbers
 * MUST match Linux x86_64 ABI exactly for binary compatibility.
 * Reference: arch/x86/entry/syscalls/syscall_64.tbl in Linux kernel.
 * =================================================================== */

/* Core I/O */
#define SYS_READ          0
#define SYS_WRITE         1
#define SYS_OPEN          2
#define SYS_CLOSE         3
#define SYS_STAT          4
#define SYS_FSTAT         5
#define SYS_LSTAT         6
#define SYS_POLL          7
#define SYS_LSEEK         8
#define SYS_MMAP          9
#define SYS_MPROTECT      10
#define SYS_MUNMAP        11
#define SYS_BRK           12

/* Signals */
#define SYS_RT_SIGACTION  13
#define SYS_RT_SIGPROCMASK 14
#define SYS_RT_SIGRETURN  15

/* I/O control */
#define SYS_IOCTL         16
#define SYS_PREAD64       17
#define SYS_PWRITE64      18
#define SYS_READV         19
#define SYS_WRITEV        20
#define SYS_ACCESS        21
#define SYS_PIPE          22
#define SYS_SELECT        23

/* Process */
#define SYS_DUP           32
#define SYS_DUP2          33
#define SYS_GETPID        39
#define SYS_SENDFILE      40
#define SYS_SOCKET        41
#define SYS_CONNECT       42
#define SYS_ACCEPT        43
#define SYS_FORK          57
#define SYS_VFORK         58
#define SYS_EXECVE        59
#define SYS_EXIT          60
#define SYS_WAIT4         61
#define SYS_KILL          62
#define SYS_UNAME         63

/* File operations */
#define SYS_FCNTL         72
#define SYS_TRUNCATE      76
#define SYS_FTRUNCATE     77
#define SYS_GETDENTS      78
#define SYS_GETCWD        79
#define SYS_CHDIR         80
#define SYS_MKDIR         83
#define SYS_RMDIR         84
#define SYS_CREAT         85
#define SYS_UNLINK        87
#define SYS_GETTIMEOFDAY  96
#define SYS_GETRLIMIT     97
#define SYS_GETUID        102
#define SYS_GETGID        104
#define SYS_GETEUID       107
#define SYS_GETEGID       108
#define SYS_GETPPID       110
#define SYS_GETGROUPS     115
#define SYS_SETGROUPS     116

/* Thread/arch */
#define SYS_ARCH_PRCTL    158
#define SYS_GETTID        186
#define SYS_GETDENTS64    217
#define SYS_SET_TID_ADDR  218
#define SYS_CLOCK_GETTIME 228
#define SYS_EXIT_GROUP    231
#define SYS_TGKILL        234
#define SYS_WAITID        247
#define SYS_OPENAT        257
#define SYS_MKDIRAT       258
#define SYS_NEWFSTATAT    262
#define SYS_UNLINKAT      263

/* NexusOS extensions (above 512 to avoid collision) */
#define SYS_NEXUS_DEBUG   512
```

---

## 5. MSR Setup (`syscall.c`)

```c
/* syscall.c — System Call Interface Initialization */

#include "syscall.h"
#include "../hal/io.h"     /* rdmsr/wrmsr */
#include "../hal/gdt.h"    /* segment selectors */

/* MSR addresses */
#define MSR_EFER      0xC0000080
#define MSR_STAR      0xC0000081
#define MSR_LSTAR     0xC0000082
#define MSR_FMASK     0xC0000084

/* External: assembly entry point in syscall_entry.asm */
extern void syscall_entry(void);

int syscall_init(void) {
    /* 1. Enable SCE (System Call Extensions) bit in EFER */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 0);  /* SCE bit */
    wrmsr(MSR_EFER, efer);

    /* 2. Set STAR: bits 63:48 = user CS-8 (for sysret), bits 47:32 = kernel CS
     * Layout: [63:48] user_cs | [47:32] kernel_cs
     * GDT: 0x08=kernel_code, 0x10=kernel_data, 0x18=user_code, 0x20=user_data
     * sysret loads CS from STAR[63:48]+16 and SS from STAR[63:48]+8
     * syscall loads CS from STAR[47:32] and SS from STAR[47:32]+8   */
    uint64_t star = 0;
    star |= ((uint64_t)0x0008 << 32);  /* kernel code selector */
    star |= ((uint64_t)0x0013 << 48);  /* user code selector - 8 (sysret adds 16) */
    wrmsr(MSR_STAR, star);

    /* 3. Set LSTAR: kernel syscall entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. Set FMASK: mask applied to RFLAGS on syscall entry
     * Bit 9 = IF (interrupt flag) — mask it so we enter with interrupts off */
    wrmsr(MSR_FMASK, (1ULL << 9));  /* disable interrupts on syscall entry */

    debug_log(DEBUG_LEVEL_INFO, "SYSCALL", "Syscall interface initialized (LSTAR=%p)",
              (void *)syscall_entry);
    return 0;
}
```

**IMPORTANT**: The STAR segment selectors must match your GDT exactly. Read `kernel/src/hal/gdt.c` to confirm the exact offsets before hardcoding these values.

---

## 6. Assembly Entry Point (`syscall_entry.asm`)

```nasm
; syscall_entry.asm — System Call Entry/Exit
; 
; Upon syscall instruction:
;   CPU saves RIP -> RCX, RFLAGS -> R11
;   CPU loads CS/SS from IA32_STAR
;   CPU jumps to IA32_LSTAR (here)
;   Interrupts masked by IA32_FMASK
;
; Our job:
;   Save all user registers onto the kernel stack
;   Call C handler: syscall_dispatch(rax, rdi, rsi, rdx, r10, r8, r9)
;   Restore user registers
;   Execute sysretq

[bits 64]
section .text

extern syscall_dispatch   ; C function in syscall_table.c

global syscall_entry
syscall_entry:
    ; At entry: we are on USER stack (RSP points to user memory)
    ; We must switch to the kernel stack for this thread.
    ; The kernel stack pointer is stored in TSS.RSP0.
    ; Use swapgs to access the per-CPU area where we stored the user RSP.
    ;
    ; Simple approach (single CPU): use a per-process kernel stack RSP
    ; stored in a known location. We use the TSS RSP0 mechanism with swapgs.
    ;
    ; HOWEVER: for Phase 4 (single CPU), we can use a simpler approach:
    ; The current thread's kernel stack is in current_thread->kstack_virt.
    ; We store the user RSP temporarily in a scratch location.

    ; Save user RSP in a temp location (per-CPU scratch)
    ; For single-CPU Phase 4, use a global variable:
    swapgs                      ; swap GS to kernel GS base (must set up MSR_GS_BASE)

    ; Alternative simpler approach without swapgs:
    ; Use the red zone is NOT available (kernel compiled -mno-red-zone)
    ; Use a per-CPU scratch variable

    ; RECOMMENDED for Phase 4 simplicity:
    ; Keep using a global kernel_syscall_rsp0 variable
    ; and update it in the scheduler when switching threads.

    ; For Phase 4, simplest correct approach:
    mov     [rel user_rsp_scratch], rsp   ; save user RSP
    mov     rsp, [rel kernel_rsp_scratch] ; load kernel stack RSP

    ; Push user context as a syscall_frame_t on kernel stack
    push    rcx                ; saved user RIP (from syscall instruction)
    push    r11                ; saved user RFLAGS
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    push    [rel user_rsp_scratch]  ; user RSP

    ; Enable interrupts now (we're safely on kernel stack)
    sti

    ; Set up arguments for syscall_dispatch:
    ; int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2,
    ;                          uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)
    ; rax=num, rdi=a1, rsi=a2, rdx=a3, r10=a4, r8=a5, r9=a6
    ; Move r10 to rcx (4th arg in SysV ABI for the C call):
    mov     rcx, r10

    ; Call C dispatcher (rax, rdi, rsi, rdx already correct)
    call    syscall_dispatch

    ; rax now holds return value
    cli                        ; disable interrupts before returning to user

    ; Restore saved registers
    pop     r15                ; discard user RSP (we restore from stack logic)
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    pop     r11                ; restore RFLAGS for sysretq
    pop     rcx                ; restore RIP for sysretq

    ; Switch back to user stack
    mov     rsp, [rel user_rsp_scratch]

    sysretq



section .data
align 8
user_rsp_scratch:   dq 0   ; stores user RSP during syscall
kernel_rsp_scratch: dq 0   ; kernel RSP for current thread (updated by scheduler)

global kernel_rsp_scratch    ; exported so scheduler can update it
```

> **Note**: The `swapgs`/`MSR_GS_BASE` approach is cleaner for SMP. For Phase 4 single-CPU, the `kernel_rsp_scratch` global variable is simpler. The scheduler must update `kernel_rsp_scratch` on every context switch.

---

## 7. Syscall Dispatch (`syscall_table.c`)

```c
/* syscall_table.c — Syscall Dispatch Table */

#include "syscall.h"
#include "syscall_handlers.h"
#include "../lib/debug.h"

typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t);

/* Table indexed by syscall number */
static syscall_fn_t syscall_table[512] = {
    [SYS_READ]         = sys_read,
    [SYS_WRITE]        = sys_write,
    [SYS_OPEN]         = sys_open,
    [SYS_CLOSE]        = sys_close,
    [SYS_STAT]         = sys_stat,
    [SYS_FSTAT]        = sys_fstat,
    [SYS_LSEEK]        = sys_lseek,
    [SYS_MMAP]         = sys_mmap,
    [SYS_MPROTECT]     = sys_mprotect,
    [SYS_MUNMAP]       = sys_munmap,
    [SYS_BRK]          = sys_brk,
    [SYS_EXIT]         = sys_exit,
    [SYS_FORK]         = sys_fork,
    [SYS_VFORK]        = sys_fork,   /* alias to fork for Phase 4 */
    [SYS_EXECVE]       = sys_execve,
    [SYS_WAIT4]        = sys_wait4,
    [SYS_WAITID]       = sys_waitid,
    [SYS_GETPID]       = sys_getpid,
    [SYS_GETPPID]      = sys_getppid,
    [SYS_GETUID]       = sys_getuid,
    [SYS_GETGID]       = sys_getgid,
    [SYS_GETEUID]      = sys_geteuid,
    [SYS_GETEGID]      = sys_getegid,
    [SYS_GETCWD]       = sys_getcwd,
    [SYS_CHDIR]        = sys_chdir,
    [SYS_MKDIR]        = sys_mkdir,
    [SYS_RMDIR]        = sys_rmdir,
    [SYS_UNLINK]       = sys_unlink,
    [SYS_DUP]          = sys_dup,
    [SYS_DUP2]         = sys_dup2,
    [SYS_PIPE]         = sys_pipe,
    [SYS_IOCTL]        = sys_ioctl,
    [SYS_UNAME]        = sys_uname,
    [SYS_KILL]         = sys_kill,
    [SYS_GETTID]       = sys_gettid,
    [SYS_SET_TID_ADDR] = sys_set_tid_address,
    [SYS_ARCH_PRCTL]   = sys_arch_prctl,
    [SYS_CLOCK_GETTIME]= sys_clock_gettime,
    [SYS_EXIT_GROUP]   = sys_exit_group,
    [SYS_TGKILL]       = sys_tgkill,
    [SYS_OPENAT]       = sys_openat,
    [SYS_GETDENTS64]   = sys_getdents64,
    [SYS_RT_SIGACTION] = sys_rt_sigaction,
    [SYS_RT_SIGPROCMASK]= sys_rt_sigprocmask,
};

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2,
                         uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    if (num >= 512 || syscall_table[num] == NULL) {
        debug_log(DEBUG_LEVEL_WARN, "SYSCALL",
                  "Unknown syscall %llu from PID %d", num,
                  current_process ? current_process->pid : -1);
        return -ENOSYS;
    }
    return syscall_table[num](a1, a2, a3, a4, a5, a6);
}
```

---

## 8. Syscall Handlers (`syscall_handlers.c`)

Implement each handler. Priority order (implement these first, rest are stubs):

### MUST IMPLEMENT (fully functional)
1. `sys_write` — write to fd (fd 1 = stdout → kprintf/terminal)
2. `sys_read` — read from fd (fd 0 = stdin → input_poll_event)
3. `sys_open` — open file via VFS
4. `sys_close` — close fd
5. `sys_exit` / `sys_exit_group` — process exit via scheduler
6. `sys_fork` — clone process (copy-on-write optional, just deep copy for Phase 4)
7. `sys_execve` — load ELF, replace process image
8. `sys_getpid` / `sys_getppid` — trivial
9. `sys_getuid` / `sys_getgid` / `sys_geteuid` / `sys_getegid` — read from PCB
10. `sys_mmap` — anonymous mmap (MAP_ANONYMOUS)
11. `sys_brk` — extend heap
12. `sys_wait4` — wait for child
13. `sys_uname` — return uname struct with NexusOS info
14. `sys_getcwd` — return CWD from PCB
15. `sys_chdir` — change CWD in PCB via VFS
16. `sys_arch_prctl` — ARCH_SET_FS (needed by glibc TLS)
17. `sys_set_tid_address` — needed by musl/glibc startup
18. `sys_clock_gettime` — return ms-based time from pit_get_ticks
19. `sys_rt_sigaction` — stub (return 0, signals later)
20. `sys_rt_sigprocmask` — stub (return 0)

### STUB (return -ENOSYS initially, document TODO)
Everything else. **Must not crash** — return -ENOSYS cleanly.

### sys_write example:
```c
int64_t sys_write(uint64_t fd, uint64_t buf_user, uint64_t count,
                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;

    /* Validate user pointer — basic check */
    if (buf_user == 0 || buf_user >= PROC_USER_VIRT_END) return -EFAULT;
    if (count == 0) return 0;
    if (count > 65536) count = 65536;  /* cap for safety */

    const char *buf = (const char *)buf_user; /* user-space pointer */

    if (fd == 1 || fd == 2) {
        /* stdout / stderr → terminal */
        for (uint64_t i = 0; i < count; i++) {
            terminal_putchar(&main_terminal, buf[i]);
        }
        return (int64_t)count;
    }

    /* Other FDs: look up in current_process->fds */
    if (fd >= 64 || current_process->fds[fd] == NULL) return -EBADF;
    vfs_node_t *node = (vfs_node_t *)current_process->fds[fd];
    int64_t r = vfs_write(node, buf, count, /* offset from seek pos */ 0);
    return r;
}
```

### sys_fork:
```c
int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3,
                 uint64_t a4, uint64_t a5, uint64_t a6) {
    /* Create new PCB, clone address space (deep copy for Phase 4) */
    process_t *child = process_create(current_process->pid);
    if (!child) return -ENOMEM;

    /* Clone FDs */
    for (int i = 0; i < 64; i++) child->fds[i] = current_process->fds[i];
    memcpy(child->cwd, current_process->cwd, 256);

    /* Clone address space (Phase 4: full copy; Phase 5+: COW) */
    child->cr3 = vmm_clone_address_space(current_process->cr3);
    if (!child->cr3) { process_destroy(child); return -ENOMEM; }

    /* Clone current thread into child */
    thread_t *child_thread = thread_create(child,
        /* entry = return from syscall, set to syscall return path */
        (uint64_t)fork_child_return, 0, 0);

    /* Child returns 0 from fork — set return value in child's stack */
    /* parent's kernel stack has syscall frame — copy and set rax=0 in child */
    fork_setup_child_return(child_thread, 0 /* child rax */);

    scheduler_enqueue(child_thread);

    /* Parent returns child PID */
    return child->pid;
}
```

### sys_execve:
```c
int64_t sys_execve(uint64_t path_user, uint64_t argv_user, uint64_t envp_user,
                   uint64_t a4, uint64_t a5, uint64_t a6) {
    const char *path = (const char *)path_user;
    /* 1. Open file via VFS */
    vfs_node_t *file = vfs_open(path, 0);
    if (!file) return -ENOENT;

    /* 2. Load ELF into current process's address space */
    uint64_t entry_point = 0;
    int r = elf_load(current_process, file, &entry_point);
    vfs_close(file);
    if (r != 0) return -ENOEXEC;

    /* 3. Set up user stack with argc/argv/envp */
    uint64_t new_sp = setup_user_stack(current_process, argv_user, envp_user);

    /* 4. Replace current thread's return context:
     *    Next time scheduler returns to user, it goes to entry_point with new_sp */
    exec_replace_context(current_thread, entry_point, new_sp);

    /* 5. exec does not return on success */
    scheduler_yield();  /* will return to user at new entry point */
    return 0;  /* unreachable */
}
```

---

## 9. ELF64 Loader (`elf_loader.c`)

```c
/* ELF64 loader — loads a static ELF binary into a process address space */

#include <stdint.h>
#include "elf_loader.h"
#include "../fs/vfs.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"

/* ELF64 header magic: 0x7F 'E' 'L' 'F' */
#define ELF_MAGIC   0x464C457F

typedef struct {
    uint32_t e_ident_magic;
    uint8_t  e_ident_class;       /* 2 = ELF64 */
    uint8_t  e_ident_data;        /* 1 = little-endian */
    uint8_t  e_ident_version;
    uint8_t  e_ident_osabi;
    uint8_t  e_ident_pad[8];
    uint16_t e_type;              /* 2 = ET_EXEC */
    uint16_t e_machine;           /* 0x3E = x86_64 */
    uint32_t e_version;
    uint64_t e_entry;             /* Entry point virtual address */
    uint64_t e_phoff;             /* Program header table offset */
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;             /* Number of program headers */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_hdr_t;

typedef struct {
    uint32_t p_type;    /* 1 = PT_LOAD */
    uint32_t p_flags;   /* PF_R=4, PF_W=2, PF_X=1 */
    uint64_t p_offset;  /* Offset in file */
    uint64_t p_vaddr;   /* Virtual address to load at */
    uint64_t p_paddr;
    uint64_t p_filesz;  /* Size in file */
    uint64_t p_memsz;   /* Size in memory (may be > filesz, zero-pad) */
    uint64_t p_align;
} __attribute__((packed)) elf64_phdr_t;

#define PT_LOAD     1
#define PF_X        1
#define PF_W        2
#define PF_R        4

/* Load ELF file into process. Returns 0 on success, -1 on error.
 * Sets *entry_out to the ELF entry point virtual address. */
int elf_load(process_t *proc, vfs_node_t *file, uint64_t *entry_out) {
    elf64_hdr_t hdr;
    if (vfs_read(file, &hdr, sizeof(hdr), 0) < (int64_t)sizeof(hdr)) return -1;
    if (hdr.e_ident_magic != ELF_MAGIC) return -1;
    if (hdr.e_ident_class != 2) return -1;   /* not 64-bit */
    if (hdr.e_machine != 0x3E) return -1;    /* not x86_64 */

    *entry_out = hdr.e_entry;

    /* Switch to process page table */
    uint64_t saved_cr3 = read_cr3();
    write_cr3(proc->cr3);

    /* Load each PT_LOAD segment */
    for (uint16_t i = 0; i < hdr.e_phnum; i++) {
        elf64_phdr_t phdr;
        uint64_t phdr_offset = hdr.e_phoff + i * hdr.e_phentsize;
        vfs_read(file, &phdr, sizeof(phdr), phdr_offset);

        if (phdr.p_type != PT_LOAD) continue;
        if (phdr.p_memsz == 0) continue;

        /* Allocate pages for this segment */
        uint64_t virt_start = phdr.p_vaddr & ~0xFFFULL;
        uint64_t virt_end   = (phdr.p_vaddr + phdr.p_memsz + 0xFFF) & ~0xFFFULL;
        uint64_t num_pages  = (virt_end - virt_start) / 4096;

        uint64_t flags = PAGE_PRESENT | PAGE_USER;
        if (phdr.p_flags & PF_W) flags |= PAGE_WRITABLE;

        for (uint64_t j = 0; j < num_pages; j++) {
            uint64_t phys = pmm_alloc_page();
            if (!phys) { write_cr3(saved_cr3); return -1; }
            uint64_t virt = virt_start + j * 4096;
            vmm_map_page(virt, phys, flags);
            memset((void *)(phys + hhdm_offset), 0, 4096);
        }

        /* Copy file content */
        if (phdr.p_filesz > 0) {
            vfs_read(file, (void *)phdr.p_vaddr, phdr.p_filesz, phdr.p_offset);
        }
        /* Zero-pad (p_memsz - p_filesz) — already zeroed by memset above */
    }

    write_cr3(saved_cr3);
    return 0;
}
```

---

## 10. User-Mode Transition (First Process Launch)

After `scheduler_init()` creates the idle thread, `init_process_start()` must:

1. Create `process_t` for init (PID 1)
2. Load `/init` ELF binary using `elf_load()`
3. Set up user stack (argc=1, argv=["init"], envp=[NULL])
4. Create thread with `kstack_rsp` pointing to a pre-built frame that will `sysretq` to user entry

The key insight: instead of modifying `context.asm`, you pre-build the kernel stack of the init thread so that when `schedule()` switches to it, it IRET/sysrets into Ring 3.

```c
void init_process_start(void) {
    /* 1. Create PCB */
    process_t *init = process_create(0);  /* ppid=0 (kernel) */
    init->pid = PID_INIT;
    init->uid = 0; init->euid = 0;  /* root for now */
    init->gid = 0; init->egid = 0;

    /* 2. Allocate page table */
    init->cr3 = vmm_clone_kernel_space();  /* copy kernel mappings */

    /* 3. Load ELF */
    vfs_node_t *init_bin = vfs_open("/init", 0);
    if (!init_bin) kpanic("Cannot open /init");
    uint64_t entry = 0;
    if (elf_load(init, init_bin, &entry) != 0) kpanic("Failed to load /init");
    vfs_close(init_bin);

    /* 4. Setup user stack */
    uint64_t user_sp = setup_init_stack(init, entry);

    /* 5. Create init thread with user-mode return frame */
    thread_t *t = thread_create_usermode(init, entry, user_sp);

    /* 6. Set FDs: stdin=tty0, stdout=tty0, stderr=tty0 */
    init->fds[0] = vfs_open("/dev/tty0", 0);
    init->fds[1] = init->fds[0];
    init->fds[2] = init->fds[0];

    /* 7. Enqueue */
    scheduler_enqueue(t);

    debug_log(DEBUG_LEVEL_INFO, "SYSCALL", "Init process created (PID 1, entry=%p)",
              (void *)entry);
}
```

`thread_create_usermode` pre-builds a kernel stack that contains a sysret frame:
```c
/* Pre-built kernel stack for user-mode thread entry:
 * When switch_context returns into this thread, it pops callee-saved regs,
 * then executes the code at the bottom of the function which does sysretq.
 * We achieve this by making the "return address" in switch_context point to
 * a kernel stub that calls sysretq with the right rcx (user RIP) and rsp. */
```

A simpler approach: use IRETQ instead of sysretq for the first entry into user mode.
Pre-build an IRET frame on the kernel stack:
```
high address (stack top)
  [user SS  = 0x23]   <- user data selector + RPL3
  [user RSP = user_sp]
  [user RFLAGS = 0x200]  (IF bit set = interrupts enabled)
  [user CS  = 0x1B]   <- user code selector + RPL3
  [user RIP = entry]
low address
```
Then in `thread_create_usermode`, set `t->kstack_rsp` to point to a stub that executes IRETQ after popping these 5 values.

---

## 11. procfs (Optional but Recommended)

Create `kernel/src/fs/procfs.c`:
- Mount at `/proc`
- `/proc/self` → symlink to `/proc/<current_pid>`
- `/proc/<pid>/status` → text file with uid, gid, ppid, state
- `/proc/<pid>/maps` → virtual memory map
- `/proc/<pid>/fd/` → open file descriptors
- `/proc/uptime` → seconds since boot

This is optional but dramatically improves Linux compatibility. `ps`, `top`, `cat /proc/self/status` used by many programs.

---

## 12. File Descriptor Management

All FD management lives in the PCB (`process_t->fds[64]`). Rules:
- `fds[0]` = stdin, `fds[1]` = stdout, `fds[2]` = stderr (set by init/exec)
- `sys_open()`: scan for first NULL slot, open VFS node, store pointer
- `sys_close()`: call `vfs_close()`, set slot to NULL
- `sys_dup(fd)`: copy pointer to next free slot
- `sys_dup2(old,new)`: copy pointer to `fds[new]`, close if already open
- `sys_fork()`: copy entire `fds[]` array to child

---

## 13. Memory Mapping (`sys_mmap`)

For Phase 4, implement anonymous mmap (MAP_ANONYMOUS):
```c
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset) {
    /* MAP_ANONYMOUS: allocate pages, map into user address space */
    if (!(flags & MAP_ANONYMOUS)) return -ENOSYS;  /* file-backed: later */

    uint64_t num_pages = (length + 0xFFF) / 4096;
    uint64_t virt = addr;
    if (!virt) {
        /* Find free virtual address */
        virt = current_process->heap_end;
        current_process->heap_end += num_pages * 4096;
    }

    uint64_t vmm_flags = PAGE_PRESENT | PAGE_USER;
    if (prot & PROT_WRITE) vmm_flags |= PAGE_WRITABLE;

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (!phys) return -ENOMEM;
        vmm_map_page(virt + i * 4096, phys, vmm_flags);
        memset((void *)(phys + hhdm_offset), 0, 4096);
    }
    return (int64_t)virt;
}
```

---

## 14. Build & Test Protocol

```bash
make clean && make all   # Must: 0 errors, 0 warnings

# Test syscall interface:
# Create a minimal test binary in userspace/ that calls write(1, "hello\n", 6)
# Load it via init_process_start() instead of real /init
# Verify "hello" appears on terminal

make all lodbug && make run lodbug
# Watch serial for: [SYSCALL] write fd=1 count=6
```

### Common Mistakes
1. **STAR register wrong** → system hangs on first syscall. Double-check GDT offsets.
2. **Kernel RSP not set** → corrupted kernel stack, triple fault on first syscall.
3. **Missing sysretq** → CPU stays in ring 0, user program never runs.
4. **rcx/r11 clobbered** → user RIP/RFLAGS destroyed: infinite fault loop.
5. **No `sti` on entry** → interrupts stay off: scheduler stalls, system freezes.

---

## 15. Memory Efficiency Requirements (MANDATORY)

> Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` BEFORE implementing.

### sys_fork — COW Implementation (REQUIRED, not optional)

**Do NOT deep-copy the address space.** Use COW from day 1:

```c
int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3,
                 uint64_t a4, uint64_t a5, uint64_t a6) {
    process_t *child = process_create(current_process->pid);
    if (!child) return -ENOMEM;

    /* Copy FD table pointer (lazy: share until exec) */
    if (current_process->fd_table) {
        child->fd_table = kmalloc(sizeof(fd_table_t));
        memcpy(child->fd_table, current_process->fd_table, sizeof(fd_table_t));
    }

    /* COW: clone page table, mark ALL writable user pages read-only in BOTH */
    child->cr3 = vmm_cow_clone(current_process->cr3);
    if (!child->cr3) { process_destroy(child); return -ENOMEM; }

    /* Copy stack/heap limits */
    child->heap_base         = current_process->heap_base;
    child->heap_end          = current_process->heap_end;
    child->stack_top_virt    = current_process->stack_top_virt;
    child->stack_limit_virt  = current_process->stack_limit_virt;

    /* Create child thread with fork return = 0 */
    thread_t *ct = thread_create_fork_child(child, current_thread);
    scheduler_enqueue(ct);

    return child->pid;  /* parent gets child PID; child gets 0 via fork return setup */
}
```

`vmm_cow_clone(parent_cr3)`:
1. Allocate new PML4 (1 page)
2. Copy kernel entries 256–511 (shared, no duplication)
3. Walk user entries 0–255: for each present PTE:
   - Call `pmm_page_ref(phys)` → increment refcount
   - Mark PTE as read-only in BOTH parent and child page tables
   - Copy the PTE to child's page table
4. Flush TLB (write CR3)
5. Total cost: 1 PML4 page (4 KB) regardless of process size

### sys_mmap — Lazy Physical Allocation

```c
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                 uint64_t flags, uint64_t fd, uint64_t offset) {
    if (!(flags & MAP_ANONYMOUS)) return -ENOSYS;

    uint64_t num_pages = (length + 0xFFF) / 4096;
    uint64_t virt = addr ? addr : current_process->heap_end;

    /* EFFICIENCY: do NOT allocate physical pages now.
     * Just extend virtual range. Pages allocated on demand in page_fault_handler.
     * This means mmap(2MB) costs zero physical RAM until actually accessed. */
    current_process->heap_end = virt + num_pages * 4096;

    /* Mark virtual range as "reserved anon" in process mm_region list */
    /* (simple approach: just update heap_end; fault handler does the rest) */

    return (int64_t)virt;
}
```

### sys_brk — Lazy Extension

```c
int64_t sys_brk(uint64_t new_brk, uint64_t a2, uint64_t a3,
                uint64_t a4, uint64_t a5, uint64_t a6) {
    if (new_brk == 0) return (int64_t)current_process->heap_end;
    if (new_brk < current_process->heap_base) return -EINVAL;

    /* EFFICIENCY: extending brk just moves the virtual boundary.
     * No physical pages allocated until accessed (page fault handler). */
    current_process->heap_end = (new_brk + 0xFFF) & ~0xFFFULL;
    return (int64_t)current_process->heap_end;
}
```

### ELF Loader — Lazy Segment Loading

ELF loader MUST NOT pre-fault all pages. For each PT_LOAD segment:
```c
/* DO NOT: */
for (uint64_t j = 0; j < num_pages; j++) {
    uint64_t phys = pmm_alloc_page();
    vmm_map_page(virt + j * 4096, phys, flags);  /* WRONG: allocates all upfront */
}

/* DO INSTEAD: */
/* Only map the pages that contain actual file content (p_filesz) immediately.
 * The rest (zero-fill region: p_memsz - p_filesz) will be demand-paged. */
uint64_t file_pages = (phdr.p_filesz + 0xFFF) / 4096;
for (uint64_t j = 0; j < file_pages; j++) {
    uint64_t phys = pmm_alloc_page();
    vmm_map_page(virt_start + j * 4096, phys, flags);
    /* Copy only needed portion from file */
    uint64_t copy_off = j * 4096;
    uint64_t copy_len = (j == file_pages-1) ? (phdr.p_filesz % 4096) : 4096;
    vfs_read(file, (void *)(phys + hhdm_offset), copy_len, phdr.p_offset + copy_off);
}
/* BSS region: mark virtual range as demand-anon. page fault fills with zero. */
/* No physical allocation needed — handled by page_fault_handler. */
```

### procfs — Zero Physical Cost

Implement procfs with **zero static buffers**. All content computed on-read:
```c
/* BAD — wastes RAM: */
static char proc_status_buf[4096];  /* pre-allocated, always in RAM */

/* GOOD — zero waste: */
int64_t procfs_read_status(vfs_node_t *node, void *buf, uint64_t size, uint64_t off) {
    process_t *p = (process_t *)node->private;
    char tmp[256];
    int len = snprintf(tmp, sizeof(tmp),
        "Pid: %d\nPPid: %d\nUid: %u\nGid: %u\nState: %c\n",
        p->pid, p->ppid, p->uid, p->gid, proc_state_char(p->state));
    /* Copy to user buf */
    if (off >= (uint64_t)len) return 0;
    uint64_t copy = len - off;
    if (copy > size) copy = size;
    memcpy(buf, tmp + off, copy);
    return copy;
}
```

### Memory Budget (document in syscall.md):

| Allocation | Size | When | Freed |
|---|---|---|---|
| Syscall table (512 fn ptrs) | 4 KB static | boot | never |
| `kernel_rsp_scratch` (1 qword) | 8 B static | boot | never |
| ELF phdr read buffer | 64 B stack | during exec | on return |
| procfs node per process | ~64 B heap | on mount | process exit |
| **Per-fork cost** | **4 KB (one PML4)** | fork | child exit |
| **Avg mmap cost** | **0 until accessed** | mmap | munmap |

---

## 16. Documentation Required (write LAST)

Create `docs/phase4/syscall.md`:
- Linux ABI register table
- syscall_entry.asm annotated flow diagram
- MSR configuration (STAR, LSTAR, FMASK values)
- syscall_table layout + how to add new syscall
- ELF loader: what segment types are handled
- FD table layout
- mmap implementation notes
- uname struct returned values
- ARCH_PRCTL supported operations
- Known limitations: signals unimplemented, file-backed mmap unimplemented
- Future: signals, COW fork, file-backed mmap, networking sockets

---

## 16. Starting Agent Prompt

```
You are a Kernel Agent implementing NexusOS Phase 4 Tasks 4.3 (System Calls)
and 4.4 (User-mode transition).

WAIT CONDITION: Do not start coding until you confirm Agent A is complete
(kernel/src/sched/process.h and kernel/src/sched/thread.h exist and are readable).

READ FIRST (mandatory, in order):
1. planned_implementations/AGENT_B_SYSCALL.md (this document — full spec)
2. AGENTS.md
3. knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md
4. knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md
5. knowledge_items/nexusos-progress-report/artifacts/progress_report.md
6. knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md
7. kernel/src/sched/process.h (from Agent A)
8. kernel/src/sched/thread.h (from Agent A)
9. kernel/src/sched/scheduler.h (from Agent A)
10. kernel/src/fs/vfs.h
11. kernel/src/mm/vmm.h, pmm.h, heap.h
12. kernel/src/hal/gdt.h (for segment selectors)
13. kernel/src/kernel.c (DO NOT MODIFY)

YOUR TASK:
Implement the Linux-compatible x86_64 syscall interface as specified in
planned_implementations/AGENT_B_SYSCALL.md.

Create these files:
- kernel/src/syscall/syscall.h (syscall numbers + errno constants)
- kernel/src/syscall/syscall.c (MSR setup, syscall_init)
- kernel/src/syscall/syscall_table.c (dispatch table)
- kernel/src/syscall/syscall_handlers.c (individual handlers)
- kernel/src/syscall/elf_loader.h + elf_loader.c (ELF64 loader)
- kernel/src/arch/x86_64/syscall_entry.asm (low-level entry/exit)
- kernel/src/fs/procfs.h + procfs.c (optional /proc filesystem)

RULES:
- Build must pass 0 errors, 0 C warnings after every file
- Syscall numbers MUST match Linux x86_64 exactly (table in spec)
- Unimplemented syscalls return -ENOSYS (38) — never crash
- All pointers from userspace: validate before dereference
- Use debug_log for every syscall at DEBUG_LEVEL_TRACE
- DO NOT modify: kernel.c, hal/idt.c, hal/pic.c, mm/*.c, sched/
- Coordinate with user before modifying kernel.c init sequence

VERIFICATION:
1. Write a minimal test userspace binary that calls:
   write(1, "NexusOS syscall OK\n", 19)
   exit(0)
2. Place compiled binary in ramfs during init
3. Load and run it via init_process_start()
4. Verify string appears in terminal
5. Build: 0 errors, 0 warnings

WHEN DONE:
1. Write docs/phase4/syscall.md
2. Export: kernel/src/syscall/syscall.h is stable API for App Agent
3. Report: files created, build status, test results
4. Signal: "AGENT B COMPLETE — syscall.h and init_process_start available"
```
