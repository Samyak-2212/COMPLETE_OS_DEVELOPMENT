/* ============================================================================
 * ps2_keyboard.h — NexusOS PS/2 Keyboard Driver
 * Purpose: Scancode translation and key event generation (IRQ1)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_INPUT_PS2_KEYBOARD_H
#define NEXUS_DRIVERS_INPUT_PS2_KEYBOARD_H

#include <stdint.h>

/* Initialize the PS/2 keyboard driver (registers IRQ1 handler) */
void ps2_keyboard_init(void);

/* Get current modifier state */
uint8_t ps2_keyboard_get_modifiers(void);

#endif /* NEXUS_DRIVERS_INPUT_PS2_KEYBOARD_H */
