/* ============================================================================
 * ps2_mouse.c — NexusOS PS/2 Mouse Driver
 * Purpose: Decode 3/4-byte mouse packets, track movement and buttons (IRQ12)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/input/ps2_mouse.h"
#include "drivers/input/ps2_controller.h"
#include "drivers/input/input_event.h"
#include "hal/io.h"
#include "hal/isr.h"
#include "hal/pic.h"
#include "lib/printf.h"

/* Forward declaration for input manager */
extern void input_push_event(const input_event_t *event);

/* ── State ──────────────────────────────────────────────────────────────── */

static uint8_t mouse_packet[4];
static int packet_index = 0;
static int has_scroll_wheel = 0;
static uint8_t prev_buttons = 0;

/* ── IRQ12 handler ──────────────────────────────────────────────────────── */

static void mouse_irq_handler(registers_t *regs) {
    (void)regs;

    uint8_t data = inb(PS2_DATA_PORT);

    /* Byte 0 must have bit 3 set (always-1 bit in PS/2 protocol) */
    if (packet_index == 0 && !(data & 0x08)) {
        /* Out of sync — discard and wait for valid first byte */
        pic_send_eoi(12);
        return;
    }

    mouse_packet[packet_index++] = data;

    int packet_size = has_scroll_wheel ? 4 : 3;

    if (packet_index >= packet_size) {
        packet_index = 0;

        /* Decode packet */
        uint8_t flags = mouse_packet[0];
        int16_t dx = (int16_t)mouse_packet[1];
        int16_t dy = (int16_t)mouse_packet[2];
        int8_t  dz = 0;

        /* Sign-extend X and Y */
        if (flags & (1 << 4)) dx |= 0xFF00;  /* X sign bit */
        if (flags & (1 << 5)) dy |= 0xFF00;  /* Y sign bit */

        /* Invert Y (PS/2 mouse Y is inverted — up is positive) */
        dy = -dy;

        /* Scroll wheel (4th byte, if present) */
        if (has_scroll_wheel) {
            dz = (int8_t)mouse_packet[3];
        }

        /* Check for overflow — discard packet */
        if (flags & (1 << 6) || flags & (1 << 7)) {
            pic_send_eoi(12);
            return;
        }

        /* Generate movement event */
        if (dx != 0 || dy != 0) {
            input_event_t event;
            event.type = INPUT_EVENT_MOUSE_MOVE;
            event.scancode = 0;
            event.ascii = 0;
            event.modifiers = 0;
            event.mouse_dx = dx;
            event.mouse_dy = dy;
            event.mouse_dz = 0;
            event.mouse_buttons = flags & 0x07;
            input_push_event(&event);
        }

        /* Generate button events if changed */
        uint8_t buttons = flags & 0x07;
        if (buttons != prev_buttons) {
            input_event_t event;
            event.type = INPUT_EVENT_MOUSE_BUTTON;
            event.scancode = 0;
            event.ascii = 0;
            event.modifiers = 0;
            event.mouse_dx = 0;
            event.mouse_dy = 0;
            event.mouse_dz = 0;
            event.mouse_buttons = buttons;
            input_push_event(&event);
            prev_buttons = buttons;
        }

        /* Generate scroll event */
        if (dz != 0) {
            input_event_t event;
            event.type = INPUT_EVENT_MOUSE_SCROLL;
            event.scancode = 0;
            event.ascii = 0;
            event.modifiers = 0;
            event.mouse_dx = 0;
            event.mouse_dy = 0;
            event.mouse_dz = dz;
            event.mouse_buttons = buttons;
            input_push_event(&event);
        }
    }

    pic_send_eoi(12);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void ps2_mouse_init(void) {
    /* Enable mouse (port 2) — should already be done by controller */

    /* Set defaults */
    ps2_send_data_port2(0xFF);  /* Reset */
    ps2_read_data();            /* ACK */
    ps2_read_data();            /* Self-test result (0xAA) */
    ps2_read_data();            /* Mouse ID (0x00) */

    /* Try to enable scroll wheel (set sample rate 200, 100, 80) */
    uint8_t rates[] = {200, 100, 80};
    for (int i = 0; i < 3; i++) {
        ps2_send_data_port2(0xF3);  /* Set sample rate */
        ps2_read_data();             /* ACK */
        ps2_send_data_port2(rates[i]);
        ps2_read_data();             /* ACK */
    }

    /* Check mouse ID — 3 means scroll wheel enabled */
    ps2_send_data_port2(0xF2);  /* Get device ID */
    ps2_read_data();             /* ACK */
    uint8_t id = ps2_read_data();
    has_scroll_wheel = (id == 3) ? 1 : 0;

    /* Set sample rate to 100 */
    ps2_send_data_port2(0xF3);
    ps2_read_data();
    ps2_send_data_port2(100);
    ps2_read_data();

    /* Set resolution to 4 counts/mm */
    ps2_send_data_port2(0xE8);
    ps2_read_data();
    ps2_send_data_port2(0x02);
    ps2_read_data();

    /* Enable data reporting */
    ps2_send_data_port2(0xF4);
    ps2_read_data();    /* ACK */

    /* Register IRQ12 handler */
    isr_register_handler(IRQ12, mouse_irq_handler);

    /* Unmask IRQ12 */
    pic_clear_mask(12);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("PS/2 mouse initialized (IRQ12, scroll=%s)\n",
            has_scroll_wheel ? "yes" : "no");
}
