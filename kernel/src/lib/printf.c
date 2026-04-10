/* ============================================================================
 * printf.c — NexusOS Kernel Printf
 * Purpose: Formatted text output to framebuffer via character grid
 * Author: NexusOS Kernel Team
 *
 * Supports: %d, %i, %u, %x, %X, %p, %s, %c, %%, %ld, %lu, %lx, %lX,
 *           %lld, %llu, %llx, %llX, zero-padding (e.g. %08x), field width
 * ============================================================================ */

#include "lib/printf.h"
#include "lib/string.h"
#include "drivers/video/framebuffer.h"
#include <stdint.h>

#include "display/display_manager.h"
#include <stdarg.h>
#include "lib/klog.h"

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Print a single character by routing through display manager */
void kputchar(char c) {
    klog_putc(c);
    display_manager_write(&c, 1);
    serial_putc(c);
}

/* Print a null-terminated string */
void kputs(const char *s) {
    while (*s) {
        kputchar(*s++);
    }
}

/* Print an unsigned 64-bit integer in the given base (10 or 16) */
static void print_uint64(uint64_t value, int base, int uppercase,
                          int width, char pad) {
    char buf[20]; /* Max uint64 decimal: 20 digits */
    int pos = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (value == 0) {
        buf[pos++] = '0';
    } else {
        while (value > 0) {
            buf[pos++] = digits[value % base];
            value /= base;
        }
    }

    /* Pad to field width */
    while (pos < width) {
        buf[pos++] = pad;
    }

    /* Print in reverse */
    for (int i = pos - 1; i >= 0; i--) {
        kputchar(buf[i]);
    }
}

/* Print a signed 64-bit integer */
static void print_int64(int64_t value, int width, char pad) {
    if (value < 0) {
        kputchar('-');
        if (width > 0) width--;
        print_uint64((uint64_t)(-value), 10, 0, width, pad);
    } else {
        print_uint64((uint64_t)value, 10, 0, width, pad);
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Initialize the kprintf console. Now delegates to display manager. */
void kprintf_init(void) {
    klog_init();
    display_manager_init();
}

/* Set text colors via ANSI escape codes */
void kprintf_set_color(unsigned int fg, unsigned int bg) {
    (void)fg; (void)bg;
}

/* Kernel formatted print */
int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = 0;

    while (*fmt) {
        if (*fmt != '%') {
            kputchar(*fmt++);
            count++;
            continue;
        }

        fmt++; /* skip '%' */

        /* Parse flags */
        char pad = ' ';
        int left_align = 0;
        while (1) {
            if (*fmt == '0') pad = '0';
            else if (*fmt == '-') left_align = 1;
            else break;
            fmt++;
        }

        /* Parse field width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int length = 0; /* 0=default, 1=long, 2=long long */
        if (*fmt == 'l') {
            length = 1;
            fmt++;
            if (*fmt == 'l') {
                length = 2;
                fmt++;
            }
        }

        /* Parse conversion specifier */
        switch (*fmt) {
            case 'd':
            case 'i': {
                int64_t val;
                if (length == 2)      val = va_arg(args, int64_t);
                else if (length == 1) val = va_arg(args, long);
                else                  val = va_arg(args, int);
                print_int64(val, width, pad);
                break;
            }
            case 'u': {
                uint64_t val;
                if (length == 2)      val = va_arg(args, uint64_t);
                else if (length == 1) val = va_arg(args, unsigned long);
                else                  val = va_arg(args, unsigned int);
                print_uint64(val, 10, 0, width, pad);
                break;
            }
            case 'x': {
                uint64_t val;
                if (length == 2)      val = va_arg(args, uint64_t);
                else if (length == 1) val = va_arg(args, unsigned long);
                else                  val = va_arg(args, unsigned int);
                print_uint64(val, 16, 0, width, pad);
                break;
            }
            case 'X': {
                uint64_t val;
                if (length == 2)      val = va_arg(args, uint64_t);
                else if (length == 1) val = va_arg(args, unsigned long);
                else                  val = va_arg(args, unsigned int);
                print_uint64(val, 16, 1, width, pad);
                break;
            }
            case 'p': {
                uint64_t val = (uint64_t)(uintptr_t)va_arg(args, void *);
                kputs("0x");
                print_uint64(val, 16, 0, 16, '0');
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (s == (void *)0) s = "(null)";
                int len = (int)strlen(s);
                if (!left_align) {
                    while (len < width--) kputchar(pad);
                }
                kputs(s);
                if (left_align) {
                    while (len < width--) kputchar(pad);
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                kputchar(c);
                break;
            }
            case '%':
                kputchar('%');
                break;
            default:
                /* Unknown specifier — print literally */
                kputchar('%');
                if (left_align) kputchar('-');
                if (width > 0) {
                    /* This is a bit complex for a default, but better than nothing */
                    char wbuf[10];
                    int wpos = 0;
                    int tw = width;
                    while (tw > 0) { wbuf[wpos++] = (tw % 10) + '0'; tw /= 10; }
                    while (wpos > 0) kputchar(wbuf[--wpos]);
                }
                kputchar(*fmt);
                break;
        }
        fmt++;
    }

    va_end(args);
    return 0; /* Updated kprintf doesn't strictly track count for now */
}
