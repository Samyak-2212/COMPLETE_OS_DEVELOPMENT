/* ============================================================================
 * cmd_cd.c — NexusOS Shell Command: cd
 * Purpose: Change the shell working directory via VFS path resolution.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "fs/vfs.h"

static int cmd_cd(int argc, char **argv) {
    if (argc < 2) return 0;

    char path[SHELL_MAX_LINE_LEN];
    shell_normalize_path(argv[1], path);

    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("cd: %s: No such directory\n", path);
        return 1;
    }

    if ((node->flags & FS_DIRECTORY) == 0) {
        kprintf("cd: %s: Not a directory\n", path);
        return 1;
    }

    shell_set_cwd(path, node);
    return 0;
}

REGISTER_SHELL_COMMAND(cd, "Change directory", "fs", cmd_cd);
