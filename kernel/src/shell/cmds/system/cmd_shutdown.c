/* ============================================================================
 * cmd_shutdown.c — NexusOS Shell Command: shutdown
 * Purpose: Halt the system (cli + hlt). ACPI proper arrives in Phase 5.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_shutdown(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[1;31mShutdown (hlt).\033[0m\n");
    __asm__ volatile("cli; hlt");
    return 0;
}

REGISTER_SHELL_COMMAND(shutdown, "Power off system", "system", cmd_shutdown);
