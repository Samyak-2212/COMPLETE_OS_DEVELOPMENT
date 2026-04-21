/* syscall_table.c — Syscall Dispatch Table */

#include "syscall.h"
#include "syscall_handlers.h"
#include "lib/printf.h"
#include "sched/process.h"

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
        kprintf("[SYSCALL] Warn: Unknown syscall %llu from PID %d\n", 
                (unsigned long long)num,
                current_process ? current_process->pid : -1);
        return -ENOSYS;
    }
    /* log every syscall trace as per requirement */
    /* kprintf("[SYSCALL] dispatch num=%llu pid=%d\n", (unsigned long long)num, current_process ? current_process->pid : -1); */
    return syscall_table[num](a1, a2, a3, a4, a5, a6);
}
