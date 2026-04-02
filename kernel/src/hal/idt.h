/* ============================================================================
 * idt.h — NexusOS Interrupt Descriptor Table
 * Purpose: 64-bit IDT structures and initialization
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_HAL_IDT_H
#define NEXUS_HAL_IDT_H

#include <stdint.h>

/* ── IDT Entry (16 bytes in long mode) ───────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t offset_low;      /* Offset bits 0-15 */
    uint16_t selector;        /* Code segment selector */
    uint8_t  ist;             /* IST index (bits 0-2), zero = no IST */
    uint8_t  type_attr;       /* Type + attributes (P, DPL, type) */
    uint16_t offset_mid;      /* Offset bits 16-31 */
    uint32_t offset_high;     /* Offset bits 32-63 */
    uint32_t reserved;        /* Must be zero */
} idt_entry_t;

/* ── IDT Pointer (IDTR) ─────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t limit;           /* Size of IDT - 1 */
    uint64_t base;            /* Linear address of IDT */
} idt_pointer_t;

/* ── Gate types ──────────────────────────────────────────────────────────── */

#define IDT_GATE_INTERRUPT  0x8E    /* P=1, DPL=0, Type=InterruptGate64 */
#define IDT_GATE_TRAP       0x8F    /* P=1, DPL=0, Type=TrapGate64 */
#define IDT_GATE_USER_INT   0xEE    /* P=1, DPL=3, Type=InterruptGate64 */

/* ── Functions ───────────────────────────────────────────────────────────── */

/* Initialize the IDT with all 256 entries */
void idt_init(void);

/* Set a specific IDT entry */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector,
                  uint8_t ist, uint8_t type_attr);

#endif /* NEXUS_HAL_IDT_H */
