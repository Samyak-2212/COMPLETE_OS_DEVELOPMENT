/* ============================================================================
 * cmd_ls.c — NexusOS Shell Command: ls
 * Purpose: List directory contents via VFS readdir.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "fs/vfs.h"
#include <stdint.h>

static int cmd_ls(int argc, char **argv) {
    char path[SHELL_MAX_LINE_LEN];

    if (argc > 1) {
        shell_normalize_path(argv[1], path);
    } else {
        strcpy(path, shell_get_cwd());
    }

    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("\033[1;31mls: %s: No such file or directory\033[0m\n", path);
        return 1;
    }

    if ((node->flags & FS_DIRECTORY) == 0) {
        kprintf("%s\n", node->name);
        return 0;
    }

    int      i     = 0;
    dirent_t *de;
    int      first = 1;

    while ((de = vfs_readdir(node, i++)) != NULL) {
        if (!first) kprintf("  ");
        first = 0;

        int has_space = (strchr(de->name, ' ') != NULL);

        if (de->type & FS_DIRECTORY) {
            kprintf("\033[1;34m%s%s%s\033[0m",
                    has_space ? "\"" : "", de->name, has_space ? "\"" : "");
        } else {
            kprintf("\033[1;32m%s%s%s\033[0m",
                    has_space ? "\"" : "", de->name, has_space ? "\"" : "");
        }
    }
    kprintf("\n");
    return 0;
}

REGISTER_SHELL_COMMAND_EXT(ls, "[path]", "-l   Use a long listing format\n  -a   Do not ignore entries starting with .", "List directory contents", "Lists items in current directory or specified path.", "fs", cmd_ls, true);
