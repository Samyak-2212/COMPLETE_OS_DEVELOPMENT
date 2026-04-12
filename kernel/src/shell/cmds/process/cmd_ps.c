/* ============================================================================
 * cmd_ps.c — NexusOS Shell Command: ps
 * Purpose: Process list (stub — requires Phase 4 multitasking scheduler).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_ps(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("ps: not yet implemented (requires Phase 4 scheduler).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(ps, "", "", "Process list (stub)", "Process list (stub)", "process", cmd_ps);
