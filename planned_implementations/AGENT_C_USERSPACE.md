# NexusOS Phase 4 — Agent C: Userspace (libc + init + shell)

> **Role**: App Agent  
> **Tasks**: 4.5 (Minimal libc), 4.6 (Init process + userspace shell)  
> **Files Owned**: entire `userspace/` tree  
> **Depends On**: Agent B completing syscall.h + init_process_start (wait for "AGENT B COMPLETE")  
> **Estimated Complexity**: MEDIUM-HIGH  
> **Efficiency Mandate**: Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` FIRST

---

## 0. Mandatory Pre-Reading

> **KEY EFFICIENCY RULE**: Your binaries must be small. Target: `init` ≤ 50 KB, `nsh` ≤ 100 KB.
> Use `-Os -ffunction-sections -fdata-sections --gc-sections` in ALL makefiles.
> Every libc function added must be called by something real. No dead code.

In order:
1. `AGENTS.md`
2. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md`
3. `planned_implementations/AGENT_B_SYSCALL.md` — understand syscall ABI + Linux numbers
4. `kernel/src/syscall/syscall.h` — exact syscall numbers (from Agent B)
5. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md`
6. `kernel/src/shell/shell_core.c` — existing kernel shell commands for reference
7. `kernel/src/shell/shell_command.h` — understand command registration pattern
8. `kernel/src/config/nexus_config.h` — config constants

**You do NOT touch any kernel source files. Your domain is `userspace/` only.**

---

## 1. Overview

You build:
- `userspace/libc/` — minimal C library (expandable to full glibc)
- `userspace/init/init.c` — PID 1 process
- `userspace/shell/shell.c` — userspace shell (replaces kernel shell long-term)
- `userspace/GNUmakefile` — builds all userspace ELFs
- `userspace/config/` — runtime config files

The libc is designed to be progressively expanded. All unimplemented functions have clear TODO stubs. Directory structure mirrors glibc.

---

## 2. Directory Layout

```
userspace/
├── GNUmakefile                 ← build all userspace ELFs
├── README.md                   ← how to build + run userspace programs
├── config/
│   ├── init.conf               ← init configuration (programs to spawn)
│   └── shell.conf              ← shell configuration (prompt, history size)
├── libc/
│   ├── README.md               ← libc status, glibc compat map
│   ├── include/                ← all header files
│   │   ├── syscall.h           ← syscall wrappers (low-level)
│   │   ├── unistd.h            ← POSIX: read, write, fork, exec, etc.
│   │   ├── stdio.h             ← printf, puts, fgets, fopen, fclose
│   │   ├── stdlib.h            ← malloc, free, exit, atoi, getenv
│   │   ├── string.h            ← strlen, strcpy, strcmp, memcpy, etc.
│   │   ├── stdint.h            ← uint8_t, uint64_t, etc.
│   │   ├── stddef.h            ← size_t, NULL, offsetof
│   │   ├── stdbool.h           ← bool, true, false
│   │   ├── errno.h             ← errno variable + error codes
│   │   ├── fcntl.h             ← O_RDONLY, O_WRONLY, O_CREAT, etc.
│   │   ├── sys/types.h         ← pid_t, uid_t, gid_t, off_t, mode_t
│   │   ├── sys/stat.h          ← struct stat, stat()
│   │   ├── sys/wait.h          ← waitpid, WEXITSTATUS
│   │   ├── sys/mman.h          ← mmap, munmap, PROT_*, MAP_*
│   │   ├── sys/ioctl.h         ← ioctl
│   │   ├── sys/utsname.h       ← struct utsname, uname()
│   │   ├── dirent.h            ← opendir, readdir, closedir
│   │   ├── signal.h            ← signal(), sigaction() stubs
│   │   ├── time.h              ← clock_gettime, struct timespec
│   │   └── limits.h            ← PATH_MAX, NAME_MAX, etc.
│   ├── src/
│   │   ├── crt0.asm            ← C runtime startup (_start → main → exit)
│   │   ├── syscall.asm         ← raw syscall wrappers (per-number)
│   │   ├── syscall.c           ← higher-level syscall helpers
│   │   ├── stdio.c             ← printf, puts, fgets + FILE* buffering
│   │   ├── stdlib.c            ← malloc, free (brk-based), exit, atoi
│   │   ├── string.c            ← memcpy, memset, strlen, strcmp, etc.
│   │   ├── unistd.c            ← wrappers: read, write, open, close, etc.
│   │   ├── dirent.c            ← opendir/readdir using getdents64
│   │   └── errno.c             ← errno storage
│   └── GNUmakefile             ← builds libcnexus.a
├── init/
│   ├── init.c                  ← PID 1: reads init.conf, spawns programs
│   └── GNUmakefile
└── shell/
    ├── shell.c                 ← interactive shell
    ├── builtins.c              ← cd, pwd, exit, echo, export
    ├── builtins.h
    └── GNUmakefile
