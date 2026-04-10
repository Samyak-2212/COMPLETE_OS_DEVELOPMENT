/* ============================================================================
 * cmd_umount.c — NexusOS Shell Command: umount
 * Purpose: Unmount a filesystem (stub — requires VFS unmount support).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_umount(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("umount: not yet implemented.\n");
    return 0;
}

REGISTER_SHELL_COMMAND(umount, "Unmount filesystem (stub)", "fs", cmd_umount);
