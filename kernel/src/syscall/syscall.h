/* ===================================================================
 * syscall.h — NexusOS System Call Numbers
 * MUST match Linux x86_64 ABI exactly for binary compatibility.
 * Reference: arch/x86/entry/syscalls/syscall_64.tbl in Linux kernel.
 * =================================================================== */

#ifndef NEXUS_SYSCALL_H
#define NEXUS_SYSCALL_H

#include <stdint.h>

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

/* NexusOS extensions */
#define SYS_NEXUS_DEBUG   512

/* Errno constants */
#define EPERM     1    /* Operation not permitted */
#define ENOENT    2    /* No such file or directory */
#define ESRCH     3    /* No such process */
#define EINTR     4    /* Interrupted system call */
#define EIO       5    /* I/O error */
#define ENOEXEC   8    /* Exec format error */
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

/* mmap flags */
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define MAP_SHARED      0x01
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20

int syscall_init(void);

#endif /* NEXUS_SYSCALL_H */
