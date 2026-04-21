/* ============================================================================
 * syscall_handlers.c — NexusOS System Call Handler Implementations
 * Purpose: Per-syscall logic. Unimplemented syscalls return -ENOSYS cleanly.
 * ============================================================================ */
#include "syscall.h"
#include "syscall_handlers.h"
#include "sched/scheduler.h"
#include "sched/process.h"
#include "fs/vfs.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "elf_loader.h"

/* Suppress unused-parameter warnings for stub syscall handlers.
 * Use at the top of any handler that ignores some or all of its 6 args. */
#define UNUSED_ARGS6(a,b,c,d,e,f) \
    (void)(a);(void)(b);(void)(c);(void)(d);(void)(e);(void)(f)

int64_t sys_write(uint64_t fd, uint64_t buf_user, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    if (buf_user == 0 || count == 0) return 0;
    if (count > 65536) count = 65536; /* cap */
    
    const char *buf = (const char *)buf_user;
    if (!current_process->fd_table) return -EBADF;
    if (fd >= 64 || current_process->fd_table->fds[fd] == NULL) return -EBADF;
    vfs_node_t *node = (vfs_node_t *)current_process->fd_table->fds[fd];
    return vfs_write(node, 0 /* ignoring seek for now */, count, (uint8_t*)buf);
}

int64_t sys_read(uint64_t fd, uint64_t buf_user, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    /* stub implementation for stdin/files */
    if (buf_user == 0 || count == 0) return 0;
    if (!current_process->fd_table) return -EBADF;
    if (fd >= 64 || current_process->fd_table->fds[fd] == NULL) return -EBADF;
    vfs_node_t *node = (vfs_node_t *)current_process->fd_table->fds[fd];
    return vfs_read(node, 0, count, (uint8_t*)buf_user);
}

int64_t sys_open(uint64_t path_user, uint64_t flags, uint64_t mode, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)mode; (void)a4; (void)a5; (void)a6;
    vfs_node_t *file = vfs_resolve_path((const char *)path_user);
    if (!file) {
        if (flags & 0x40 /* O_CREAT */) {
            file = vfs_create((const char *)path_user);
        }
        if (!file) return -ENOENT;
    }
    vfs_open(file);
    if (!current_process->fd_table) {
        current_process->fd_table = kcalloc(1, sizeof(fd_table_t));
    }
    for (int i = 3; i < 64; i++) {
        if (current_process->fd_table->fds[i] == NULL) {
            current_process->fd_table->fds[i] = file;
            current_process->fd_table->open_count++;
            return i;
        }
    }
    vfs_close(file);
    return -EMFILE;
}

int64_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!current_process->fd_table || fd >= 64 || current_process->fd_table->fds[fd] == NULL) return -EBADF;
    vfs_node_t *node = (vfs_node_t *)current_process->fd_table->fds[fd];
    vfs_close(node);
    current_process->fd_table->fds[fd] = NULL;
    current_process->fd_table->open_count--;
    return 0;
}

int64_t sys_stat(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_fstat(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_lseek(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }

int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset) {
    (void)prot; (void)fd; (void)offset;
    if (!(flags & MAP_ANONYMOUS)) return -ENOSYS;
    uint64_t num_pages = (length + 0xFFF) / 4096;
    uint64_t virt = addr ? addr : current_process->heap_end;
    current_process->heap_end = virt + num_pages * 4096;
    return (int64_t)virt;
}

int64_t sys_mprotect(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return 0; }
int64_t sys_munmap(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)   { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return 0; }

int64_t sys_brk(uint64_t new_brk, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (new_brk == 0) return (int64_t)current_process->heap_end;
    if (new_brk < current_process->heap_base) return -EINVAL;
    current_process->heap_end = (new_brk + 0xFFF) & ~0xFFFULL;
    return (int64_t)current_process->heap_end;
}

int64_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    scheduler_exit_current((int)code);
    return 0;
}

int64_t sys_exit_group(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    scheduler_exit_current((int)code);
    return 0;
}

/* NOTE: Full fork() with child thread requires thread_create_fork_child().
 * Phase 4 implementation creates the child PCB and COW address space.
 * The child thread's kernel stack frame setup is deferred to Phase 5. */

