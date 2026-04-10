/* ============================================================================
 * display_manager.c — NexusOS Display Management
 * Purpose: Implementation of output routing and VT management
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "display/display_manager.h"
#include "display/terminal.h"

static display_mode_t current_mode = DISPLAY_MODE_TERMINAL;

void display_manager_init(void) {
    terminal_init(&main_terminal);
    current_mode = DISPLAY_MODE_TERMINAL;
}

void display_manager_write(const char *data, uint64_t size) {
    if (current_mode == DISPLAY_MODE_TERMINAL) {
        terminal_write(&main_terminal, data, size);
    }
}

void display_manager_set_mode(display_mode_t mode) {
    current_mode = mode;
}

display_mode_t display_manager_get_mode(void) {
    return current_mode;
}
