/* ============================================================================
 * string.h — NexusOS Kernel String Utilities
 * Purpose: Freestanding string/memory functions (no libc)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_LIB_STRING_H
#define NEXUS_LIB_STRING_H

#include <stddef.h>
#include <stdint.h>

/* ── Memory operations ─────────────────────────────────────────────────── */

/* Copy n bytes from src to dest. Regions must not overlap. */
void *memcpy(void *dest, const void *src, size_t n);

/* Fill n bytes of dest with byte value c. */
void *memset(void *dest, int c, size_t n);

/* Compare n bytes of s1 and s2. Returns 0 if equal. */
int memcmp(const void *s1, const void *s2, size_t n);

/* Copy n bytes, handling overlapping regions correctly. */
void *memmove(void *dest, const void *src, size_t n);

/* ── String operations ─────────────────────────────────────────────────── */

/* Return length of null-terminated string s. */
size_t strlen(const char *s);

/* Compare two null-terminated strings. Returns 0 if equal. */
int strcmp(const char *s1, const char *s2);

/* Compare at most n characters of two strings. */
int strncmp(const char *s1, const char *s2, size_t n);

/* Copy src to dest (including null terminator). dest must be large enough. */
char *strcpy(char *dest, const char *src);

/* Copy at most n characters from src to dest. Pads with nulls if src < n. */
char *strncpy(char *dest, const char *src, size_t n);

/* Append src to the end of dest. */
char *strcat(char *dest, const char *src);

/* Find first occurrence of character c in string s. Returns NULL if not found. */
char *strchr(const char *s, int c);

/* Find last occurrence of character c in string s. Returns NULL if not found. */
char *strrchr(const char *s, int c);

/* Find first occurrence of needle in haystack. */
char *strstr(const char *haystack, const char *needle);

#endif /* NEXUS_LIB_STRING_H */
