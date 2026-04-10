/* ============================================================================
 * cmd_rmdir.c — NexusOS Shell Command: rmdir
 * Purpose: Remove empty directory (stub — requires vfs_rmdir in Phase 4).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_rmdir(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("rmdir: not yet implemented (requires Phase 4 VFS unlink).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(rmdir, "Remove directory (stub)", "fs", cmd_rmdir);
