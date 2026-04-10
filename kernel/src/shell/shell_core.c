/* ============================================================================
 * shell_core.c — NexusOS Kernel Shell Core
 * Purpose: Input loop, command tokenizer, dispatch via .shell_commands section,
 *          CWD state management, and path normalization.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "shell/shell_core.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include "drivers/input/input_event.h"
#include <stdint.h>

/* Forward declaration — provided by input_manager.c */
extern int input_poll_event(input_event_t *out);

/*
 * Linker-provided section boundaries for the .shell_commands ELF section.
 * These are defined (via assignment) in the linker script x86_64.lds.
 */
extern const shell_command_t __shell_commands_start[];
extern const shell_command_t __shell_commands_end[];

/* ── CWD state ──────────────────────────────────────────────────────────── */

static char       cwd[SHELL_MAX_LINE_LEN] = "/";
static vfs_node_t *cwd_node               = NULL;

/* Returns the current working directory path string */
const char *shell_get_cwd(void) {
    return cwd;
}

/* Updates CWD — called exclusively by cmd_cd */
void shell_set_cwd(const char *path, void *node) {
    strncpy(cwd, path, SHELL_MAX_LINE_LEN - 1);
    cwd[SHELL_MAX_LINE_LEN - 1] = '\0';
    cwd_node = (vfs_node_t *)node;
}

/* ── Path normalization ─────────────────────────────────────────────────── */

/*
 * shell_normalize_path — Resolve input (relative or absolute) relative to CWD.
 * Handles ".." and "." components.  output[] must be SHELL_MAX_LINE_LEN bytes.
 */
void shell_normalize_path(const char *input, char *output) {
    char temp[SHELL_MAX_LINE_LEN];

    if (input[0] == '/') {
        strncpy(temp, input, SHELL_MAX_LINE_LEN - 1);
        temp[SHELL_MAX_LINE_LEN - 1] = '\0';
    } else {
        strcpy(temp, cwd);
        if (strcmp(cwd, "/") != 0) strcat(temp, "/");
        strcat(temp, input);
    }

    /* Stack-based normalization for ".." and "." */
    char *parts[32];
    int   part_count = 0;
    char *token      = temp;

    if (*token == '/') token++;   /* skip leading '/' */

    while (token && *token && part_count < 32) {
        char *next_token = strchr(token, '/');
        if (next_token) *next_token = '\0';

        if (strcmp(token, "..") == 0) {
            if (part_count > 0) part_count--;
        } else if (strcmp(token, ".") != 0 && strlen(token) > 0) {
            parts[part_count++] = token;
        }

        if (!next_token) break;
        token = next_token + 1;
    }

    strcpy(output, "/");
    for (int i = 0; i < part_count; i++) {
        strcat(output, parts[i]);
        if (i < part_count - 1) strcat(output, "/");
    }
}

/* ── Command dispatch ───────────────────────────────────────────────────── */

/*
 * shell_execute — Tokenize line and dispatch to matching registered command.
 * Returns the handler's return value, or -1 if no command matched.
 */
int shell_execute(const char *line) {
    if (!line || strlen(line) == 0) return 0;

    char *buf = kmalloc(strlen(line) + 1);
    if (!buf) return -1;
    strcpy(buf, line);

    char *argv[SHELL_MAX_ARGS];
    int   argc = 0;

    /* Whitespace tokenizer */
    char *p = buf;
    while (*p && argc < SHELL_MAX_ARGS) {
        while (*p == ' ') *p++ = '\0';
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
    }

    if (argc == 0) {
        kfree(buf);
        return 0;
    }

    int ret = -1;
    for (const shell_command_t *cmd = __shell_commands_start;
         cmd < __shell_commands_end;
         cmd++) {
        if (strcmp(argv[0], cmd->name) == 0) {
            ret = cmd->handler(argc, argv);
            break;
        }
    }

    if (ret == -1) {
        kprintf("nexus: %s: command not found\n", argv[0]);
    }

    kfree(buf);
    return ret;
}

/* ── Interactive input loop ─────────────────────────────────────────────── */

/* shell_run — Launches the interactive kernel shell.  Never returns. */
void shell_run(void) {
    char line[SHELL_MAX_LINE_LEN];
    int  pos = 0;

    cwd_node = vfs_root;

    kprintf("\nWelcome to NexusOS Shell!\nType 'help' for a list of commands.\n\n");

    while (1) {
        kprintf("\033[1;36mnexus\033[0m:\033[1;34m%s\033[0m$ ", cwd);
        pos = 0;
        memset(line, 0, sizeof(line));

        while (1) {
            input_event_t event;
            if (input_poll_event(&event)) {
                if (event.type == INPUT_EVENT_KEY_PRESS) {
                    if (event.ascii == '\n') {
                        kprintf("\n");
                        shell_execute(line);
                        break;
                    } else if (event.ascii == '\b') {
                        if (pos > 0) {
                            pos--;
                            line[pos] = '\0';
                            kprintf("\b");
                        }
                    } else if (event.ascii >= 32 && event.ascii <= 126) {
                        if (pos < SHELL_MAX_LINE_LEN - 1) {
                            line[pos++] = event.ascii;
                            kputchar(event.ascii);
                        }
                    }
                }
            }
            __asm__ volatile("hlt");
        }
    }
}