```

---

## 3. Build System (`userspace/GNUmakefile`)

```makefile
# userspace/GNUmakefile — Build all userspace ELFs for NexusOS
# Usage: make all    → builds libc, init, shell
#        make clean  → removes build artifacts

TOOLCHAIN  ?= x86_64-elf-  # or just empty if host gcc targets x86_64
CC          = $(TOOLCHAIN)gcc
AS          = nasm
LD          = $(TOOLCHAIN)ld
AR          = $(TOOLCHAIN)ar

# Userspace compiler flags — NOT kernel flags (no -ffreestanding, etc.)
CFLAGS = -std=gnu11 -Wall -Wextra -Werror \
         -nostdlib -nostdinc \
         -I libc/include \
         -fno-stack-protector \
         -m64 -march=x86-64 \
         -mno-red-zone \
         -fno-pic -static

LDFLAGS = -static -nostdlib

BUILD_DIR = build

.PHONY: all clean libc init shell

all: libc init shell

libc:
	$(MAKE) -C libc

init: libc
	$(MAKE) -C init

shell: libc
	$(MAKE) -C shell

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C init clean
	$(MAKE) -C shell clean
	rm -rf $(BUILD_DIR)
```

**Cross-compilation note**: If building on Linux/WSL2, you may use system `gcc` targeting
`x86_64-elf`. If not available, use `gcc` directly since WSL2 is already x86_64.
Check with `gcc -dumpmachine` — should say `x86_64-linux-gnu`.
If so, you can use `-static` ELFs directly.

---

## 4. CRT0 and Syscall Foundation

### `libc/src/crt0.asm`

```nasm
; crt0.asm — C Runtime Entry Point
; On process entry (via kernel exec):
;   RSP = user stack pointer
;   Stack layout (from kernel): [argc] [argv[0]..argv[n] NULL] [envp[0]..envp[n] NULL]
;   RDI = argc (passed by kernel in register for convenience — check kernel impl)

[bits 64]
section .text
global _start
extern main
extern _exit
extern __libc_start_main

_start:
    ; At entry: rsp = user stack top
    ; Stack: [argc] [argv pointers] [NULL] [envp pointers] [NULL]
    xor rbp, rbp            ; ABI: zero frame pointer at entry

    pop rdi                  ; argc
    mov rsi, rsp             ; argv (pointer to array of char*)
    lea rdx, [rsp + rdi*8 + 8] ; envp (after argv + NULL)

    ; Initialize libc (sets up malloc, errno, etc.)
    call __libc_start_main   ; (argc, argv, envp) → calls main()

    ; Should not reach here
    mov rdi, 0
    call _exit
    ud2
```

### `libc/src/syscall.asm`

```nasm
; syscall.asm — Low-level system call wrappers
; Convention: arguments in rdi, rsi, rdx, rcx, r8, r9
; syscall ABI: args in rdi, rsi, rdx, r10, r8, r9 (r10 not rcx!)
; Return: rax (≥0=success, <0=-errno)

[bits 64]
section .text

; Macro for generating syscall wrappers
; Usage: DEFSYSCALL name, number
%macro DEFSYSCALL 2
global %1
%1:
    mov rax, %2       ; syscall number
    mov r10, rcx      ; 4th arg: ABI uses rcx, syscall uses r10
    syscall
    ; If rax < 0, set errno = -rax, return -1
    cmp rax, 0
    jge .ok_%1
    neg rax
    ; Store in errno (global variable)
    lea rcx, [rel errno]
    mov [rcx], eax
    mov rax, -1
.ok_%1:
    ret
%endmacro

