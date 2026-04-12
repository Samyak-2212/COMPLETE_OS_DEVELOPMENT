/* ============================================================================
 * cmd_clear.c — NexusOS Shell Command: clear
 * Purpose: Clear the terminal screen using ANSI escape sequences.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[2J\033[H");
    return 0;
}

REGISTER_SHELL_COMMAND(clear, "", "", "Clear terminal screen", "Clear terminal screen", "system", cmd_clear);
