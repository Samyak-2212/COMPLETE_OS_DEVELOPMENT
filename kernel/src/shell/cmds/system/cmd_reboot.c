/* ============================================================================
 * cmd_reboot.c — NexusOS Shell Command: reboot
 * Purpose: Reboot the system via PS/2 controller pulse on pin 0xFE.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "hal/io.h"

static int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[1;33m[WARN] Reboot requires Phase 5 generic ACPI FADT parser.\033[0m\n");
    kprintf("To safely restart, please close the QEMU window directly.\n");
    return 0;
}

REGISTER_SHELL_COMMAND(reboot, "", "", "Reboot system (STUB)", "Reboot system (Pending Phase 5 ACPI driver)", "system", cmd_reboot);