; Core syscalls
DEFSYSCALL __syscall_read,     0
DEFSYSCALL __syscall_write,    1
DEFSYSCALL __syscall_open,     2
DEFSYSCALL __syscall_close,    3
DEFSYSCALL __syscall_stat,     4
DEFSYSCALL __syscall_fstat,    5
DEFSYSCALL __syscall_lseek,    8
DEFSYSCALL __syscall_mmap,     9
DEFSYSCALL __syscall_mprotect, 10
DEFSYSCALL __syscall_munmap,   11
DEFSYSCALL __syscall_brk,      12
DEFSYSCALL __syscall_exit,     60
DEFSYSCALL __syscall_fork,     57
DEFSYSCALL __syscall_execve,   59
DEFSYSCALL __syscall_wait4,    61
DEFSYSCALL __syscall_getpid,   39
DEFSYSCALL __syscall_getppid,  110
DEFSYSCALL __syscall_getuid,   102
DEFSYSCALL __syscall_getgid,   104
DEFSYSCALL __syscall_geteuid,  107
DEFSYSCALL __syscall_getegid,  108
DEFSYSCALL __syscall_getcwd,   79
DEFSYSCALL __syscall_chdir,    80
DEFSYSCALL __syscall_mkdir,    83
DEFSYSCALL __syscall_rmdir,    84
DEFSYSCALL __syscall_unlink,   87
DEFSYSCALL __syscall_dup,      32
DEFSYSCALL __syscall_dup2,     33
DEFSYSCALL __syscall_pipe,     22
DEFSYSCALL __syscall_ioctl,    16
DEFSYSCALL __syscall_uname,    63
DEFSYSCALL __syscall_kill,     62
DEFSYSCALL __syscall_getdents64, 217
DEFSYSCALL __syscall_clock_gettime, 228
DEFSYSCALL __syscall_arch_prctl,  158
DEFSYSCALL __syscall_set_tid_address, 218
DEFSYSCALL __syscall_openat,   257
DEFSYSCALL __syscall_exit_group, 231
DEFSYSCALL __syscall_tgkill,   234
DEFSYSCALL __syscall_rt_sigaction, 13
DEFSYSCALL __syscall_rt_sigprocmask, 14
DEFSYSCALL __syscall_access,   21
DEFSYSCALL __syscall_creat,    85
DEFSYSCALL __syscall_fcntl,    72
DEFSYSCALL __syscall_dup3,     292
```

---

## 5. libc Headers

### `libc/include/syscall.h`
```c
#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H
#include <stdint.h>

/* Raw syscall with N arguments. Do not call directly — use wrappers. */
long __syscall0(long num);
long __syscall1(long num, long a1);
long __syscall2(long num, long a1, long a2);
long __syscall3(long num, long a1, long a2, long a3);
long __syscall4(long num, long a1, long a2, long a3, long a4);
long __syscall5(long num, long a1, long a2, long a3, long a4, long a5);
long __syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6);

/* Syscall numbers (MUST match kernel/src/syscall/syscall.h exactly) */
#define SYS_READ      0
#define SYS_WRITE     1
#define SYS_OPEN      2
#define SYS_CLOSE     3
/* ... (full list from Agent B's syscall.h) ... */
#define SYS_EXIT      60
#define SYS_FORK      57
#define SYS_EXECVE    59
#endif
```

### `libc/include/unistd.h`
```c
#ifndef UNISTD_H
#define UNISTD_H
#include <sys/types.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     open(const char *path, int flags, ...);
int     close(int fd);
pid_t   fork(void);
int     execv(const char *path, char *const argv[]);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execvp(const char *file, char *const argv[]);
void    _exit(int status);
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
gid_t   getgid(void);
uid_t   geteuid(void);
gid_t   getegid(void);
int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int pipefd[2]);
int     unlink(const char *path);
int     rmdir(const char *path);
int     mkdir(const char *path, mode_t mode);
int     access(const char *path, int mode);
off_t   lseek(int fd, off_t offset, int whence);
int     usleep(unsigned int usec);     /* TODO: implement via clock_nanosleep */
unsigned int sleep(unsigned int sec);  /* TODO */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
```

### `libc/include/stdio.h`
```c
#ifndef STDIO_H
#define STDIO_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

typedef struct _FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int     printf(const char *fmt, ...);
int     fprintf(FILE *stream, const char *fmt, ...);
int     sprintf(char *buf, const char *fmt, ...);
int     snprintf(char *buf, size_t n, const char *fmt, ...);
int     vprintf(const char *fmt, va_list ap);
int     vfprintf(FILE *stream, const char *fmt, va_list ap);
int     vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int     puts(const char *s);
int     putchar(int c);
int     fputs(const char *s, FILE *stream);
int     fputc(int c, FILE *stream);
int     getchar(void);
char   *fgets(char *s, int size, FILE *stream);
int     fgetc(FILE *stream);
FILE   *fopen(const char *path, const char *mode);
int     fclose(FILE *fp);
size_t  fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t  fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int     fflush(FILE *stream);
int     feof(FILE *stream);
int     ferror(FILE *stream);
void    perror(const char *s);

#define EOF (-1)

#endif
```

### `libc/include/stdlib.h`
```c
#ifndef STDLIB_H
#define STDLIB_H
#include <stddef.h>

void   *malloc(size_t size);
void   *calloc(size_t nmemb, size_t size);
void   *realloc(void *ptr, size_t size);
void    free(void *ptr);
void    exit(int status);
void    abort(void);
int     atoi(const char *s);
long    atol(const char *s);
long long atoll(const char *s);
double  atof(const char *s);               /* TODO: floating point */
char   *getenv(const char *name);
int     putenv(char *string);
int     setenv(const char *name, const char *value, int overwrite);
int     unsetenv(const char *name);
int     system(const char *command);       /* TODO: fork+exec */
char   *realpath(const char *path, char *resolved); /* TODO */
int     abs(int j);
long    labs(long j);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
```

---

## 6. malloc Implementation (`libc/src/stdlib.c`)

Use `brk`-based allocator (sbrk-equivalent):

```c
/* Simple free-list malloc using brk syscall */

static uint64_t heap_base = 0;
static uint64_t heap_cur  = 0;

static void *libc_sbrk(intptr_t increment) {
    if (heap_base == 0) {
        heap_base = (uint64_t)__syscall1(SYS_BRK, 0); /* get current brk */
        heap_cur  = heap_base;
    }
    uint64_t new_end = heap_cur + (uint64_t)increment;
    uint64_t result  = (uint64_t)__syscall1(SYS_BRK, (long)new_end);
    if (result < new_end) return (void *)-1; /* OOM */
    void *old = (void *)heap_cur;
    heap_cur = new_end;
    return old;
}

/* Header prepended to each allocation */
typedef struct malloc_header {
    size_t             size;    /* usable size (excludes header) */
    int                free;    /* 1 = free, 0 = in use */
    struct malloc_header *next;
} malloc_header_t;

static malloc_header_t *heap_head = NULL;

void *malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15ULL; /* 16-byte align */

    /* Search free list first */
    malloc_header_t *h = heap_head;
    while (h) {
        if (h->free && h->size >= size) {
            h->free = 0;
            return (void *)(h + 1);
        }
        h = h->next;
    }

    /* Extend heap */
    malloc_header_t *new = libc_sbrk(sizeof(malloc_header_t) + size);
    if (new == (void *)-1) return NULL;
    new->size = size;
    new->free = 0;
    new->next = heap_head;
    heap_head = new;
    return (void *)(new + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    malloc_header_t *h = (malloc_header_t *)ptr - 1;
    h->free = 1;
    /* TODO: coalescing */
}

void *calloc(size_t nmemb, size_t size) {
    void *p = malloc(nmemb * size);
    if (p) memset(p, 0, nmemb * size);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    malloc_header_t *h = (malloc_header_t *)ptr - 1;
    if (h->size >= size) return ptr;
    void *new = malloc(size);
    if (!new) return NULL;
    memcpy(new, ptr, h->size);
    free(ptr);
    return new;
}
```

---

## 7. printf Implementation (`libc/src/stdio.c`)

Implement `vsnprintf` fully, then define everything in terms of it:
- Support: `%d`, `%i`, `%u`, `%x`, `%X`, `%o`, `%s`, `%c`, `%p`, `%l`, `%ll`, `%zu`, `%%`
- Width + padding: `%5d`, `%-10s`, `%08x`
- `printf` → `vsnprintf` + `write(1, ...)`
- `fprintf` → `vsnprintf` + `write(fd, ...)`
- `FILE *stdout` = internal struct wrapping fd=1; `stdin` wraps 0; `stderr` wraps 2
- `fopen` → open() syscall, `fread` → read(), `fwrite` → write()

---

## 8. Init Process (`userspace/init/init.c`)

```c
/* init.c — NexusOS Init Process (PID 1)
 * Reads /etc/nexus/init.conf to determine programs to spawn.
 * Provides a fallback shell if no config found.
 * Reaps zombie children (wait loop).
 * Design allows future: runlevels, service supervision, multi-user login.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

#define INIT_CONF_PATH   "/etc/nexus/init.conf"
#define DEFAULT_SHELL     "/bin/nsh"
#define MAX_SERVICES      32

/* Service entry */
typedef struct {
    char path[256];      /* executable path */
    char args[512];      /* space-separated args */
    int  respawn;        /* 1 = restart on exit */
    pid_t pid;           /* current PID (0 = not running) */
} service_t;

static service_t services[MAX_SERVICES];
static int service_count = 0;

static void parse_init_conf(void) {
    FILE *f = fopen(INIT_CONF_PATH, "r");
    if (!f) {
        fprintf(stderr, "init: no %s, starting default shell\n", INIT_CONF_PATH);
        /* Default: start /bin/nsh */
        strcpy(services[0].path, DEFAULT_SHELL);
        services[0].args[0] = '\0';
        services[0].respawn = 1;
        service_count = 1;
        return;
    }
    char line[768];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* Format: [respawn|once] /path/to/binary [args...] */
        service_t *s = &services[service_count];
        char mode[16];
        if (sscanf(line, "%15s %255s %511[^\n]", mode, s->path, s->args) >= 2) {
            s->respawn = (strcmp(mode, "respawn") == 0) ? 1 : 0;
            service_count++;
            if (service_count >= MAX_SERVICES) break;
        }
    }
    fclose(f);
}

