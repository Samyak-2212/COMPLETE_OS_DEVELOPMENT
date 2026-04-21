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

#define SYS_RT_SIGACTION  13
#define SYS_RT_SIGPROCMASK 14
#define SYS_RT_SIGRETURN  15

#define SYS_IOCTL         16
#define SYS_PREAD64       17
#define SYS_PWRITE64      18
#define SYS_READV         19
#define SYS_WRITEV        20
#define SYS_ACCESS        21
#define SYS_PIPE          22
#define SYS_SELECT        23

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

#define SYS_NEXUS_DEBUG   512

#endif
