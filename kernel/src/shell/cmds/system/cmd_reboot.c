/* ============================================================================
 * cmd_reboot.c — NexusOS Shell Command: reboot
 * Purpose: Reboot the system via PS/2 controller pulse on pin 0xFE.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "hal/io.h"
#include <stdint.h>

static int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    kprintf("\033[1;31mRebooting...\033[0m\n");
    /* Wait for PS/2 input buffer to clear, then pulse reset */
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    return 0;
}

REGISTER_SHELL_COMMAND(reboot, "Reboot system", "system", cmd_reboot);
