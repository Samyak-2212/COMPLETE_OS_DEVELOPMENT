/* syscall_handlers.h */

#ifndef NEXUS_SYSCALL_HANDLERS_H
#define NEXUS_SYSCALL_HANDLERS_H

#include <stdint.h>

int64_t sys_read(uint64_t fd, uint64_t buf_user, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_write(uint64_t fd, uint64_t buf_user, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_open(uint64_t path_user, uint64_t flags, uint64_t mode, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_stat(uint64_t path_user, uint64_t statbuf_user, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_fstat(uint64_t fd, uint64_t statbuf_user, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset);
int64_t sys_mprotect(uint64_t addr, uint64_t len, uint64_t prot, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_munmap(uint64_t addr, uint64_t length, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_brk(uint64_t new_brk, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_execve(uint64_t path_user, uint64_t argv_user, uint64_t envp_user, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_wait4(uint64_t pid, uint64_t status_user, uint64_t options, uint64_t rusage_user, uint64_t a5, uint64_t a6);
int64_t sys_waitid(uint64_t idtype, uint64_t id, uint64_t infop_user, uint64_t options, uint64_t rusage_user, uint64_t a6);
int64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_getppid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_getuid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_getgid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_geteuid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_getegid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_getcwd(uint64_t buf_user, uint64_t size, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_chdir(uint64_t path_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_mkdir(uint64_t path_user, uint64_t mode, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_rmdir(uint64_t path_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_unlink(uint64_t path_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_dup(uint64_t oldfd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_pipe(uint64_t pipefd_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_ioctl(uint64_t fd, uint64_t cmd, uint64_t arg, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_uname(uint64_t buf_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_kill(uint64_t pid, uint64_t sig, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_gettid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_set_tid_address(uint64_t tidptr_user, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_arch_prctl(uint64_t code, uint64_t addr, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_clock_gettime(uint64_t clk_id, uint64_t tp_user, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_exit_group(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_tgkill(uint64_t tgid, uint64_t tid, uint64_t sig, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_openat(uint64_t dirfd, uint64_t path_user, uint64_t flags, uint64_t mode, uint64_t a5, uint64_t a6);
int64_t sys_getdents64(uint64_t fd, uint64_t dirent_user, uint64_t count, uint64_t a4, uint64_t a5, uint64_t a6);
int64_t sys_rt_sigaction(uint64_t signum, uint64_t act_user, uint64_t oldact_user, uint64_t sigsetsize, uint64_t a5, uint64_t a6);
int64_t sys_rt_sigprocmask(uint64_t how, uint64_t set_user, uint64_t oldset_user, uint64_t sigsetsize, uint64_t a5, uint64_t a6);

#endif /* NEXUS_SYSCALL_HANDLERS_H */
