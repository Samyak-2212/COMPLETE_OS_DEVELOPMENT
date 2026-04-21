#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>

struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

DIR *opendir(const char *name) {
    int fd = open(name, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return NULL;

    DIR *d = malloc(sizeof(DIR));
    if (!d) {
        close(fd);
        return NULL;
    }
    d->fd = fd;
    d->buf_pos = 0;
    d->buf_end = 0;
    return d;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp) return NULL;

    if (dirp->buf_pos >= dirp->buf_end) {
        long res = __syscall3(SYS_GETDENTS64, dirp->fd, (long)dirp->buffer, sizeof(dirp->buffer));
        if (res <= 0) return NULL;
        dirp->buf_pos = 0;
        dirp->buf_end = res;
    }

    struct linux_dirent64 *d64 = (struct linux_dirent64 *)(dirp->buffer + dirp->buf_pos);
    dirp->buf_pos += d64->d_reclen;

    static struct dirent d;
    d.d_ino = d64->d_ino;
    d.d_off = d64->d_off;
    d.d_reclen = d64->d_reclen;
    d.d_type = d64->d_type;
    strncpy(d.d_name, d64->d_name, sizeof(d.d_name) - 1);
    d.d_name[sizeof(d.d_name) - 1] = '\0';
    
    return &d;
}

int closedir(DIR *dirp) {
    if (!dirp) return -1;
    int r = close(dirp->fd);
    free(dirp);
    return r;
}
