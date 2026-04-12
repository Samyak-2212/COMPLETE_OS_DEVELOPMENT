/* ============================================================================
 * cmd_help.c — NexusOS Shell Command: help
 * Purpose: List all registered commands grouped by category.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"

static int cmd_help(int argc, char **argv) {
    if (argc == 2) {
        for (const shell_command_t *cmd = __shell_commands_start;
             cmd < __shell_commands_end;
             cmd++) {
            if (strcmp(argv[1], cmd->name) == 0) {
                kprintf("\033[1;36mUsage:\033[0m %s", cmd->name);
                if (cmd->options && cmd->options[0] != '\0') {
                    kprintf(" [OPTIONS]");
                }
                if (cmd->args && cmd->args[0] != '\0') {
                    kprintf(" %s", cmd->args);
                }
                kprintf("\n\n  %s\n\n", cmd->usage_help);

                if (cmd->options && cmd->options[0] != '\0') {
                    kprintf("\033[1;36mOptions:\033[0m\n  %s\n\n", cmd->options);
                }
                return 0;
            }
        }
        kprintf("help: no such documentation for '%s'\n", argv[1]);
        return 1;
    }

    static const char *categories[] = {"system", "fs", "diag", "process", NULL};
    static const char *cat_labels[] = {"System", "Filesystem", "Diagnostics", "Process"};

    kprintf("\033[1;36mNexusOS Shell Help\033[0m\n");

    for (int c = 0; categories[c] != NULL; c++) {
        int printed_header = 0;
        for (const shell_command_t *cmd = __shell_commands_start;
             cmd < __shell_commands_end;
             cmd++) {
            if (strcmp(cmd->category, categories[c]) == 0) {
                if (!printed_header) {
                    kprintf("\n  \033[1;33m[%s]\033[0m\n", cat_labels[c]);
                    printed_header = 1;
                }
                kprintf("    \033[1;32m%s\033[0m", cmd->name);
                int pad = 12 - strlen(cmd->name);
                if (pad < 0) pad = 0;
                for (int i = 0; i < pad; i++) kprintf(" ");
                kprintf(" - %s\n", cmd->description);
            }
        }
    }
    kprintf("\n");
    return 0;
}

REGISTER_SHELL_COMMAND(help, "[command]", "", "Display available commands", "Display available commands", "system", cmd_help);
