/* ============================================================================
 * pit.h — NexusOS PIT Timer Driver
 * Purpose: Programmable Interval Timer (8254) for system tick
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_TIMER_PIT_H
#define NEXUS_DRIVERS_TIMER_PIT_H

#include <stdint.h>

/* PIT I/O ports */
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

/* PIT base frequency */
#define PIT_BASE_FREQ   1193182

/* Target tick frequency (1000 Hz = 1ms per tick) */
#define PIT_TARGET_FREQ 1000

/* Initialize the PIT at 1000 Hz on IRQ0 */
void pit_init(void);

/* Get current tick count since boot */
uint64_t pit_get_ticks(void);

/* Busy-wait for the given number of milliseconds */
void pit_sleep_ms(uint64_t ms);

/* Stop and clear the boot responsiveness pixel at (0,0) */
void pit_stop_boot_pixel(void);

#endif /* NEXUS_DRIVERS_TIMER_PIT_H */
