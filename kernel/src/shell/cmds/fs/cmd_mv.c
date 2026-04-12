/* ============================================================================
 * cmd_mv.c — NexusOS Shell Command: mv
 * Purpose: Move/rename file (stub — requires VFS rename support).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_mv(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("mv: not yet implemented (requires Phase 4 VFS rename).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(mv, "<src> <dst>", "", "Move/rename file (stub)", "Move/rename file (stub)", "fs", cmd_mv);
