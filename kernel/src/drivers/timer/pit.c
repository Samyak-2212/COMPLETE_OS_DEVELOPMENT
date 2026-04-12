/* ============================================================================
 * pit.c — NexusOS PIT Timer Driver
 * Purpose: 1ms system tick via PIT Channel 0 (IRQ0)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/timer/pit.h"
#include "hal/io.h"
#include "hal/isr.h"
#include "hal/pic.h"
#include "lib/printf.h"
#include "lib/debug.h"
#include "drivers/video/framebuffer.h"
#include "display/terminal.h"

/* ── State ──────────────────────────────────────────────────────────────── */

static volatile uint64_t tick_count = 0;
static volatile int      boot_pixel_active = 1;

/* ── IRQ0 handler ───────────────────────────────────────────────────────── */

static void pit_irq_handler(registers_t *regs) {
    (void)regs;
    tick_count++;

    /* Boot responsiveness pixel: blink (0,0) every 100ms while booting */
    if (boot_pixel_active && tick_count % 100 == 0) {
        static int toggle = 0;
        framebuffer_put_pixel(0, 0, toggle ? 0xFFFFFFFF : 0x00000000);
        toggle = !toggle;
    }

    /* Update terminal cursor blink */
    terminal_blink_tick(&main_terminal);

    pic_send_eoi(0);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void pit_init(void) {
    /* Calculate divisor for target frequency */
    uint16_t divisor = (uint16_t)(PIT_BASE_FREQ / PIT_TARGET_FREQ);

    /* Channel 0, lobyte/hibyte access, rate generator (mode 2) */
    outb(PIT_COMMAND, 0x34);

    /* Send divisor (low byte, then high byte) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register IRQ0 handler */
    isr_register_handler(IRQ0, pit_irq_handler);

    /* Unmask IRQ0 */
    pic_clear_mask(0);

    kprintf_set_color(0x0088FF88, 0x001A1A2E);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, 0x001A1A2E);
    kprintf("PIT initialized: %u Hz (divisor=%u)\n",
            (unsigned int)PIT_TARGET_FREQ, (unsigned int)divisor);
}

uint64_t pit_get_ticks(void) {
    return tick_count;
}

void pit_sleep_ms(uint64_t ms) {
    uint64_t target = tick_count + ms;
    while (tick_count < target) {
        __asm__ volatile ("hlt");
    }
}

/* Stop and clear the boot responsiveness pixel */
void pit_stop_boot_pixel(void) {
    boot_pixel_active = 0;
    framebuffer_put_pixel(0, 0, 0x00000000); /* Clear to black */
}
