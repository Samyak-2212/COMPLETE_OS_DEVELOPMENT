/* ============================================================================
 * cmd_help.c — NexusOS Shell Command: help
 * Purpose: List all registered commands grouped by category.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"

static int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;

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
                kprintf("    \033[1;32m%-12s\033[0m - %s\n",
                        cmd->name, cmd->description);
            }
        }
    }
    kprintf("\n");
    return 0;
}

REGISTER_SHELL_COMMAND(help, "Display available commands", "system", cmd_help);
