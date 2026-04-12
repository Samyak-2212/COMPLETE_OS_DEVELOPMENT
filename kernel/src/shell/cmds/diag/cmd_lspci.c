/* ============================================================================
 * cmd_lspci.c — NexusOS Shell Command: lspci
 * Purpose: List all PCI devices enumerated at boot.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "drivers/pci/pci.h"
#include <stdint.h>

static int cmd_lspci(int argc, char **argv) {
    (void)argc; (void)argv;

    int count = 0;
    pci_device_t *devices = pci_get_devices(&count);

    kprintf("\033[1;37mPCI Bus Enumeration (%d devices):\033[0m\n", count);
    for (int i = 0; i < count; i++) {
        kprintf("  \033[33m%02x:%02x.%d\033[0m - "
                "\033[1;32m[%04x:%04x]\033[0m "
                "\033[36m(class %02x%02x)\033[0m %s\n",
                devices[i].bus,
                devices[i].device,
                devices[i].function,
                devices[i].vendor_id,
                devices[i].device_id,
                devices[i].class_code,
                devices[i].subclass_code,
                devices[i].multi_function ? "\033[1;30m(multi)\033[0m" : "");
    }
    return 0;
}

REGISTER_SHELL_COMMAND(lspci, "", "", "List PCI devices", "List PCI devices", "diag", cmd_lspci);
