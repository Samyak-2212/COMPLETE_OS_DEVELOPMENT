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

/* ── Console state ──────────────────────────────────────────────────────── */

static uint32_t cursor_col = 0;
static uint32_t cursor_row = 0;
static uint32_t max_cols = 0;
static uint32_t max_rows = 0;
static uint32_t text_fg = 0x00CCCCCC;  /* Light gray */
static uint32_t text_bg = 0x00000000;  /* Black */

/* ── Internal helpers ───────────────────────────────────────────────────── */

/* Advance to next line, scrolling if necessary */
static void newline(void) {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= max_rows) {
        cursor_row = max_rows - 1;
        framebuffer_scroll(FONT_HEIGHT);
    }
}

/* Print a single character, handling control characters */
void kputchar(char c) {
    if (max_cols == 0 || max_rows == 0) return;

    switch (c) {
        case '\n':
            newline();
            return;
        case '\r':
            cursor_col = 0;
            return;
        case '\t':
            /* Tab to next 8-column boundary */
            cursor_col = (cursor_col + 8) & ~7u;
            if (cursor_col >= max_cols) {
                newline();
            }
            return;
        case '\b':
            if (cursor_col > 0) {
                cursor_col--;
                framebuffer_draw_char(cursor_col * FONT_WIDTH,
                                      cursor_row * FONT_HEIGHT,
                                      ' ', text_fg, text_bg);
            }
            return;
        default:
            break;
    }

    /* Wrap if we've hit the right edge */
    if (cursor_col >= max_cols) {
        newline();
    }

    framebuffer_draw_char(cursor_col * FONT_WIDTH,
                          cursor_row * FONT_HEIGHT,
                          c, text_fg, text_bg);
    cursor_col++;
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

/* Initialize the kprintf console. Must be called after framebuffer_init(). */
void kprintf_init(void) {
    framebuffer_get_text_dimensions(&max_cols, &max_rows);
    cursor_col = 0;
    cursor_row = 0;
}

/* Set text colors */
void kprintf_set_color(unsigned int fg, unsigned int bg) {
    text_fg = fg;
    text_bg = bg;
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
        if (*fmt == '0') {
            pad = '0';
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
                kputs(s);
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
                kputchar(*fmt);
                break;
        }
        fmt++;
        count++;
    }

    va_end(args);
    return count;
}
