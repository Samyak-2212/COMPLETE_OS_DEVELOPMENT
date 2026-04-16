/* ============================================================================
 * dbg_log.c — NexusOS Debugger Structured Logging
 * Purpose: High-level serial logging (hidbug+). Provides dbg_init(),
 *          dbg_log(), and dbg_panic_hook(). All output goes to COM1 only.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_HIDBUG

#include <stdarg.h>

/* ── Forward declarations from dbg_serial.c ──────────────────────────────── */

void dbg_serial_init(void);
void dbg_serial_puts(const char *s);
void dbg_serial_printf(const char *fmt, ...);

/* ── Level labels ─────────────────────────────────────────────────────────── */

static const char *level_names[] = {
    "TRACE", "INFO", "WARN", "ERROR", "PANIC"
};

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * dbg_init — Initialize UART and print the debugger banner.
 * Must be called before any dbg_log() invocations.
 */
void dbg_init(void) {
    dbg_serial_init();
    dbg_serial_puts("\r\n");
    dbg_serial_puts("==============================================\r\n");
#if DEBUG_LEVEL >= DBG_LODBUG
    dbg_serial_puts("  NexusOS Debugger — LODBUG mode active\r\n");
    dbg_serial_puts("  Type 'help' at the nxdbg> prompt.\r\n");
#else
    dbg_serial_puts("  NexusOS Debugger — HIDBUG mode active\r\n");
    dbg_serial_puts("  Serial console mirror enabled.\r\n");
#endif
    dbg_serial_puts("==============================================\r\n\r\n");
}

/*
 * dbg_log — Structured log entry to serial.
 *
 * Format: [LEVEL][COMP] message\n
 * Example: [INFO][BOOT] GDT initialized.
 *
 * Filtered by DBG_MIN_LOG_LEVEL — entries below that threshold are dropped.
 */
void dbg_log(int level, const char *component, const char *fmt, ...) {
    if (level < DBG_MIN_LOG_LEVEL) return;

    /* Clamp level index */
    if (level < 0) level = 0;
    if (level > 4) level = 4;

    dbg_serial_printf("[%s][%s] ", level_names[level], component);

    /* Re-implement varargs forwarding via a minimal bridge */
    char buf[DBG_LOG_BUF_SIZE];
    size_t pos = 0;

    va_list args;
    va_start(args, fmt);

    /* Minimal vsnprintf — same logic as dbg_serial_printf but into buf */
    while (*fmt && pos < DBG_LOG_BUF_SIZE - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        fmt++;

        bool is_ll = false;
        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_ll = true; fmt++; }
        }

        switch (*fmt) {
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = *s++;
                break;
            }
            case 'c': buf[pos++] = (char)va_arg(args, int); break;
            case 'd':
            case 'i': {
                int64_t val = is_ll ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
                if (val < 0) { if (pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = '-'; val = -val; }
                char tmp[20]; int tlen = 0;
                if (val == 0) tmp[tlen++] = '0';
                while (val > 0) { tmp[tlen++] = (char)('0' + val % 10); val /= 10; }
                while (tlen > 0 && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = tmp[--tlen];
                break;
            }
            case 'u': {
                uint64_t val = is_ll ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                char tmp[20]; int tlen = 0;
                if (val == 0) tmp[tlen++] = '0';
                while (val > 0) { tmp[tlen++] = (char)('0' + val % 10); val /= 10; }
                while (tlen > 0 && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = tmp[--tlen];
                break;
            }
            case 'x':
            case 'X':
            case 'p': {
                uint64_t val;
                if (*fmt == 'p') val = (uint64_t)(uintptr_t)va_arg(args, void *);
                else val = is_ll ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
                const char *hex = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                char tmp[16]; int tlen = 0;
                if (val == 0) tmp[tlen++] = '0';
                while (val > 0) { tmp[tlen++] = hex[val & 0xF]; val >>= 4; }
                while (tlen > 0 && pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = tmp[--tlen];
                break;
            }
            case '%': if (pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = '%'; break;
            default:
                if (pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = '%';
                if (pos < DBG_LOG_BUF_SIZE - 1) buf[pos++] = *fmt;
                break;
        }
        fmt++;
    }
    va_end(args);

    buf[pos] = '\0';
    dbg_serial_puts(buf);
    dbg_serial_puts("\r\n");
}

/*
 * dbg_panic_hook — Called by kpanic() before halting.
 * Logs the panic message. In LODBUG mode, enters interactive console so
 * agents can inspect the machine state before the system is lost.
 */
void dbg_panic_hook(const char *msg) {
    dbg_serial_puts("\r\n");
    dbg_serial_puts("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
    dbg_serial_puts("  KERNEL PANIC\r\n");
    dbg_serial_printf("  %s\r\n", msg);
    dbg_serial_puts("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");

#if DEBUG_LEVEL >= DBG_LODBUG
    dbg_serial_puts("  Entering interactive debugger. Type 'continue' to halt.\r\n\r\n");
    dbg_console_enter();
#endif
}

#endif /* DEBUG_LEVEL >= DBG_HIDBUG */
