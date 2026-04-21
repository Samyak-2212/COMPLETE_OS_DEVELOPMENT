#ifndef STDIO_H
#define STDIO_H
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

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
