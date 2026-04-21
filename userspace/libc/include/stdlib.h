#ifndef STDLIB_H
#define STDLIB_H
#include <stddef.h>

void   *malloc(size_t size);
void   *calloc(size_t nmemb, size_t size);
void   *realloc(void *ptr, size_t size);
void    free(void *ptr);
void    exit(int status);
void    abort(void);
int     atoi(const char *s);
long    atol(const char *s);
long long atoll(const char *s);
double  atof(const char *s);               /* TODO: floating point */
char   *getenv(const char *name);
int     putenv(char *string);
int     setenv(const char *name, const char *value, int overwrite);
int     unsetenv(const char *name);
int     system(const char *command);       /* TODO: fork+exec */
char   *realpath(const char *path, char *resolved); /* TODO */
int     abs(int j);
long    labs(long j);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

#endif
