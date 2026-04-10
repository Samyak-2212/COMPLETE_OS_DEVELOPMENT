/* ============================================================================
 * cmd_free.c — NexusOS Shell Command: free
 * Purpose: Display physical memory usage via PMM page counters.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
#include "lib/printf.h"
#include "mm/pmm.h"
#include <stdint.h>

static int cmd_free(int argc, char **argv) {
    (void)argc; (void)argv;

    uint64_t total = pmm_get_total_page_count() * 4096;
    uint64_t free  = pmm_get_free_page_count()  * 4096;
    uint64_t used  = total - free;

    kprintf("\033[1;37mMemory Statistics:\033[0m\n");
    kprintf("  \033[1;32mTotal:\033[0m %lu MiB\n",            total / (1024 * 1024));
    kprintf("  \033[1;31mUsed: \033[0m %lu MiB (%lu KB)\n",  used  / (1024 * 1024), used  / 1024);
    kprintf("  \033[1;34mFree: \033[0m %lu MiB (%lu KB)\n",  free  / (1024 * 1024), free  / 1024);
    return 0;
}

REGISTER_SHELL_COMMAND(free, "Display memory usage", "diag", cmd_free);
