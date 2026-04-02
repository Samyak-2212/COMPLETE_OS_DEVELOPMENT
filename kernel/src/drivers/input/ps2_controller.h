/* ============================================================================
 * ps2_controller.h — NexusOS i8042 PS/2 Controller
 * Purpose: Initialize and manage the i8042 PS/2 controller
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_INPUT_PS2_CONTROLLER_H
#define NEXUS_DRIVERS_INPUT_PS2_CONTROLLER_H

#include <stdint.h>

/* i8042 ports */
#define PS2_DATA_PORT       0x60
#define PS2_STATUS_PORT     0x64
#define PS2_COMMAND_PORT    0x64

/* Status register bits */
#define PS2_STATUS_OUTPUT   (1 << 0)    /* Output buffer full (data to read) */
#define PS2_STATUS_INPUT    (1 << 1)    /* Input buffer full (don't write yet) */

/* Initialize the i8042 PS/2 controller.
 * Returns: bit 0 = port 1 exists (keyboard), bit 1 = port 2 exists (mouse) */
int ps2_controller_init(void);

/* Wait until the input buffer is empty (safe to write) */
void ps2_wait_input(void);

/* Wait until the output buffer has data (safe to read) */
void ps2_wait_output(void);

/* Send a command byte to the controller */
void ps2_send_command(uint8_t cmd);

/* Send a data byte to port 1 (keyboard) */
void ps2_send_data(uint8_t data);

/* Send a data byte to port 2 (mouse) */
void ps2_send_data_port2(uint8_t data);

/* Read a data byte from the controller */
uint8_t ps2_read_data(void);

#endif /* NEXUS_DRIVERS_INPUT_PS2_CONTROLLER_H */
