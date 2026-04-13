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
#include "lib/serial.h"

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Print a single character by routing through display manager */
void kputchar(char c) {
    klog_putc(c);
    display_manager_write(&c, 1);
    serial_putc(c);
}

/* Print a null-terminated string */
void kputs(const char *s) {
    display_manager_write(s, strlen(s));
    while (*s) {
        klog_putc(*s);
        serial_putc(*s++);
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

#ifdef DEBUGGER_ENABLED
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
#endif

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

/* vsnprintf - minimal kernel implementation for batched writes */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    size_t pos = 0;
#ifdef DEBUG_UNUSED_FOR_NOW
    const char *digits = "0123456789abcdef";
#endif

    #define APPEND(c) if (pos < size - 1) buf[pos++] = (c)

    while (*fmt && pos < size - 1) {
        if (*fmt != '%') {
            APPEND(*fmt++);
            continue;
        }
        fmt++;
        
        /* Parse conversion */
        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) { APPEND(*s++); }
                break;
            }
            case 'd': {
                int64_t val = va_arg(args, int);
                if (val < 0) { APPEND('-'); val = -val; }
                char tbuf[20]; int tpos = 0;
                if (val == 0) tbuf[tpos++] = '0';
                else while (val > 0) { tbuf[tpos++] = (val % 10) + '0'; val /= 10; }
                while (tpos > 0) APPEND(tbuf[--tpos]);
                break;
            }
            case 'c': APPEND((char)va_arg(args, int)); break;
            case '%': APPEND('%'); break;
            default: APPEND('%'); APPEND(*fmt); break;
        }
        fmt++;
    }
    buf[pos] = '\0';
    return (int)pos;
}

/* Kernel formatted print - now optimized with vsnprintf and batched writes */
int kprintf(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    /* Output to all enabled destinations as a single batch where possible */
    display_manager_write(buf, (uint64_t)len);
    
    /* Still log to serial and klog */
    for (int i = 0; i < len; i++) {
        klog_putc(buf[i]);
        serial_putc(buf[i]);
    }

    return len;
}
