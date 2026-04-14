/* ============================================================================
 * printf.h — NexusOS Kernel Printf
 * Purpose: kprintf() for formatted output to framebuffer console
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_LIB_PRINTF_H
#define NEXUS_LIB_PRINTF_H

#include <stdarg.h>
#include <stddef.h>
#include "drivers/video/framebuffer.h"

/* Initialize the kprintf console (must be called after framebuffer_init). */
void kprintf_init(void);

/* Kernel-level formatted print (supports %d, %u, %x, %X, %p, %s, %c, %%). */
int kprintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Print a single character to the console. */
void kputchar(char c);

/* Print a null-terminated string to the console. */
void kputs(const char *s);

/* Set text colors: fg and bg are 32-bit ARGB values. */
void kprintf_set_color(unsigned int fg, unsigned int bg);

#endif /* NEXUS_LIB_PRINTF_H */
