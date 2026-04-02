/* ============================================================================
 * ps2_mouse.h — NexusOS PS/2 Mouse Driver
 * Purpose: PS/2 mouse packet decoding (IRQ12)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_INPUT_PS2_MOUSE_H
#define NEXUS_DRIVERS_INPUT_PS2_MOUSE_H

#include <stdint.h>

/* Initialize the PS/2 mouse driver (registers IRQ12 handler) */
void ps2_mouse_init(void);

#endif /* NEXUS_DRIVERS_INPUT_PS2_MOUSE_H */
