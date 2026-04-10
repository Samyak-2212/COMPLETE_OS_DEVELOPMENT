/* ============================================================================
 * cmd_uptime.c — NexusOS Shell Command: uptime
 * Purpose: Display system uptime using the PIT tick counter.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "drivers/timer/pit.h"
#include <stdint.h>

static int cmd_uptime(int argc, char **argv) {
    (void)argc; (void)argv;

    uint64_t ticks   = pit_get_ticks();
    uint64_t seconds = ticks / 1000;
    uint64_t minutes = seconds / 60;
    uint64_t hours   = minutes / 60;

    kprintf("Uptime: %lu:%02lu:%02lu (%lu ticks)\n",
            hours, minutes % 60, seconds % 60, ticks);
    return 0;
}

REGISTER_SHELL_COMMAND(uptime, "System uptime", "diag", cmd_uptime);
