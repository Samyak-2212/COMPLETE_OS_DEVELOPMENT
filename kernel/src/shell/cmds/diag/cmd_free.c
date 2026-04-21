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
    uint64_t mb    = 1000 * 1000;

    kprintf("\033[1;37mMemory Statistics:\033[0m\n");
    kprintf("  \033[1;32mTotal:\033[0m %lu.%02lu MB\n",
            total / mb, ((total % mb) * 100) / mb);
    kprintf("  \033[1;31mUsed: \033[0m %lu.%02lu MB (%lu KB)\n",
            used / mb, ((used % mb) * 100) / mb, used / 1000);
    kprintf("  \033[1;34mFree: \033[0m %lu.%02lu MB (%lu KB)\n",
            free / mb, ((free % mb) * 100) / mb, free / 1000);
    return 0;
}

REGISTER_SHELL_COMMAND(free, "", "", "Display memory usage", "Display memory usage", "diag", cmd_free);
