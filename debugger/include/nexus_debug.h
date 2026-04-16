/* ============================================================================
 * nexus_debug.h — NexusOS Debugger Public API
 * Purpose: Master header for the two-tier NexusOS serial debugger.
 *          Compile-time gated via DEBUG_LEVEL:
 *            0 = DBG_OFF    — zero code, zero overhead (release)
 *            1 = DBG_HIDBUG — serial log mirror of boot + shell
 *            2 = DBG_LODBUG — full interactive agent debugger
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#ifndef NEXUS_DEBUG_H
#define NEXUS_DEBUG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Compile-time mode constants ──────────────────────────────────────────── */

#define DBG_OFF     0   /* No debugger — release build */
#define DBG_HIDBUG  1   /* High-level: serial console mirror */
#define DBG_LODBUG  2   /* Low-level: full interactive debugger */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DBG_OFF
#endif

/* ── Configurable macros (change to taste, never hardcode elsewhere) ──────── */

#define DBG_COM_PORT        0x3F8   /* COM1. COM2 = 0x2F8 */
#define DBG_BAUD_RATE       115200
#define DBG_LOG_BUF_SIZE    512     /* Max formatted log line length */
#define DBG_CMD_BUF_SIZE    256     /* Max interactive command line length */
#define DBG_MAX_WATCHPOINTS 4       /* x86_64 DR0–DR3 */
#define DBG_HISTORY_SIZE    16      /* Command history ring buffer depth */
#define DBG_PROMPT          "nxdbg> "
#define DBG_MIN_LOG_LEVEL   DBG_TRACE  /* Filter: only print >= this level */

/* ── Future transport abstraction ─────────────────────────────────────────────
 * Current transport: DBG_TRANSPORT_SERIAL (always active).
 *
 * When Ethernet/WiFi drivers are available, add:
 *   DBG_TRANSPORT_SSH  — SSH server over TCP (requires net stack + crypto)
 *   DBG_TRANSPORT_UDP  — Lightweight UDP syslog-style remote logging
 *
 * To add a new transport:
 *   1. Define DBG_TRANSPORT_<NAME> constant below.
 *   2. Implement dbg_transport_ops_t in debugger/src/dbg_transport_<name>.c.
 *   3. Call dbg_transport_register(&my_ops) from kernel init after net is up.
 *   4. Gate with #if DEBUG_LEVEL >= DBG_HIDBUG.
 *
 * All transports share the same dbg_log() / dbg_console_poll() API. ──────── */

#define DBG_TRANSPORT_SERIAL  0x01  /* COM1 UART — always available, active now */
#define DBG_TRANSPORT_SSH     0x02  /* SSH over TCP — future: needs net + crypto */
#define DBG_TRANSPORT_UDP     0x04  /* UDP syslog   — future: needs net stack */

#define DBG_ACTIVE_TRANSPORT  DBG_TRANSPORT_SERIAL  /* change when net added */

/* ── Log levels ───────────────────────────────────────────────────────────── */

#define DBG_TRACE   0
#define DBG_INFO    1
#define DBG_WARN    2
#define DBG_ERROR   3
#define DBG_PANIC   4

/* ── Command registration macro (for extensible command table) ────────────── */

#if DEBUG_LEVEL >= DBG_LODBUG

typedef struct dbg_cmd {
    const char *name;
    const char *usage;
    const char *help;
    int (*handler)(int argc, char **argv);
} dbg_cmd_t;

/*
 * DBG_REGISTER_COMMAND — Place a dbg_cmd_t into the .dbg_commands ELF section.
 * The linker script collects all such entries into a contiguous array bounded
 * by __dbg_commands_start / __dbg_commands_end. dbg_console.c iterates them.
 *
 * Usage:
 *   static int my_cmd(int argc, char **argv) { ... }
 *   DBG_REGISTER_COMMAND(myname, "myname [args]", "Does something", my_cmd);
 */
#define DBG_REGISTER_COMMAND(cmd_name, cmd_usage, cmd_help, func)   \
    static const dbg_cmd_t __dbg_cmd_##cmd_name                     \
        __attribute__((used, section(".dbg_commands"), aligned(8))) = { \
            .name    = #cmd_name,                                    \
            .usage   = cmd_usage,                                    \
            .help    = cmd_help,                                     \
            .handler = func,                                         \
        }

extern const dbg_cmd_t __dbg_commands_start[];
extern const dbg_cmd_t __dbg_commands_end[];

#endif /* DEBUG_LEVEL >= DBG_LODBUG */

/* ============================================================================
 * HIDBUG+ API  (DEBUG_LEVEL >= 1)
 * ============================================================================ */

#if DEBUG_LEVEL >= DBG_HIDBUG

/* Initialize the serial port and print the debugger banner */
void dbg_init(void);

/* Formatted log to serial: "[LEVEL][COMP] message\n" */
void dbg_log(int level, const char *component, const char *fmt, ...);

/* Called on kernel panic — logs message; if LODBUG, enters interactive mode */
void dbg_panic_hook(const char *msg);

/* Raw serial I/O — available to any kernel subsystem */
void dbg_serial_putc(char c);
void dbg_serial_puts(const char *s);

#else /* DEBUG_LEVEL == 0 — all no-ops */

#define dbg_init()                  ((void)0)
#define dbg_log(l, c, f, ...)       ((void)0)
#define dbg_panic_hook(m)           ((void)0)
#define dbg_serial_putc(c)          ((void)0)
#define dbg_serial_puts(s)          ((void)0)

#endif /* DEBUG_LEVEL >= DBG_HIDBUG */

/* ============================================================================
 * LODBUG API  (DEBUG_LEVEL >= 2)
 * ============================================================================ */

#if DEBUG_LEVEL >= DBG_LODBUG

/* Non-blocking: check serial for a complete command line and process it */
void dbg_console_poll(void);

/* Blocking: enter interactive prompt loop (used by panic hook and traps) */
void dbg_console_enter(void);

/* Memory inspection */
void dbg_mem_dump(uint64_t addr, uint64_t len);
bool dbg_mem_write(uint64_t addr, const uint8_t *data, uint64_t len);
bool dbg_is_addr_mapped(uint64_t addr);

/* Register dump (from saved trap context or live CPU state) */
void dbg_regs_dump(void);

/* Stack backtrace using frame pointers */
void dbg_backtrace(void);

/* ELF symbol resolution — returns "name+0xOFFSET" in out (must have room) */
void dbg_symbols_init(void);
const char *dbg_resolve_symbol(uint64_t addr, uint64_t *offset_out);

/* Hardware breakpoints (DR0–DR3) */
int  dbg_bp_set(uint64_t addr);            /* Returns slot 0-3, or -1 if full */
void dbg_bp_clear(int slot);
void dbg_bp_list(void);

/* Data watchpoints via DR7 condition bits */
void dbg_watch_set(uint64_t addr, int len_code, int type);

/* Inject a command into the kernel shell and execute it */
void dbg_shell_inject(const char *cmd);

/* Is the interactive debugger currently active? */
bool dbg_is_active(void);

#else /* DEBUG_LEVEL < 2 — all no-ops */

#define dbg_console_poll()          ((void)0)
#define dbg_console_enter()         ((void)0)
#define dbg_mem_dump(a, l)          ((void)0)
#define dbg_mem_write(a, d, l)      (false)
#define dbg_is_addr_mapped(a)       (false)
#define dbg_regs_dump()             ((void)0)
#define dbg_backtrace()             ((void)0)
#define dbg_symbols_init()          ((void)0)
#define dbg_resolve_symbol(a, o)    (NULL)
#define dbg_bp_set(a)               (-1)
#define dbg_bp_clear(s)             ((void)0)
#define dbg_bp_list()               ((void)0)
#define dbg_watch_set(a, l, t)      ((void)0)
#define dbg_shell_inject(c)         ((void)0)
#define dbg_is_active()             (false)

#endif /* DEBUG_LEVEL >= DBG_LODBUG */

#endif /* NEXUS_DEBUG_H */
