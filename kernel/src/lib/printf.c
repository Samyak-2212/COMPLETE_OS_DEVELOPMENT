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
#if defined(DEBUG_LEVEL) && DEBUG_LEVEL >= 1
#include "nexus_debug.h"
#endif

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Print a single character by routing through display manager */
void kputchar(char c) {
    klog_putc(c);
    display_manager_write(&c, 1);
#if defined(DEBUG_LEVEL) && DEBUG_LEVEL >= 1
    dbg_serial_putc(c);
#endif
}

/* Print a null-terminated string */
void kputs(const char *s) {
    display_manager_write(s, strlen(s));
    while (*s) {
        klog_putc(*s);
#if defined(DEBUG_LEVEL) && DEBUG_LEVEL >= 1
        dbg_serial_putc(*s);
#endif
        s++;
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
static void __attribute__((unused)) print_int64(int64_t value, int width, char pad) {
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

/* vsnprintf — full kernel implementation for batched writes
 * Supports: %d %i %u %x %X %p %s %c %%
 *           length mods: l (long), ll (long long)
 *           flags: zero-pad, field width (e.g. %02x, %04x, %016lx)
 */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    if (!buf || size == 0) return 0;
    size_t pos = 0;

    #define VSNPRINTF_APPEND(c) do { if (pos < size - 1) buf[pos++] = (c); } while(0)

    /* Helper: render uint64 into a local buffer, then APPEND in order */
    #define RENDER_UINT(val64, base, upper, width, pad) do {        \
        const char *_digs = (upper) ? "0123456789ABCDEF"            \
                                     : "0123456789abcdef";           \
        char _tb[20]; int _tp = 0;                                   \
        uint64_t _v = (val64);                                       \
        if (_v == 0) { _tb[_tp++] = '0'; }                          \
        else { while (_v) { _tb[_tp++] = _digs[_v % (base)];        \
                            _v /= (base); } }                        \
        while (_tp < (int)(width)) _tb[_tp++] = (pad);              \
        while (_tp > 0) VSNPRINTF_APPEND(_tb[--_tp]);                \
    } while(0)

    while (*fmt && pos < size - 1) {
        if (*fmt != '%') { VSNPRINTF_APPEND(*fmt++); continue; }
        fmt++; /* skip '%' */

        /* --- flag/width parsing --- */
        char pad_char = ' ';
        int  width    = 0;

        if (*fmt == '0') { pad_char = '0'; fmt++; }
        while (*fmt >= '1' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* --- length modifier --- */
        int len_mod = 0; /* 0=int, 1=long, 2=long long */
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { fmt++; len_mod = 2; }
            else              { len_mod = 1; }
        }

        /* --- conversion --- */
        switch (*fmt) {
            /* ── signed decimal ─────────────────────────────────────── */
            case 'd': case 'i': {
                int64_t val;
                if      (len_mod == 2) val = (int64_t)va_arg(args, long long);
                else if (len_mod == 1) val = (int64_t)va_arg(args, long);
                else                   val = (int64_t)va_arg(args, int);
                if (val < 0) { VSNPRINTF_APPEND('-'); val = -val; }
                RENDER_UINT((uint64_t)val, 10, 0, width, pad_char);
                break;
            }
            /* ── unsigned decimal ───────────────────────────────────── */
            case 'u': {
                uint64_t val;
                if      (len_mod == 2) val = (uint64_t)va_arg(args, unsigned long long);
                else if (len_mod == 1) val = (uint64_t)va_arg(args, unsigned long);
                else                   val = (uint64_t)(unsigned int)va_arg(args, unsigned int);
                RENDER_UINT(val, 10, 0, width, pad_char);
                break;
            }
            /* ── hex lowercase ──────────────────────────────────────── */
            case 'x': {
                uint64_t val;
                if      (len_mod == 2) val = (uint64_t)va_arg(args, unsigned long long);
                else if (len_mod == 1) val = (uint64_t)va_arg(args, unsigned long);
                else                   val = (uint64_t)(unsigned int)va_arg(args, unsigned int);
                RENDER_UINT(val, 16, 0, width, pad_char);
                break;
            }
            /* ── hex uppercase ──────────────────────────────────────── */
            case 'X': {
                uint64_t val;
                if      (len_mod == 2) val = (uint64_t)va_arg(args, unsigned long long);
                else if (len_mod == 1) val = (uint64_t)va_arg(args, unsigned long);
                else                   val = (uint64_t)(unsigned int)va_arg(args, unsigned int);
                RENDER_UINT(val, 16, 1, width, pad_char);
                break;
            }
            /* ── pointer ────────────────────────────────────────────── */
            case 'p': {
                uint64_t val = (uint64_t)(uintptr_t)va_arg(args, void *);
                VSNPRINTF_APPEND('0'); VSNPRINTF_APPEND('x');
                RENDER_UINT(val, 16, 0, 16, '0');
                break;
            }
            /* ── string ─────────────────────────────────────────────── */
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) { VSNPRINTF_APPEND(*s++); }
                break;
            }
            /* ── char ───────────────────────────────────────────────── */
            case 'c': VSNPRINTF_APPEND((char)va_arg(args, int)); break;
            /* ── literal % ──────────────────────────────────────────── */
            case '%': VSNPRINTF_APPEND('%'); break;
            /* ── unknown: emit literally ─────────────────────────────── */
            default:  VSNPRINTF_APPEND('%'); VSNPRINTF_APPEND(*fmt); break;
        }
        fmt++;
    }
    buf[pos] = '\0';
    return (int)pos;

    #undef VSNPRINTF_APPEND
    #undef RENDER_UINT
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
    
    /* Still log to klog (and serial if debugger enabled) */
    for (int i = 0; i < len; i++) {
        klog_putc(buf[i]);
#if defined(DEBUG_LEVEL) && DEBUG_LEVEL >= 1
        dbg_serial_putc(buf[i]);
#endif
    }

    return len;
}
