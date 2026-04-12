/* ============================================================================
 * shell_command.h — NexusOS Shell Command Registration Interface
 * Purpose: Shared struct and REGISTER_SHELL_COMMAND macro for linker-section
 *          auto-registration of shell commands.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_SHELL_COMMAND_H
#define NEXUS_SHELL_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

/* ── Constants ─────────────────────────────────────────────────────────── */

#define SHELL_MAX_LINE_LEN  256
#define SHELL_MAX_ARGS      16

/* ── Types ──────────────────────────────────────────────────────────────── */

/* Command handler signature */
typedef int (*shell_cmd_handler_t)(int argc, char **argv);

/*
 * shell_command_t — Command descriptor placed into .shell_commands ELF section.
 * Each registered command is a const instance of this struct.
 */
typedef struct {
    const char          *name;
    const char          *args;
    const char          *options;
    const char          *description;
    const char          *usage_help;
    const char          *category;   /* "system", "fs", "diag", "process" */
    shell_cmd_handler_t  handler;
    bool                 no_h_help;  /* if true, restrict help intercept to --help only */
} shell_command_t;

/*
 * REGISTER_SHELL_COMMAND — Zero-touch command registration macro.
 *
 * Usage (in any .c command file):
 *   static int cmd_foo(int argc, char **argv) { ... }
 *   REGISTER_SHELL_COMMAND(foo, "Does foo things", "system", cmd_foo);
 *
 * The macro places a shell_command_t into ELF section ".shell_commands".
 * The linker script collects all such structs into a contiguous array
 * bounded by __shell_commands_start / __shell_commands_end.
 * shell_core.c iterates them at runtime — no manual table edits needed.
 */
#define REGISTER_SHELL_COMMAND_EXT(cmd_name, cmd_args, cmd_opts, desc, usage, cat, func, no_h) \
    static const shell_command_t __shell_cmd_##cmd_name                        \
        __attribute__((used, section(".shell_commands"), aligned(8))) = {       \
            .name        = #cmd_name,                                           \
            .args        = cmd_args,                                            \
            .options     = cmd_opts,                                            \
            .description = desc,                                                \
            .usage_help  = usage,                                               \
            .category    = cat,                                                 \
            .handler     = func,                                                \
            .no_h_help   = no_h                                                 \
        }

#define REGISTER_SHELL_COMMAND(cmd_name, cmd_args, cmd_opts, desc, usage, cat, func) \
    REGISTER_SHELL_COMMAND_EXT(cmd_name, cmd_args, cmd_opts, desc, usage, cat, func, false)

/* ── Linker-provided section boundaries (defined by x86_64.lds) ─────────── */

extern const shell_command_t __shell_commands_start[];
extern const shell_command_t __shell_commands_end[];

/* ── Shared shell state accessors (defined in shell_core.c) ─────────────── */

/* Returns the current working directory string (read-only) */
const char *shell_get_cwd(void);

/* Sets the CWD — called only by cmd_cd */
void shell_set_cwd(const char *path, void *node);

/* Resolve relative or absolute path into output[] using current CWD */
void shell_normalize_path(const char *input, char *output);

#endif /* NEXUS_SHELL_COMMAND_H */
