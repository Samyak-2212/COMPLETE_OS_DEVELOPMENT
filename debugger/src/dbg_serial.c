/* ============================================================================
 * dbg_serial.c — NexusOS Debugger UART Driver
 * Purpose: Sole COM1 (0x3F8) driver for the kernel. Polling-based, no
 *          interrupts, no heap, no locks — survives kernel panics.
 *          Replaces kernel/src/lib/serial.c entirely.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_HIDBUG

#include <hal/io.h>
#include <stdarg.h>
#include <lib/string.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static inline int dbg_tx_empty(void) {
    return inb(DBG_COM_PORT + 5) & 0x20;
}

static inline int dbg_rx_ready(void) {
    return inb(DBG_COM_PORT + 5) & 0x01;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * dbg_serial_init — Configure COM port.
 * Called once from dbg_init(). Safe to call multiple times (idempotent).
 */
void dbg_serial_init(void) {
    uint16_t port = DBG_COM_PORT;
    uint16_t divisor = 115200 / DBG_BAUD_RATE;

    outb(port + 1, 0x00);              /* Disable interrupts */
    outb(port + 3, 0x80);              /* Enable DLAB (set baud rate divisor) */
    outb(port + 0, (uint8_t)(divisor & 0xFF));      /* Divisor low byte */
    outb(port + 1, (uint8_t)((divisor >> 8) & 0xFF)); /* Divisor high byte */
    outb(port + 3, 0x03);              /* 8 bits, no parity, one stop bit */
    outb(port + 2, 0xC7);              /* Enable FIFO, clear, 14-byte threshold */
    outb(port + 4, 0x0B);              /* RTS/DSR set */
}

/* Blocking write — spins until transmit holding register is empty */
void dbg_serial_putc(char c) {
    if (c == '\n') {
        while (dbg_tx_empty() == 0);
        outb(DBG_COM_PORT, '\r');
    }
    while (dbg_tx_empty() == 0);
    outb(DBG_COM_PORT, (uint8_t)c);
}

/* String write */
void dbg_serial_puts(const char *s) {
    while (*s) {
        dbg_serial_putc(*s++);
    }
}

/* Non-blocking: returns true if a byte is waiting in the receive buffer */
bool dbg_serial_poll(void) {
    return dbg_rx_ready() != 0;
}

/* Blocking read — spins until a byte is available */
char dbg_serial_getc(void) {
    while (!dbg_rx_ready());
    return (char)inb(DBG_COM_PORT);
}

/*
 * dbg_serial_printf — Minimal formatted print to serial (no heap).
 * Supports: %s, %c, %d, %u, %x, %llx, %llu, %p, %%
 * Uses a stack buffer of DBG_LOG_BUF_SIZE bytes.
 */
void dbg_serial_printf(const char *fmt, ...) {
    char buf[DBG_LOG_BUF_SIZE];
    size_t pos = 0;

    va_list args;
    va_start(args, fmt);

    while (*fmt && pos < DBG_LOG_BUF_SIZE - 1) {
        if (*fmt != '%') { buf[pos++] = *fmt++; continue; }
        fmt++;

        /* Flags */
        bool left_align = false;
        if (*fmt == '-') { left_align = true; fmt++; }

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');

        /* Length modifier */
        bool is_ll = false;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_ll = true; fmt++; }
        }

        /* Render value into tmp[] */
        char tmp[64];
        int  tlen = 0;

        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s && tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = *s++;
                break;
            }
            case 'c':
                tmp[tlen++] = (char)va_arg(args, int);
                break;
            case 'd': case 'i': {
                int64_t val = is_ll ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
                char rev[20]; int rlen = 0;
                bool neg = (val < 0);
                if (neg) val = -val;
                if (val == 0) rev[rlen++] = '0';
                while (val > 0) { rev[rlen++] = (char)('0' + val % 10); val /= 10; }
                if (neg && tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = '-';
                while (rlen > 0 && tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = rev[--rlen];
                break;
            }
            case 'u': {
                uint64_t val = is_ll ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                char rev[20]; int rlen = 0;
                if (val == 0) rev[rlen++] = '0';
                while (val > 0) { rev[rlen++] = (char)('0' + val % 10); val /= 10; }
                while (rlen > 0 && tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = rev[--rlen];
                break;
            }
            case 'x': case 'X': case 'p': {
                uint64_t val = (*fmt == 'p') ? (uint64_t)(uintptr_t)va_arg(args, void *)
                             : (is_ll ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int));
                const char *hex = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                char rev[16]; int rlen = 0;
                if (val == 0) rev[rlen++] = '0';
                while (val > 0) { rev[rlen++] = hex[val & 0xF]; val >>= 4; }
                while (rlen > 0 && tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = rev[--rlen];
                break;
            }
            case '%':
                tmp[tlen++] = '%';
                break;
            default:
                tmp[tlen++] = '%';
                if (tlen < (int)sizeof(tmp) - 1) tmp[tlen++] = *fmt;
                break;
        }
        fmt++;

        /* Emit with padding */
        int pad = width - tlen;
        if (!left_align) {
            while (pad-- > 0 && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = ' ';
        }
        for (int i = 0; i < tlen && pos < DBG_LOG_BUF_SIZE - 1; i++) buf[pos++] = tmp[i];
        if (left_align) {
            while (pad-- > 0 && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = ' ';
        }
    }
    va_end(args);

    buf[pos] = '\0';
    dbg_serial_puts(buf);
}

#endif /* DEBUG_LEVEL >= DBG_HIDBUG */
