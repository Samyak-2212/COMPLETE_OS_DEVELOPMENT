/* ============================================================================
 * isr.h — NexusOS Interrupt Service Routine Declarations
 * Purpose: CPU register state struct and ISR handler declarations
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_HAL_ISR_H
#define NEXUS_HAL_ISR_H

#include <stdint.h>

/* ── Register state pushed by ISR stubs ──────────────────────────────────── */

typedef struct __attribute__((packed)) {
    /* Pushed by our stub: general-purpose registers */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;

    /* Pushed by our stub */
    uint64_t int_no;    /* Interrupt number */
    uint64_t err_code;  /* Error code (or 0 if none) */

    /* Pushed by CPU automatically */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

/* ── ISR handler type ────────────────────────────────────────────────────── */

typedef void (*isr_handler_t)(registers_t *regs);

/* Register an ISR handler for interrupt number n (0-255) */
void isr_register_handler(uint8_t n, isr_handler_t handler);

/* ── interrupt number constants ──────────────────────────────────────────── */

/* CPU Exceptions (0-31) */
#define ISR_DIVIDE_ERROR          0
#define ISR_DEBUG                 1
#define ISR_NMI                   2
#define ISR_BREAKPOINT            3
#define ISR_OVERFLOW              4
#define ISR_BOUND_RANGE           5
#define ISR_INVALID_OPCODE        6
#define ISR_DEVICE_NOT_AVAIL      7
#define ISR_DOUBLE_FAULT          8
#define ISR_COPROCESSOR_SEG       9
#define ISR_INVALID_TSS          10
#define ISR_SEGMENT_NOT_PRESENT  11
#define ISR_STACK_SEGMENT        12
#define ISR_GENERAL_PROTECTION   13
#define ISR_PAGE_FAULT           14
#define ISR_X87_FP               16
#define ISR_ALIGNMENT_CHECK      17
#define ISR_MACHINE_CHECK        18
#define ISR_SIMD_FP              19

/* IRQ offsets (remapped PIC: IRQ0-15 → INT 32-47) */
#define IRQ_BASE    32
#define IRQ0        32      /* PIT Timer */
#define IRQ1        33      /* PS/2 Keyboard */
#define IRQ2        34      /* Cascade (PIC2) */
#define IRQ3        35      /* COM2 */
#define IRQ4        36      /* COM1 */
#define IRQ5        37      /* LPT2 */
#define IRQ6        38      /* Floppy */
#define IRQ7        39      /* LPT1 / Spurious */
#define IRQ8        40      /* RTC */
#define IRQ9        41      /* Free */
#define IRQ10       42      /* Free */
#define IRQ11       43      /* Free */
#define IRQ12       44      /* PS/2 Mouse */
#define IRQ13       45      /* FPU */
#define IRQ14       46      /* Primary ATA */
#define IRQ15       47      /* Secondary ATA */

#endif /* NEXUS_HAL_ISR_H */
