/* ============================================================================
 * pic.c — NexusOS 8259 PIC Driver
 * Purpose: Remap PIC IRQ0-15 to INT 32-47, mask/unmask, EOI
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "hal/pic.h"
#include "hal/io.h"
#include "lib/printf.h"

/* ICW1 bits */
#define ICW1_ICW4       0x01    /* ICW4 needed */
#define ICW1_INIT       0x10    /* Initialization command */

/* ICW4 bits */
#define ICW4_8086       0x01    /* 8086/88 mode */

/* OCW3 — read ISR */
#define PIC_READ_ISR    0x0B

/* Remap PIC: IRQ0-7 → INT 32-39, IRQ8-15 → INT 40-47 */
void pic_init(void) {
    uint8_t mask1, mask2;

    /* Save existing masks */
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);

    /* ICW1: begin initialization sequence */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: vector offset */
    outb(PIC1_DATA, 0x20);     /* PIC1: IRQ 0-7  → INT 32-39 */
    io_wait();
    outb(PIC2_DATA, 0x28);     /* PIC2: IRQ 8-15 → INT 40-47 */
    io_wait();

    /* ICW3: cascading */
    outb(PIC1_DATA, 0x04);     /* PIC1: IRQ2 has slave */
    io_wait();
    outb(PIC2_DATA, 0x02);     /* PIC2: cascade identity = 2 */
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Restore saved masks (all masked initially for safety) */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    /* Mask all IRQs initially — drivers will unmask as needed */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    /* Unmask cascade line (IRQ2) so PIC2 IRQs can fire */
    pic_clear_mask(2);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("PIC remapped: IRQ0-7 -> INT 32-39, IRQ8-15 -> INT 40-47\n");
}

/* Send End-Of-Interrupt */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);   /* EOI to slave */
    }
    outb(PIC1_COMMAND, 0x20);       /* EOI to master */
}

/* Mask (disable) a specific IRQ line */
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}

/* Unmask (enable) a specific IRQ line */
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/* Disable the PIC entirely (mask all, for APIC transition) */
void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* Read the combined In-Service Register */
uint16_t pic_get_isr(void) {
    outb(PIC1_COMMAND, PIC_READ_ISR);
    outb(PIC2_COMMAND, PIC_READ_ISR);
    return (uint16_t)((inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND));
}

/* Read the Interrupt Mask Register */
uint16_t pic_read_imr(void) {
    return (uint16_t)((inb(PIC2_DATA) << 8) | inb(PIC1_DATA));
}

#include "lib/debug.h"
void pic_dump_state(void) {
#ifdef DEBUGGER_ENABLED
    uint16_t imr = pic_read_imr();
    uint16_t isr = pic_get_isr();
    uint16_t irr = pic_get_irr();
    debug_log(DEBUG_LEVEL_INFO, "PIC", "State: IMR=0x%04x, ISR=0x%04x, IRR=0x%04x", 
              (int)imr, (int)isr, (int)irr);
#endif
}

/* Read the combined Interrupt Request Register */
uint16_t pic_get_irr(void) {
    outb(PIC1_COMMAND, PIC_READ_IRR);
    outb(PIC2_COMMAND, PIC_READ_IRR);
    return (uint16_t)((inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND));
}
