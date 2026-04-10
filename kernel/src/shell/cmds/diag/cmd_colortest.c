/* ============================================================================
 * cmd_colortest.c — NexusOS Shell Command: colortest
 * Purpose: Display the ANSI 16-color palette for terminal verification.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_colortest(int argc, char **argv) {
    (void)argc; (void)argv;

    kprintf("Terminal Color Test:\n");
    for (int i = 0; i < 8; i++) {
        kprintf("\033[%dm  HEX%d  \033[0m", 30 + i, i);
    }
    kprintf("\n");
    for (int i = 0; i < 8; i++) {
        kprintf("\033[%dm  HEX%d  \033[0m", 90 + i, i + 8);
    }
    kprintf("\n");
    return 0;
}

REGISTER_SHELL_COMMAND(colortest, "Terminal color test", "diag", cmd_colortest);
