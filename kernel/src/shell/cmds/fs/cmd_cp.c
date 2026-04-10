/* ============================================================================
 * cmd_cp.c — NexusOS Shell Command: cp
 * Purpose: Copy file (stub — requires full VFS read+write support).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_cp(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("cp: not yet implemented (requires Phase 4 VFS read/write).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(cp, "Copy file (stub)", "fs", cmd_cp);
