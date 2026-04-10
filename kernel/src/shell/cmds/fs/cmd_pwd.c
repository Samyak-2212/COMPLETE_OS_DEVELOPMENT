/* ============================================================================
 * cmd_pwd.c — NexusOS Shell Command: pwd
 * Purpose: Print the current working directory.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("%s\n", shell_get_cwd());
    return 0;
}

REGISTER_SHELL_COMMAND(pwd, "Print working directory", "fs", cmd_pwd);