static pid_t start_service(service_t *s) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        /* Build argv from s->args */
        char *argv[64];
        argv[0] = s->path;
        int argc = 1;
        char *tok = strtok(s->args, " ");
        while (tok && argc < 63) { argv[argc++] = tok; tok = strtok(NULL, " "); }
        argv[argc] = NULL;
        execv(s->path, argv);
        fprintf(stderr, "init: exec %s failed\n", s->path);
        _exit(1);
    }
    return pid;
}

int main(void) {
    printf("NexusOS init (PID %d) starting...\n", getpid());

    /* Set up /etc/nexus directory */
    mkdir("/etc", 0755);
    mkdir("/etc/nexus", 0755);
    mkdir("/bin", 0755);
    mkdir("/usr", 0755);
    mkdir("/usr/bin", 0755);

    parse_init_conf();

    /* Start all services */
    for (int i = 0; i < service_count; i++) {
        services[i].pid = start_service(&services[i]);
        printf("init: started %s (PID %d)\n", services[i].path, services[i].pid);
    }

    /* Main loop: reap zombies, respawn services */
    for (;;) {
        int status;
        pid_t dead = waitpid(-1, &status, 0);  /* wait for any child */
        if (dead < 0) continue;

        /* Find which service died */
        for (int i = 0; i < service_count; i++) {
            if (services[i].pid == dead) {
                printf("init: service %s (PID %d) exited with %d\n",
                       services[i].path, dead, WEXITSTATUS(status));
                services[i].pid = 0;
                if (services[i].respawn) {
                    printf("init: respawning %s\n", services[i].path);
                    services[i].pid = start_service(&services[i]);
                }
                break;
            }
        }
    }
}
```

### `init.conf` documentation (in the file as comments)

Create `/etc/nexus/init.conf`:
```conf
# NexusOS init.conf — Service Configuration
# Format: [respawn|once] /path/to/binary [arguments]
#
# respawn — restart service automatically if it exits
# once    — run once; do not restart
#
# Lines starting with # are comments.
# Empty lines are ignored.
#
# Example:
#   respawn /bin/nsh    ← Start NexusOS shell, respawn on exit
#   once    /bin/setup  ← Run once during boot

respawn /bin/nsh
```

---

## 9. Userspace Shell (`userspace/shell/shell.c`)

```c
/* shell.c — NexusOS Userspace Shell (nsh)
 * Compatible with basic POSIX sh subset.
 * Runs as a userspace process — no kernel privileges.
 * All I/O via syscalls (read/write on stdin/stdout).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "builtins.h"

#define MAX_ARGS     64
#define MAX_INPUT    1024
#define PROMPT_MAX   64

/* Read shell config from /etc/nexus/shell.conf */
typedef struct {
    char prompt[PROMPT_MAX];    /* Default: "nexus$ " */
    int  history_size;          /* Max history lines */
    int  echo_commands;         /* Echo each command before exec */
} shell_config_t;

static shell_config_t shell_cfg = {
    .prompt = "nexus\\w$ ",
    .history_size = 100,
    .echo_commands = 0,
};

static void load_shell_conf(void) {
    FILE *f = fopen("/etc/nexus/shell.conf", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        char key[64], val[192];
        if (sscanf(line, "%63s = %191[^\n]", key, val) == 2) {
            if (strcmp(key, "prompt") == 0) strncpy(shell_cfg.prompt, val, PROMPT_MAX-1);
            if (strcmp(key, "history_size") == 0) shell_cfg.history_size = atoi(val);
        }
    }
    fclose(f);
}

static void print_prompt(void) {
    /* Expand \w to CWD */
    char cwd[256] = "/";
    getcwd(cwd, sizeof(cwd));
    /* Simple prompt: just print cwd */
    printf("[nexus:%s]$ ", cwd);
    fflush(stdout);
}

static int parse_args(char *line, char **argv) {
    int argc = 0;
    char *tok = strtok(line, " \t");
    while (tok && argc < MAX_ARGS - 1) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t");
    }
    argv[argc] = NULL;
    return argc;
}

