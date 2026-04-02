/* ============================================================================
 * pic.h — NexusOS 8259 PIC Driver
 * Purpose: Remap PIC, mask/unmask IRQs, send EOI
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_HAL_PIC_H
#define NEXUS_HAL_PIC_H

#include <stdint.h>

/* PIC I/O ports */
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

#define PIC_READ_IRR    0x0A
#define PIC_READ_ISR    0x0B

/* Initialize and remap the PIC (IRQ0→INT32, IRQ8→INT40) */
void pic_init(void);

/* Send End-Of-Interrupt for the given IRQ number (0-15) */
void pic_send_eoi(uint8_t irq);

/* Mask (disable) a specific IRQ line */
void pic_set_mask(uint8_t irq);

/* Unmask (enable) a specific IRQ line */
void pic_clear_mask(uint8_t irq);

/* Disable the PIC entirely (for APIC migration) */
void pic_disable(void);

/* Get the combined ISR (In-Service Register) */
uint16_t pic_get_isr(void);

/* Get the combined IRR (Interrupt Request Register) */
uint16_t pic_get_irr(void);

/* Get the combined IMR (Interrupt Mask Register) */
uint16_t pic_read_imr(void);

/* Log PIC state to serial */
void pic_dump_state(void);

#endif /* NEXUS_HAL_PIC_H */
