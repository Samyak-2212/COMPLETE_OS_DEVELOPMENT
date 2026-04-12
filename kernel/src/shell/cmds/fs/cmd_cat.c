/* ============================================================================
 * cmd_cat.c — NexusOS Shell Command: cat
 * Purpose: Print file contents to the terminal via VFS read.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include <stdint.h>

static int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("Usage: cat <file>\n");
        return 1;
    }

    char path[SHELL_MAX_LINE_LEN];
    shell_normalize_path(argv[1], path);

    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("cat: %s: No such file\n", path);
        return 1;
    }

    if ((node->flags & FS_FILE) == 0) {
        kprintf("cat: %s: Is a directory\n", argv[1]);
        return 1;
    }

    uint8_t *buf = kmalloc(node->length + 1);
    if (!buf) {
        kprintf("cat: out of memory\n");
        return 1;
    }

    uint64_t nread = vfs_read(node, 0, node->length, buf);
    buf[nread] = '\0';
    kprintf("%s\n", (char *)buf);
    kfree(buf);
    return 0;
}

REGISTER_SHELL_COMMAND(cat, "<file>", "", "Concatenate and print files", "Concatenate and print files", "fs", cmd_cat);