static void exec_command(char **argv, int argc,
                          int in_fd, int out_fd) {
    /* Check builtins first */
    if (builtin_run(argv, argc) == 0) return;  /* was a builtin */

    /* External command */
    pid_t pid = fork();
    if (pid == 0) {
        /* Setup redirected FDs if needed */
        if (in_fd != STDIN_FILENO)  { dup2(in_fd, STDIN_FILENO);  close(in_fd); }
        if (out_fd != STDOUT_FILENO){ dup2(out_fd, STDOUT_FILENO); close(out_fd); }

        /* Try /bin/<cmd>, then /usr/bin/<cmd>, then direct path */
        char path[256];
        if (argv[0][0] != '/') {
            snprintf(path, sizeof(path), "/bin/%s", argv[0]);
            execv(path, argv);
            snprintf(path, sizeof(path), "/usr/bin/%s", argv[0]);
            execv(path, argv);
        }
        execv(argv[0], argv);
        fprintf(stderr, "nsh: %s: command not found\n", argv[0]);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* Handle pipe: "cmd1 | cmd2" */
static void handle_pipeline(char *line) {
    char *pipe_pos = strchr(line, '|');
    if (!pipe_pos) {
        /* No pipe */
        char *argv[MAX_ARGS];
        int   argc;
        /* Handle I/O redirection: > and < */
        int out_fd = STDOUT_FILENO;
        char *redir = strchr(line, '>');
        if (redir) {
            *redir = '\0';
            char *fname = redir + 1;
            while (*fname == ' ') fname++;
            fname[strcspn(fname, " \n")] = '\0';
            out_fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        argc = parse_args(line, argv);
        if (argc > 0) exec_command(argv, argc, STDIN_FILENO, out_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);
        return;
    }

    /* Two-stage pipeline for now (extend to N later) */
    *pipe_pos = '\0';
    char *cmd1 = line;
    char *cmd2 = pipe_pos + 1;

    int pipefd[2];
    pipe(pipefd);

    char *argv1[MAX_ARGS], *argv2[MAX_ARGS];
    int argc1 = parse_args(cmd1, argv1);
    int argc2 = parse_args(cmd2, argv2);

    /* Fork first command → writes to pipe */
    pid_t p1 = fork();
    if (p1 == 0) {
        close(pipefd[0]);
        exec_command(argv1, argc1, STDIN_FILENO, pipefd[1]);
        close(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);

    /* Fork second command → reads from pipe */
    pid_t p2 = fork();
    if (p2 == 0) {
        exec_command(argv2, argc2, pipefd[0], STDOUT_FILENO);
        close(pipefd[0]);
        _exit(0);
    }
    close(pipefd[0]);

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    load_shell_conf();

    char line[MAX_INPUT];
    for (;;) {
        print_prompt();
        if (!fgets(line, sizeof(line), stdin)) break;

        /* Strip trailing newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (line[0] == '\0') continue;

        handle_pipeline(line);
    }
    return 0;
}
```

### `builtins.c`
```c
/* builtins.c — Shell built-in commands (cd, pwd, exit, echo, etc.) */

#include "builtins.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Returns 0 if builtin was handled, 1 if not a builtin */
int builtin_run(char **argv, int argc) {
    if (argc == 0) return 1;

    if (strcmp(argv[0], "cd") == 0) {
        const char *dir = argc > 1 ? argv[1] : "/";
        if (chdir(dir) != 0) perror("cd");
        return 0;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd))) puts(cwd);
        return 0;
    }
    if (strcmp(argv[0], "exit") == 0) {
        exit(argc > 1 ? atoi(argv[1]) : 0);
    }
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            printf("%s%s", argv[i], i+1 < argc ? " " : "");
        }
        puts("");
        return 0;
    }
    if (strcmp(argv[0], "export") == 0) {
        /* TODO: environment variable management */
        return 0;
    }
    if (strcmp(argv[0], "uname") == 0) {
        puts("NexusOS 0.2.0 nexus x86_64 GNU/Linux");
        return 0;
    }
    return 1;  /* not a builtin */
}
```

---

## 10. Config Files

### `userspace/config/init.conf`
```conf
# NexusOS init.conf — Initial Service Configuration
# Generated by NexusOS installer. Edit to customize boot services.
# If this file is deleted or damaged, recreate it using the format:
#
#   [respawn|once] /path/to/binary [arguments]
#
# respawn: service restarts automatically on exit
# once: service runs once and is not restarted
#
# Default configuration — minimal shell on console:
respawn /bin/nsh
```

### `userspace/config/shell.conf`
```conf
# NexusOS shell.conf — Shell Configuration
# If this file is deleted, the shell uses compiled defaults.
# Recreate with the following documented keys:
#
# prompt = <string>
#   Shell prompt string. Supported escapes:
#   \w = current working directory
#   \u = current user (TODO: when users implemented)
#   \h = hostname
#   Default: nexus\w$
#
# history_size = <number>
#   Number of history entries to keep. Default: 100
#
# echo_commands = <0|1>
#   Echo each command before execution. Default: 0

prompt = nexus:\w$ 
history_size = 100
echo_commands = 0
```

---

## 11. Build Verification

```bash
# From repo root (WSL2):
cd userspace
make all

# Expected output:
# → build/libcnexus.a
# → build/init      (ELF64 static)
# → build/nsh       (ELF64 static)

# Verify ELF:
file build/init   # should say: ELF 64-bit LSB executable, x86-64, statically linked

# Check size stays reasonable:
ls -lh build/
```

---

## 12. Integration with Kernel

You create the ELF binaries. Agent B's `init_process_start()` loads `/init`.

For the kernel to find your init binary:
1. The kernel's ramfs contains `/`, `/dev`, `/tmp`, `/mnt`, `/bin`, `/etc`
2. Your `init` binary needs to be embedded into the ISO or loaded from disk
3. For Phase 4 QEMU testing: embed init binary into kernel as a byte array (simplest)
   OR use a ramfs image (more correct)

**Simplest approach for testing**: 
```c
/* In Agent B's kernel-side code: */
/* Embed the init ELF as an include: */
static uint8_t init_elf_data[] = {
    /* xxd -i userspace/build/init */
    ...
};
/* Write to ramfs: */
vfs_node_t *f = vfs_create("/init", VFS_FILE);
vfs_write(f, init_elf_data, sizeof(init_elf_data), 0);
```

This is a coordination point — tell Agent B how to embed your binary. Preferred: link as Limine module (see Limine module request in `limine_requests.c`).

**Better approach**: Add a Limine `MODULE_REQUEST` for `/init` binary, boot from disk.

---

## 13. libc README (documentation)

Create `userspace/libc/README.md`:
```markdown
# NexusOS libc — Status and glibc Compatibility Map

## Design
- Minimal, expandable to full glibc
- Syscall wrappers in NASM (syscall.asm) for exact Linux ABI
- All unimplemented functions documented with TODO
- Headers mirror glibc layout for drop-in compatibility

## Current Status

| Header | Status | Notes |
|---|---|---|
| stdint.h | COMPLETE | All integer typedefs |
| stddef.h | COMPLETE | size_t, NULL, offsetof |
| stdbool.h | COMPLETE | bool, true, false |
| string.h | COMPLETE | All standard functions |
| stdlib.h | PARTIAL | malloc/free/exit done; floating point TODO |
| stdio.h | PARTIAL | printf/fgets done; fopen/fclose partial |
| unistd.h | PARTIAL | Core syscalls done; symlinks TODO |
| sys/types.h | COMPLETE | pid_t, uid_t, gid_t, etc |
| errno.h | COMPLETE | errno + constants |
| sys/mman.h | PARTIAL | anonymous mmap; file-backed TODO |
| sys/wait.h | PARTIAL | waitpid; WNOHANG TODO |
| signal.h | STUB | sigaction stub; signals Phase 5 |
| dirent.h | PARTIAL | readdir via getdents64 |
| time.h | PARTIAL | clock_gettime done; strftime TODO |

## Path to Full glibc
1. Implement floating point functions (math.h) — libm
2. Implement locale support (locale.h)
3. Implement threading (pthread.h) — after SMP support
4. Implement dynamic linking (dlopen) — after ELF dynamic loader
5. Implement stdio buffering (currently unbuffered)
6. Full signal handling — after kernel signals phase
```

---

## 14. Memory Efficiency Requirements (MANDATORY)

> Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` BEFORE implementing.

### Build Flags (MANDATORY in all Makefiles)

```makefile
# Add to all CFLAGS:
CFLAGS += -Os                    # optimize for size (not speed)
CFLAGS += -ffunction-sections    # put each function in its own section
CFLAGS += -fdata-sections        # put each variable in its own section

# Add to LDFLAGS:
LDFLAGS += --gc-sections         # remove unreferenced sections (dead code elimination)
LDFLAGS += -s                    # strip symbol table (save ~50% binary size)
```

### malloc — Coalescing Required

The malloc in section 6 has a TODO for coalescing. **You must implement it**:

```c
void free(void *ptr) {
    if (!ptr) return;
    malloc_header_t *h = (malloc_header_t *)ptr - 1;
    h->free = 1;

    /* Coalesce: merge adjacent free blocks to prevent fragmentation */
    malloc_header_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            /* Merge cur and cur->next */
            cur->size += sizeof(malloc_header_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}
```

Without coalescing, repeated malloc/free causes heap to grow unboundedly. This is critical for init (which forks many times).

### stdio — Buffered I/O

Implement `FILE*` with a 1 KB write buffer (not unbuffered):
```c
typedef struct _FILE {
    int      fd;
    char     buf[LIBC_STDIO_BUFFER_SIZE];  /* 1024 bytes from nexus_config.h */
    size_t   buf_pos;
    size_t   buf_len;
    int      error;
    int      eof;
} FILE;
```
Flush buffer on: newline (line-buffered stdout), fflush(), fclose(), buffer full.
**Why**: Each unbuffered `putchar('x')` = 1 syscall. With buffer: 1024 chars = 1 syscall. Massive performance win.

### libc — One Function Per Translation Unit

Directory layout must be:
```
libc/src/string/
├── memcpy.c    ← ONLY memcpy here
├── memset.c    ← ONLY memset here
├── strlen.c    ← ONLY strlen here
├── strcmp.c    ← ONLY strcmp here
└── ...
```
Not `string.c` with all functions. **Why**: linker can eliminate individual functions unused by the program. With a single string.c, if you call only `strlen`, you still pull in `strcmp`, `memcpy`, etc. This is the musl philosophy.

### init — Minimal Memory Footprint

```c
/* In init.c: use fixed-size service table, no dynamic allocation */
static service_t services[MAX_SERVICES];  /* 32 × ~784B = ~25 KB BSS */
/* This is static. Good — no heap fragmentation from service management. */

/* BUT: reduce service_t path+args buffer size: */
typedef struct {
    char  path[128];   /* was 256 — most paths are short */
    char  args[256];   /* was 512 — reduce */
    int   respawn;
    pid_t pid;
} service_t;
/* 32 services × ~396B = ~12 KB BSS. Acceptable. */
```

### Shell — Avoid Heap in Hot Path

Shell's line input buffer is stack-allocated (1 KB). Good — no malloc in the read loop.
Avoid calling malloc in the main shell loop at all. Use stack-allocated argv arrays.

### Binary Size Targets

After build: verify sizes:
```bash
ls -lh build/init build/nsh
# init:  ≤ 50 KB
# nsh:   ≤ 100 KB
# If larger: check for dead code, run nm --print-size build/init | sort -k2 -rh
# to find the largest symbols — eliminate or reduce.
```

### Memory Budget (document in userspace.md):

| Component | RAM Usage | Notes |
|---|---|---|
| init BSS (static services[32]) | ~12 KB | stack-allocated not heap |
| init heap (during operation) | < 4 KB | config parsing only |
| nsh stack frame (during execution) | < 64 KB | line buf + argv on stack |
| nsh heap (during operation) | < 8 KB | dynamic commands only |
| Shared code pages (fork) | ~0 extra | COW-shared between processes |
| **Total init+nsh at runtime** | **< 200 KB physical** | (after COW sharing) |

---

## 15. Documentation Required (write LAST)

Create `docs/phase4/userspace.md`:
- libc architecture (header layout, asm layer, C layer)
- syscall wrapper generation via NASM macro
- malloc implementation (brk-based, free list)
- crt0 startup sequence
- init process: config format, service lifecycle
- shell: parsing, pipeline, builtins, external commands
- Config files: purpose, format, recovery instructions
- How to add new userspace programs
- How to add new libc functions
- Future: threading, dynamic linking, full glibc

---

## 15. Starting Agent Prompt

```
You are an App Agent implementing NexusOS Phase 4 Tasks 4.5 (minimal libc)
and 4.6 (init process + userspace shell).

WAIT CONDITION: Do not start until you confirm Agent B is complete.
Check: kernel/src/syscall/syscall.h exists.

READ FIRST (mandatory, in order):
1. planned_implementations/AGENT_C_USERSPACE.md (this document — full spec)
2. AGENTS.md
3. knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md
4. planned_implementations/AGENT_B_SYSCALL.md (understand syscall numbers)
5. kernel/src/syscall/syscall.h (exact syscall numbers from Agent B)
6. kernel/src/shell/shell_core.c (existing kernel shell, REFERENCE ONLY)
7. knowledge_items/nexusos-progress-report/artifacts/progress_report.md

YOUR TASK:
Build the NexusOS userspace as specified in AGENT_C_USERSPACE.md.
Create: userspace/libc/, userspace/init/, userspace/shell/, userspace/GNUmakefile.
Create config files: userspace/config/init.conf, userspace/config/shell.conf.

RULES:
- You own ONLY the userspace/ directory. Never touch kernel source.
- All libc functions: use syscall wrappers in syscall.asm (Linux ABI)
- Unimplemented functions: compile-time stub returning -1 + setting ENOSYS
  (comment: /* TODO: full glibc impl */)
- Build: static ELFs only. No dynamic linking in Phase 4.
- Every header: mirrors glibc naming for future compat
- Config files: self-documenting with comments explaining every key
- NEVER hardcode paths — all paths via #define constants or config

BUILD VERIFICATION:
  cd userspace && make all
  file build/init  # must be ELF 64-bit LSB executable
  file build/nsh   # same

WHEN DONE:
1. Write docs/phase4/userspace.md
2. Write userspace/libc/README.md
3. Coordinate with Agent B: tell them where the init binary is for embedding
4. Report: files created, binary sizes, test results
5. Signal: "AGENT C COMPLETE — userspace/build/init ready"
```
