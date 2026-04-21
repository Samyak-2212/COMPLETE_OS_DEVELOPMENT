#ifndef DIRENT_H
#define DIRENT_H

#include <sys/types.h>

struct dirent {
    ino_t          d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};

typedef struct {
    int fd;
    char buffer[4096];
    int buf_pos;
    int buf_end;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif
