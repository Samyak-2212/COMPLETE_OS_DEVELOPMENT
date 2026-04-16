/* ============================================================================
 * dbg_console.c — NexusOS Debugger Interactive Command Console
 * Purpose: Non-blocking and blocking command processor for the LODBUG tier.
 *          Provides the interactive "nxdbg> " prompt, command dispatch via
 *          the .dbg_commands linker section, line editing, history, and the
 *          '!' prefix for shell command injection.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

#include <lib/string.h>

/* ── Forward declarations ─────────────────────────────────────────────────── */

void dbg_serial_putc(char c);
void dbg_serial_puts(const char *s);
void dbg_serial_printf(const char *fmt, ...);
bool dbg_serial_poll(void);
char dbg_serial_getc(void);
void dbg_shell_inject(const char *cmd);

/* ── State ────────────────────────────────────────────────────────────────── */

static volatile bool dbg_active = false;

/* Line input buffer for non-blocking polling */
static char poll_buf[DBG_CMD_BUF_SIZE];
static int  poll_pos = 0;

/* Command history ring buffer */
static char dbg_history[DBG_HISTORY_SIZE][DBG_CMD_BUF_SIZE];
static int  dbg_hist_head  = 0;  /* next write slot */
static int  dbg_hist_count = 0;

/* ── Utility: parse hex/uint from string ──────────────────────────────────── */

static uint64_t __attribute__((unused)) parse_hex(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint64_t val = 0;
    while (*s) {
        char c = *s++;
        if      (c >= '0' && c <= '9') val = val * 16 + (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = val * 16 + (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = val * 16 + (uint64_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

static uint64_t __attribute__((unused)) parse_uint(const char *s) {
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (uint64_t)(*s++ - '0');
    }
    return val;
}

/* ── History helpers ──────────────────────────────────────────────────────── */

static void hist_push(const char *line) {
    if (strlen(line) == 0) return;
    strncpy(dbg_history[dbg_hist_head % DBG_HISTORY_SIZE], line,
            DBG_CMD_BUF_SIZE - 1);
    dbg_history[dbg_hist_head % DBG_HISTORY_SIZE][DBG_CMD_BUF_SIZE - 1] = '\0';
    dbg_hist_head++;
    if (dbg_hist_count < DBG_HISTORY_SIZE) dbg_hist_count++;
}

/* ── Tokenizer ────────────────────────────────────────────────────────────── */

#define DBG_MAX_ARGS 16

/*
 * Tokenize a command line in-place. Returns argc.
 * argv[] pointers point into the original (modified) buf.
 */
static int dbg_tokenize(char *buf, char **argv) {
    int argc = 0;
    char *p = buf;

    while (*p && argc < DBG_MAX_ARGS) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        argv[argc++] = p;

        /* Advance to next whitespace */
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

/* ── Built-in commands ────────────────────────────────────────────────────── */

/*
 * help — List all registered debugger commands.
 * Iterates the .dbg_commands linker section.
 */
static int dbg_cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    dbg_serial_puts("NexusOS Debugger Commands:\r\n");
    dbg_serial_puts("  !<cmd>          Execute shell command\r\n");
    dbg_serial_puts("  continue        Resume execution\r\n");
    dbg_serial_puts("  log <level>     Set min log level (0-4)\r\n\r\n");

    for (const dbg_cmd_t *cmd = __dbg_commands_start;
         cmd < __dbg_commands_end; cmd++) {
        dbg_serial_printf("  %-16s %s\r\n", cmd->usage, cmd->help);
    }
    return 0;
}
DBG_REGISTER_COMMAND(help, "help", "Show debugger commands", dbg_cmd_help);

/*
 * continue — Set active=false to break out of the interactive loop.
 * Only meaningful when dbg_console_enter() is blocking.
 */
static int dbg_cmd_continue(int argc, char **argv) {
    (void)argc; (void)argv;
    dbg_active = false;
    return 0;
}
DBG_REGISTER_COMMAND(continue, "continue", "Resume execution", dbg_cmd_continue);

/*
 * log — Set minimum log level filter.
 * level 0=TRACE 1=INFO 2=WARN 3=ERROR 4=PANIC
 */
static int dbg_cmd_log_level = DBG_MIN_LOG_LEVEL;

int dbg_get_log_level(void) { return dbg_cmd_log_level; }

static int dbg_cmd_log(int argc, char **argv) {
    if (argc < 2) {
        dbg_serial_printf("log: current level = %d\r\n", dbg_cmd_log_level);
        return 0;
    }
    dbg_cmd_log_level = (int)parse_uint(argv[1]);
    if (dbg_cmd_log_level < 0) dbg_cmd_log_level = 0;
    if (dbg_cmd_log_level > 4) dbg_cmd_log_level = 4;
    dbg_serial_printf("log: level set to %d\r\n", dbg_cmd_log_level);
    return 0;
}
DBG_REGISTER_COMMAND(log, "log [level]", "Set/show log verbosity", dbg_cmd_log);

/* ── Dispatch ─────────────────────────────────────────────────────────────── */

/*
 * dispatch_line — Parse and dispatch a completed line.
 *
 * Special prefix '!' passes the remainder to shell_execute().
 * Otherwise, searches the .dbg_commands section for a match.
 */
static void dispatch_line(char *line) {
    /* Trim trailing whitespace */
    int len = (int)strlen(line);
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\r' ||
                       line[len - 1] == '\n')) {
        line[--len] = '\0';
    }
    if (len == 0) return;

    hist_push(line);

    /* Shell injection */
    if (line[0] == '!') {
        dbg_shell_inject(line + 1);
        return;
    }

    /* Tokenize */
    char *argv[DBG_MAX_ARGS];
    int argc = dbg_tokenize(line, argv);
    if (argc == 0) return;

    /* Search registered commands */
    for (const dbg_cmd_t *cmd = __dbg_commands_start;
         cmd < __dbg_commands_end; cmd++) {
        if (strcmp(argv[0], cmd->name) == 0) {
            cmd->handler(argc, argv);
            return;
        }
    }

    dbg_serial_printf("unknown command: %s (type 'help')\r\n", argv[0]);
}

/* ── Blocking interactive console ─────────────────────────────────────────── */

/*
 * dbg_read_line — Read a complete line from serial (blocking).
 * Supports: backspace (0x08 / 0x7F), echo.
 * Returns the line in buf (null-terminated).
 */
static void dbg_read_line(char *buf, int max) {
    int pos = 0;
    while (1) {
        char c = dbg_serial_getc();

        if (c == '\r' || c == '\n') {
            buf[pos] = '\0';
            dbg_serial_puts("\r\n");
            return;
        }

        if (c == 0x08 || c == 0x7F) {  /* Backspace / DEL */
            if (pos > 0) {
                pos--;
                dbg_serial_puts("\b \b");  /* Erase on terminal */
            }
            continue;
        }

        /* Printable ASCII */
        if (c >= 32 && c <= 126 && pos < max - 1) {
            buf[pos++] = c;
            dbg_serial_putc(c);  /* Echo */
        }
    }
}

/*
 * dbg_console_enter — Blocking interactive prompt loop.
 * Used by: panic hook, trap handlers.
 * Exits when 'continue' command sets dbg_active = false.
 */
void dbg_console_enter(void) {
    dbg_active = true;
    dbg_serial_puts("\r\n");

    while (dbg_active) {
        dbg_serial_puts(DBG_PROMPT);

        char line[DBG_CMD_BUF_SIZE];
        dbg_read_line(line, DBG_CMD_BUF_SIZE);
        dispatch_line(line);
    }
}

/* ── Non-blocking poll ────────────────────────────────────────────────────── */

/*
 * dbg_console_poll — Non-blocking command check.
 * Called from the shell idle loop. Accumulates bytes in poll_buf until
 * a newline is received, then dispatches the complete line.
 */
void dbg_console_poll(void) {
    while (dbg_serial_poll()) {
        char c = dbg_serial_getc();

        if (c == '\r' || c == '\n') {
            poll_buf[poll_pos] = '\0';
            if (poll_pos > 0) {
                dbg_serial_puts("\r\n");
                dispatch_line(poll_buf);
                dbg_serial_puts(DBG_PROMPT);
            }
            poll_pos = 0;
            continue;
        }

        if (c == 0x08 || c == 0x7F) {
            if (poll_pos > 0) {
                poll_pos--;
                dbg_serial_puts("\b \b");
            }
            continue;
        }

        if (c >= 32 && c <= 126 && poll_pos < DBG_CMD_BUF_SIZE - 1) {
            poll_buf[poll_pos++] = c;
            dbg_serial_putc(c);
        }
    }
}

/* ── Public query ─────────────────────────────────────────────────────────── */

bool dbg_is_active(void) {
    return dbg_active;
}

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
