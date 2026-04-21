#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdint.h>

struct _FILE {
    int fd;
    int error;
    int eof;
};

static FILE _stdin  = { STDIN_FILENO,  0, 0 };
static FILE _stdout = { STDOUT_FILENO, 0, 0 };
static FILE _stderr = { STDERR_FILENO, 0, 0 };

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

/* Minimal vsnprintf */
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    if (n == 0) return 0;
    size_t count = 0;
    while (*fmt && count < n - 1) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 's') {
                char *s = va_arg(ap, char*);
                if (!s) s = "(null)";
                while (*s && count < n - 1) buf[count++] = *s++;
            } else if (*fmt == 'd' || *fmt == 'i') {
                int val = va_arg(ap, int);
                char num[32];
                int i = 0;
                unsigned int uval = val;
                if (val < 0) {
                    uval = -val;
                    if (count < n - 1) buf[count++] = '-';
                }
                do {
                    num[i++] = (uval % 10) + '0';
                    uval /= 10;
                } while (uval && i < 32);
                while (i > 0 && count < n - 1) buf[count++] = num[--i];
            } else if (*fmt == 'c') {
                char c = (char)va_arg(ap, int);
                buf[count++] = c;
            } else if (*fmt == '%') {
                buf[count++] = '%';
            }
            /* TODO: expand %x, %u, etc. as needed */
        } else {
            buf[count++] = *fmt;
        }
        fmt++;
    }
    buf[count] = '\0';
    return count;
}

int printf(const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(STDOUT_FILENO, buf, ret);
    return ret;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    if (!stream) return -1;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write(stream->fd, buf, ret);
    return ret;
}

int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, 1024*1024 /* essentially infinite */, fmt, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return ret;
}

int puts(const char *s) {
    int ret = write(STDOUT_FILENO, s, strlen(s));
    write(STDOUT_FILENO, "\n", 1);
    return ret + 1;
}

int putchar(int c) {
    char ch = c;
    return write(STDOUT_FILENO, &ch, 1);
}

int fputs(const char *s, FILE *stream) {
    return write(stream->fd, s, strlen(s));
}

int fputc(int c, FILE *stream) {
    char ch = c;
    return write(stream->fd, &ch, 1);
}

FILE *fopen(const char *path, const char *mode) {
    int flags = 0;
    if (strchr(mode, 'r') && strchr(mode, '+')) flags = O_RDWR;
    else if (strchr(mode, 'r')) flags = O_RDONLY;
    else if (strchr(mode, 'w') && strchr(mode, '+')) flags = O_RDWR | O_CREAT | O_TRUNC;
    else if (strchr(mode, 'w')) flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (strchr(mode, 'a') && strchr(mode, '+')) flags = O_RDWR | O_CREAT | O_APPEND;
    else if (strchr(mode, 'a')) flags = O_WRONLY | O_CREAT | O_APPEND;

    int fd = open(path, flags, 0666);
    if (fd < 0) return NULL;
    
    FILE *f = malloc(sizeof(FILE));
    if (!f) {
        close(fd);
        return NULL;
    }
    f->fd = fd;
    f->error = 0;
    f->eof = 0;
    return f;
}

int fclose(FILE *fp) {
    if (!fp) return -1;
    int r = close(fp->fd);
    if (fp != &_stdin && fp != &_stdout && fp != &_stderr) {
        free(fp);
    }
    return r;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream) return 0;
    ssize_t r = read(stream->fd, ptr, size * nmemb);
    if (r < 0) {
        stream->error = 1;
        return 0;
    }
    if (r == 0) stream->eof = 1;
    return r / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream) return 0;
    ssize_t r = write(stream->fd, ptr, size * nmemb);
    if (r < 0) {
        stream->error = 1;
        return 0;
    }
    return r / size;
}

int fflush(FILE *stream) {
    (void)stream;
    return 0; /* No userspace buffering yet */
}

char *fgets(char *s, int size, FILE *stream) {
    if (size <= 0 || !stream) return NULL;
    int i = 0;
    while (i < size - 1) {
        char c;
        ssize_t r = read(stream->fd, &c, 1);
        if (r <= 0) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fgetc(FILE *stream) {
    char c;
    if (read(stream->fd, &c, 1) <= 0) return EOF;
    return c;
}

int getchar(void) {
    return fgetc(stdin);
}

int feof(FILE *stream) {
    return stream ? stream->eof : 1;
}

int ferror(FILE *stream) {
    return stream ? stream->error : 1;
}

void perror(const char *s) {
    extern int errno;
    if (s && *s) {
        fprintf(stderr, "%s: Error %d\n", s, errno);
    } else {
        fprintf(stderr, "Error %d\n", errno);
    }
}
