/* ============================================================================
 * display_manager.h — NexusOS Display Management
 * Purpose: Routes output between active terminal and future GUI
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DISPLAY_DISPLAY_MANAGER_H
#define NEXUS_DISPLAY_DISPLAY_MANAGER_H

#include <stdint.h>
#include <stddef.h>

/* Display modes */
typedef enum {
    DISPLAY_MODE_TERMINAL,
    DISPLAY_MODE_GUI
} display_mode_t;

void display_manager_init(void);
void display_manager_write(const char *data, uint64_t size);
void display_manager_set_mode(display_mode_t mode);
display_mode_t display_manager_get_mode(void);

#endif /* NEXUS_DISPLAY_DISPLAY_MANAGER_H */