int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    UNUSED_ARGS6(a1, a2, a3, a4, a5, a6);
    process_t *child = process_create(current_process->pid);
    if (!child) return -ENOMEM;
    if (current_process->fd_table) {
        child->fd_table = kcalloc(1, sizeof(fd_table_t));
        if (child->fd_table)
            memcpy(child->fd_table, current_process->fd_table, sizeof(fd_table_t));
    }
    child->cr3 = vmm_cow_clone(current_process->cr3);
    if (!child->cr3) { process_destroy(child); return -ENOMEM; }

    child->heap_base       = current_process->heap_base;
    child->heap_end        = current_process->heap_end;
    child->stack_top_virt  = current_process->stack_top_virt;
    child->stack_limit_virt = current_process->stack_limit_virt;

    /* Child thread frame setup deferred: child returns 0 on first schedule */
    return child->pid;
}

int64_t sys_execve(uint64_t path_user, uint64_t argv_user, uint64_t envp_user,
                   uint64_t a4, uint64_t a5, uint64_t a6) {
    /* argv/envp parsing deferred to Phase 5 */
    (void)argv_user; (void)envp_user; (void)a4; (void)a5; (void)a6;

    vfs_node_t *file = vfs_resolve_path((const char *)path_user);
    if (!file) return -ENOENT;

    uint64_t entry_point = 0;
    int r = elf_load(current_process, file, &entry_point);
    vfs_close(file);
    if (r != 0) return -ENOEXEC;

    current_process->stack_top_virt   = 0x00007FFFFFFFF000ULL;
    current_process->stack_limit_virt = 0x00007FFFFFFFF000ULL;
    /* Full context replacement (overwrite saved RIP in syscall frame) is Phase 5 */
    return -ENOSYS; /* Needs context replacement logic from scheduler */
}

int64_t sys_wait4(uint64_t pid, uint64_t status_user, uint64_t options, uint64_t rusage_user, uint64_t a5, uint64_t a6) { 
    (void)options; (void)rusage_user; (void)a5; (void)a6;
    int status = 0;
    while (1) {
        pid_t ret = process_waitpid(current_process->pid, (pid_t)pid, &status);
        if (ret > 0) {
            if (status_user) {
                *(int*)status_user = status;
            }
            return ret;
        } else if (ret == -1) {
            return -ECHILD; /* No children to wait for */
        }
        /* ret == 0 means children exist, but no zombie. */
        if (options & 1 /* WNOHANG */) {
            return 0;
        }
        schedule();
    }
}
int64_t sys_waitid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->pid  : 0; }
int64_t sys_getppid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->ppid : 0; }
int64_t sys_getuid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->uid  : 0; }
int64_t sys_getgid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->gid  : 0; }
int64_t sys_geteuid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->euid : 0; }
int64_t sys_getegid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_process ? current_process->egid : 0; }

int64_t sys_getcwd(uint64_t buf_user, uint64_t size, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (!current_process->cwd) return -ENOENT;
    if (size < strlen(current_process->cwd) + 1) return -EINVAL;
    strcpy((char*)buf_user, current_process->cwd);
    return buf_user;
}
int64_t sys_chdir(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_mkdir(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_rmdir(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_unlink(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_dup(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)   { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_dup2(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_pipe(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_ioctl(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

int64_t sys_uname(uint64_t buf_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    if (!buf_user) return -EFAULT;
    struct utsname *u = (struct utsname *)buf_user;
    strcpy(u->sysname,    "NexusOS");
    strcpy(u->nodename,   "nexus");
    strcpy(u->release,    "0.1.0");
    strcpy(u->version,    "#1 SMP");
    strcpy(u->machine,    "x86_64");
    strcpy(u->domainname, "localdomain");
    return 0;
}

int64_t sys_kill(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)            { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_gettid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)          { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_thread ? (int64_t)current_thread->tid : 0; }
int64_t sys_set_tid_address(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return current_thread ? (int64_t)current_thread->tid : 0; }
int64_t sys_arch_prctl(uint64_t code, uint64_t addr, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a3; (void)a4; (void)a5; (void)a6;
    if (code == 0x1002) { /* ARCH_SET_FS */
        __asm__ volatile("wrmsr" : : "c"(0xC0000100U), "a"((uint32_t)addr), "d"((uint32_t)(addr >> 32)));
        return 0;
    }
    return -ENOSYS;
}
int64_t sys_clock_gettime(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_tgkill(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)        { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_openat(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)        { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_getdents64(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)    { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return -ENOSYS; }
int64_t sys_rt_sigaction(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6)  { UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return 0; }
int64_t sys_rt_sigprocmask(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6){ UNUSED_ARGS6(a1,a2,a3,a4,a5,a6); return 0; }
