#ifndef ERRNO_H
#define ERRNO_H

extern int errno;

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

#endif
