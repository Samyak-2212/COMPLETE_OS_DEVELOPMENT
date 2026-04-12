/* ============================================================================
 * cmd_mkdir.c — NexusOS Shell Command: mkdir
 * Purpose: Create a directory via vfs_mkdir.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "fs/vfs.h"

static int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: mkdir <path>\n");
        return 1;
    }

    char path[SHELL_MAX_LINE_LEN];
    shell_normalize_path(argv[1], path);

    int ret = vfs_mkdir(path);
    if (ret != 0) {
        kprintf("mkdir: %s: cannot create directory\n", path);
        return 1;
    }
    return 0;
}

REGISTER_SHELL_COMMAND(mkdir, "<dir>", "", "Create directory", "Create directory", "fs", cmd_mkdir);
