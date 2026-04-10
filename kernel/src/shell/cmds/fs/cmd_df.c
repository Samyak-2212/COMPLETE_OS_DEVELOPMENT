/* ============================================================================
 * cmd_df.c — NexusOS Shell Command: df
 * Purpose: Report disk free space (stub — requires FS stat support).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_df(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("df: not yet implemented (requires VFS/FS stat support).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(df, "Disk free space (stub)", "fs", cmd_df);
