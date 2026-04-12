/* ============================================================================
 * cmd_touch.c — NexusOS Shell Command: touch
 * Purpose: Create an empty file in the VFS via vfs_mkdir sibling (ramfs create).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "fs/vfs.h"

static int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: touch <file>\n");
        return 1;
    }

    char path[SHELL_MAX_LINE_LEN];
    shell_normalize_path(argv[1], path);

    /* If the file already exists, succeed silently */
    vfs_node_t *existing = vfs_resolve_path(path);
    if (existing) return 0;

    /* Create the file using the new VFS API */
    vfs_node_t *node = vfs_create(path);
    if (!node) {
        kprintf("touch: %s: cannot create file\n", path);
        return 1;
    }

    return 0;
}

REGISTER_SHELL_COMMAND(touch, "<file>", "", "Create empty file", "Create empty file", "fs", cmd_touch);
