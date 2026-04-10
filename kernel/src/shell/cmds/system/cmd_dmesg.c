/* ============================================================================
 * cmd_dmesg.c — NexusOS Shell Command: dmesg
 * Purpose: Dump the kernel log buffer to the terminal.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/klog.h"

static int cmd_dmesg(int argc, char **argv) {
    (void)argc; (void)argv;
    klog_dump();
    return 0;
}

REGISTER_SHELL_COMMAND(dmesg, "Display kernel message buffer", "system", cmd_dmesg);
