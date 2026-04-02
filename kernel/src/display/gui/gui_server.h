/* ============================================================================
 * gui_server.h — NexusOS GUI Server (RESERVED — Phase 6)
 * Purpose: Weak symbol declarations for future GUI server
 * Author: NexusOS Kernel Team
 *
 * NOTE: This file only declares functions as weak symbols.
 * When no GUI implementation is compiled in, these resolve to NULL,
 * and the display manager stays in terminal mode.
 * ============================================================================ */

#ifndef NEXUS_DISPLAY_GUI_SERVER_H
#define NEXUS_DISPLAY_GUI_SERVER_H

/*
 * Initialize the GUI server (compositor, window manager, event dispatch).
 * Returns 0 on success.
 *
 * This symbol is declared weak. If no implementation is linked,
 * its address will be NULL, and the display manager will keep the
 * terminal active.
 */
int gui_server_init(void) __attribute__((weak));

/*
 * Check if the GUI server is available (compiled and linked).
 * Returns true if gui_server_init is a valid function pointer.
 */
static inline int gui_available(void) {
    return gui_server_init != (void *)0;
}

#endif /* NEXUS_DISPLAY_GUI_SERVER_H */
