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

    /* Determine parent directory and filename */
    char parent_path[SHELL_MAX_LINE_LEN];
    strcpy(parent_path, path);

    char *slash = NULL;
    for (char *p = parent_path; *p; p++) {
        if (*p == '/') slash = p;
    }

    const char *filename;
    if (slash && slash != parent_path) {
        *slash   = '\0';
        filename = slash + 1;
    } else {
        strcpy(parent_path, "/");
        filename = (slash) ? slash + 1 : argv[1];
    }

    vfs_node_t *parent = vfs_resolve_path(parent_path);
    if (!parent || (parent->flags & FS_DIRECTORY) == 0) {
        kprintf("touch: %s: parent directory not found\n", parent_path);
        return 1;
    }

    if (!parent->ops || !parent->ops->mkdir) {
        kprintf("touch: not supported on this filesystem\n");
        return 1;
    }

    /* Use the filesystem's mkdir-equivalent to create an empty file node.
     * ramfs exposes file creation through its finddir/mkdir path. */
    (void)filename;
    kprintf("touch: not yet implemented (requires ramfs_create_file).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(touch, "Create empty file (stub)", "fs", cmd_touch);
