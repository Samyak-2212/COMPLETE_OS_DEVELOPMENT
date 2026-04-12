/* ============================================================================
 * cmd_shutdown.c — NexusOS Shell Command: shutdown
 * Purpose: Halt the system (cli + hlt). ACPI proper arrives in Phase 5.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"

static int cmd_shutdown(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[1;31mShutdown (ACPI Virtual / hlt).\033[0m\n");
    
    /* Delay to allow QEMU's UI rendering thread to flush the framebuffer 
     * before we physically halt the vCPU logic. */
    for (volatile int i = 0; i < 90000000; i++);

    /* QEMU specific Virtual ACPI exit port */
    __asm__ volatile(
        "outw %%ax, %%dx"
        : 
        : "a"((uint16_t)0x2000), "d"((uint16_t)0x604)
    );

    /* Fallback */
    __asm__ volatile("cli; hlt");
    return 0;
}

REGISTER_SHELL_COMMAND(shutdown, "", "", "Power off system", "Power off system", "system", cmd_shutdown);
