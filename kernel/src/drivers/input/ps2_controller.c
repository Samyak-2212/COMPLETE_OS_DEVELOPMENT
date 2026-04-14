/* ============================================================================
 * ps2_controller.c — NexusOS i8042 PS/2 Controller
 * Purpose: Initialize i8042: self-test, detect ports, enable devices
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/input/ps2_controller.h"
#include "hal/io.h"
#include "lib/printf.h"

/* ── Helpers ────────────────────────────────────────────────────────────── */

void ps2_wait_input(void) {
    int timeout = 100000;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT) && --timeout > 0) {
        io_wait();
    }
}

void ps2_wait_output(void) {
    int timeout = 100000;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT) && --timeout > 0) {
        io_wait();
    }
}

void ps2_send_command(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, cmd);
}

void ps2_send_data(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

void ps2_send_data_port2(uint8_t data) {
    ps2_wait_input();
    outb(PS2_COMMAND_PORT, 0xD4);   /* Route next data byte to port 2 */
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

/* ── Controller initialization ──────────────────────────────────────────── */

int ps2_controller_init(void) {
    int ports_detected = 0;

    /* Step 1: Disable both PS/2 ports */
    ps2_send_command(0xAD);     /* Disable port 1 */
    ps2_send_command(0xA7);     /* Disable port 2 */

    /* Step 2: Flush the output buffer */
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT) {
        inb(PS2_DATA_PORT);
    }

    /* Step 3: Read controller config byte */
    ps2_send_command(0x20);     /* Read config byte */
    uint8_t config = ps2_read_data();

    /* Disable IRQs and translation for now */
    config &= ~(1 << 0);       /* Disable port 1 IRQ (for now) */
    config &= ~(1 << 1);       /* Disable port 2 IRQ (for now) */
    config &= ~(1 << 6);       /* Disable Set2→Set1 translation (driver handles Set 2) */

    /* Check if port 2 clock was disabled — indicates dual-port controller */
    int dual_port = (config & (1 << 5)) ? 1 : 0;

    /* Write updated config byte */
    ps2_send_command(0x60);     /* Write config byte */
    ps2_send_data(config);

    /* Step 4: Controller self-test */
    ps2_send_command(0xAA);
    uint8_t test = ps2_read_data();
    if (test != 0x55) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] PS/2 controller self-test failed (0x%02x)\n",
                (unsigned int)test);
        return 0;
    }

    /* Restore config byte (self-test may reset it) */
    ps2_send_command(0x60);
    ps2_send_data(config);

    /* Step 5: Check for dual-port */
    if (dual_port) {
        ps2_send_command(0xA8);     /* Enable port 2 */
        ps2_send_command(0x20);     /* Read config */
        config = ps2_read_data();
        if (!(config & (1 << 5))) {
            dual_port = 1;          /* Port 2 confirmed */
        } else {
            dual_port = 0;
        }
        ps2_send_command(0xA7);     /* Disable port 2 again */
    }

    /* Step 6: Test port 1 */
    ps2_send_command(0xAB);         /* Test port 1 */
    test = ps2_read_data();
    if (test == 0x00) {
        ports_detected |= 1;       /* Port 1 OK (keyboard) */
    }

    /* Step 7: Test port 2 */
    if (dual_port) {
        ps2_send_command(0xA9);     /* Test port 2 */
        test = ps2_read_data();
        if (test == 0x00) {
            ports_detected |= 2;   /* Port 2 OK (mouse) */
        }
    }

    /* Step 8: Enable working ports and their IRQs */
    ps2_send_command(0x20);
    config = ps2_read_data();

    if (ports_detected & 1) {
        ps2_send_command(0xAE);     /* Enable port 1 */
        config |= (1 << 0);        /* Enable port 1 IRQ */
    }
    if (ports_detected & 2) {
        ps2_send_command(0xA8);     /* Enable port 2 */
        config |= (1 << 1);        /* Enable port 2 IRQ */
    }

    ps2_send_command(0x60);
    ps2_send_data(config);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("PS/2 controller: port1=%s, port2=%s\n",
            (ports_detected & 1) ? "keyboard" : "none",
            (ports_detected & 2) ? "mouse" : "none");

    return ports_detected;
}
