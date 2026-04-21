#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syscall.h>
#include <stdint.h>

static uint64_t heap_base = 0;
static uint64_t heap_cur  = 0;

static void *libc_sbrk(intptr_t increment) {
    if (heap_base == 0) {
        heap_base = (uint64_t)__syscall1(SYS_BRK, 0); /* get current brk */
        heap_cur  = heap_base;
    }
    uint64_t new_end = heap_cur + (uint64_t)increment;
    uint64_t result  = (uint64_t)__syscall1(SYS_BRK, (long)new_end);
    if (result < new_end) return (void *)-1; /* OOM */
    void *old = (void *)heap_cur;
    heap_cur = new_end;
    return old;
}

/* Header prepended to each allocation */
typedef struct malloc_header {
    size_t             size;    /* usable size (excludes header) */
    int                free;    /* 1 = free, 0 = in use */
    struct malloc_header *next;
} malloc_header_t;

static malloc_header_t *heap_head = NULL;

void *malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15ULL; /* 16-byte align */

    /* Search free list first */
    malloc_header_t *h = heap_head;
    while (h) {
        if (h->free && h->size >= size) {
            h->free = 0;
            return (void *)(h + 1);
        }
        h = h->next;
    }

    /* Extend heap */
    malloc_header_t *new = libc_sbrk(sizeof(malloc_header_t) + size);
    if (new == (void *)-1) return NULL;
    new->size = size;
    new->free = 0;
    new->next = heap_head;
    heap_head = new;
    return (void *)(new + 1);
}

void free(void *ptr) {
    if (!ptr) return;
    malloc_header_t *h = (malloc_header_t *)ptr - 1;
    h->free = 1;
    /* TODO: coalescing */
}

void *calloc(size_t nmemb, size_t size) {
    void *p = malloc(nmemb * size);
    if (p) memset(p, 0, nmemb * size);
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    malloc_header_t *h = (malloc_header_t *)ptr - 1;
    if (h->size >= size) return ptr;
    void *new = malloc(size);
    if (!new) return NULL;
    memcpy(new, ptr, h->size);
    free(ptr);
    return new;
}

void exit(int status) {
    _exit(status);
}

void abort(void) {
    _exit(1);
}

int atoi(const char *s) {
    int res = 0;
    int sign = 1;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

long atol(const char *s) {
    return (long)atoi(s);
}

long long atoll(const char *s) {
    return (long long)atoi(s);
}

int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }

/* Env setup is normally done by crt0, but stub it out for now */
char **environ = NULL;

char *getenv(const char *name) {
    if (!environ || !name) return NULL;
    size_t len = strlen(name);
    for (char **ep = environ; *ep; ep++) {
        if (!strncmp(*ep, name, len) && (*ep)[len] == '=')
            return &(*ep)[len + 1];
    }
    return NULL;
}

int system(const char *command) {
    if (!command) return 1;
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { "nsh", "-c", (char*)command, NULL };
        execv("/bin/nsh", argv);
        _exit(127);
    } else if (pid > 0) {
        /* FIXME waitpid */
        return 0; // waitpid(pid, &wstatus, 0); 
    }
    return -1;
}

extern int main(int argc, char **argv, char **envp);

/* This libc_start_main is called from crt0.asm */
void __libc_start_main(int argc, char **argv, char **envp) {
    environ = envp;
    
    exit(main(argc, argv, envp));
}
