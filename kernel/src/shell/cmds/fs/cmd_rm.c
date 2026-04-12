/* ============================================================================
 * cmd_rm.c — NexusOS Shell Command: rm
 * Purpose: Remove file (stub — requires vfs_unlink in Phase 4).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_rm(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("rm: not yet implemented (requires Phase 4 VFS unlink).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(rm, "<file>", "", "Remove file (stub)", "Remove file (stub)", "fs", cmd_rm);
