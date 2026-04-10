/* ============================================================================
 * cmd_echo.c — NexusOS Shell Command: echo
 * Purpose: Print arguments to terminal. Supports > and >> file redirection
 *          via VFS write when a redirect token is detected.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "mm/heap.h"
#include "fs/vfs.h"
#include <stdint.h>

static int cmd_echo(int argc, char **argv) {
    /* Scan for redirect operators: > or >> */
    int  redirect_mode  = 0; /* 0=none, 1=>, 2=>> */
    int  text_end       = argc;
    const char *out_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redirect_mode = 1;
            text_end      = i;
            out_path      = argv[i + 1];
            break;
        }
        if (strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            redirect_mode = 2;
            text_end      = i;
            out_path      = argv[i + 1];
            break;
        }
    }

    /* Build the output string */
    char buf[SHELL_MAX_LINE_LEN];
    buf[0] = '\0';
    for (int i = 1; i < text_end; i++) {
        strcat(buf, argv[i]);
        if (i < text_end - 1) strcat(buf, " ");
    }

    if (redirect_mode == 0) {
        /* Plain print */
        kprintf("%s\n", buf);
        return 0;
    }

    /* File redirect */
    char path[SHELL_MAX_LINE_LEN];
    shell_normalize_path(out_path, path);

    vfs_node_t *node = vfs_resolve_path(path);
    if (!node) {
        kprintf("echo: %s: cannot open for writing\n", path);
        return 1;
    }

    uint64_t offset = (redirect_mode == 2) ? node->length : 0;
    uint64_t len    = strlen(buf);

    /* Append newline */
    char *wbuf = kmalloc(len + 2);
    if (!wbuf) return 1;
    strcpy(wbuf, buf);
    wbuf[len]     = '\n';
    wbuf[len + 1] = '\0';

    vfs_write(node, offset, len + 1, (uint8_t *)wbuf);
    kfree(wbuf);
    return 0;
}

REGISTER_SHELL_COMMAND(echo, "Print text to terminal", "fs", cmd_echo);
