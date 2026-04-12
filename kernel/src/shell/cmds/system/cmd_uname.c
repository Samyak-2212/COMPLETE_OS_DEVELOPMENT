/* ============================================================================
 * cmd_uname.c — NexusOS Shell Command: uname
 * Purpose: Print kernel/system version information.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_uname(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[1;36mNexusOS\033[0m \033[1;37m0.1.0-Genesis\033[0m \033[33mx86_64\033[0m\n");
    return 0;
}

REGISTER_SHELL_COMMAND(uname, "", "", "Print system information", "Print system information", "system", cmd_uname);
