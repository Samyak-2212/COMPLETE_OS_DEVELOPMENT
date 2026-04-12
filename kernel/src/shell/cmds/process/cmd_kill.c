/* ============================================================================
 * cmd_kill.c — NexusOS Shell Command: kill
 * Purpose: Terminate a process (stub — requires Phase 4 multitasking scheduler).
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_kill(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("kill: not yet implemented (requires Phase 4 scheduler).\n");
    return 0;
}

REGISTER_SHELL_COMMAND(kill, "<pid>", "", "Terminate process (stub)", "Terminate process (stub)", "process", cmd_kill);
