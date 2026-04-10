/* ============================================================================
 * cmd_mount.c — NexusOS Shell Command: mount
 * Purpose: Mount a filesystem (stub — auto-mount handles Phase 3 devices).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_mount(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("mount: manual mounting not yet implemented.\n");
    kprintf("       Phase 3 auto-mounts detected storage partitions at boot.\n");
    return 0;
}

REGISTER_SHELL_COMMAND(mount, "Mount filesystem (stub)", "fs", cmd_mount);
