/* ============================================================================
 * string.c — NexusOS Kernel String Utilities
 * Purpose: Freestanding implementations of string/memory functions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "lib/string.h"

/* ── Memory operations ─────────────────────────────────────────────────── */

/* Copy n bytes from src to dest. Regions must not overlap. */
void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/* Fill n bytes of dest with byte value c. */
void *memset(void *dest, int c, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    uint8_t val = (uint8_t)c;
    for (size_t i = 0; i < n; i++) {
        d[i] = val;
    }
    return dest;
}

/* Compare n bytes of s1 and s2. Returns <0, 0, or >0. */
int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}

/* Copy n bytes, handling overlapping regions correctly. */
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

/* ── String operations ─────────────────────────────────────────────────── */

/* Return length of null-terminated string s. */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* Compare two null-terminated strings. */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

/* Compare at most n characters of two strings. */
int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

/* Copy src to dest (including null terminator). */
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}

/* Copy at most n characters from src to dest. Pads with nulls. */
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/* Append src to the end of dest. */
char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d != '\0') {
        d++;
    }
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}

/* Find first occurrence of character c in string s. */
char *strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s != '\0') {
        if (*s == ch) {
            return (char *)s;
        }
        s++;
    }
    return (ch == '\0') ? (char *)s : (char *)0;
}

/* Find last occurrence of character c in string s. */
char *strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last = (char *)0;
    while (*s != '\0') {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    if (ch == '\0') {
        return (char *)s;
    }
    return (char *)last;
}

/* Find first occurrence of needle in haystack. */
char *strstr(const char *haystack, const char *needle) {
    if (*needle == '\0') {
        return (char *)haystack;
    }
    size_t needle_len = strlen(needle);
    while (*haystack != '\0') {
        if (strncmp(haystack, needle, needle_len) == 0) {
            return (char *)haystack;
        }
        haystack++;
    }
    return (char *)0;
}
