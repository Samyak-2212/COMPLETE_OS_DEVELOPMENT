/* ============================================================================
 * gdt.h — NexusOS Global Descriptor Table
 * Purpose: 64-bit GDT + TSS structures and initialization
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_HAL_GDT_H
#define NEXUS_HAL_GDT_H

#include <stdint.h>

/* ── GDT Entry (8 bytes) ────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t limit_low;       /* Limit bits 0-15 */
    uint16_t base_low;        /* Base bits 0-15 */
    uint8_t  base_mid;        /* Base bits 16-23 */
    uint8_t  access;          /* Access byte */
    uint8_t  granularity;     /* Limit 16-19 + flags */
    uint8_t  base_high;       /* Base bits 24-31 */
} gdt_entry_t;

/* ── TSS Entry in GDT (16 bytes — 2 consecutive GDT slots) ──────────── */

typedef struct __attribute__((packed)) {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} gdt_tss_entry_t;

/* ── GDT Pointer (GDTR) ─────────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t limit;           /* Size of GDT - 1 */
    uint64_t base;            /* Linear address of GDT */
} gdt_pointer_t;

/* ── Task State Segment (TSS) ────────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;            /* Stack pointer for ring 0 */
    uint64_t rsp1;            /* Stack pointer for ring 1 */
    uint64_t rsp2;            /* Stack pointer for ring 2 */
    uint64_t reserved1;
    uint64_t ist1;            /* Interrupt Stack Table entry 1 */
    uint64_t ist2;            /* IST entry 2 */
    uint64_t ist3;            /* IST entry 3 */
    uint64_t ist4;            /* IST entry 4 */
    uint64_t ist5;            /* IST entry 5 */
    uint64_t ist6;            /* IST entry 6 */
    uint64_t ist7;            /* IST entry 7 */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;     /* I/O permission bitmap offset */
} tss_t;

/* ── Segment selectors ───────────────────────────────────────────────────── */

#define GDT_KERNEL_CODE  0x08   /* Kernel code segment (index 1) */
#define GDT_KERNEL_DATA  0x10   /* Kernel data segment (index 2) */
#define GDT_USER_CODE    0x18   /* User code segment (index 3) */
#define GDT_USER_DATA    0x20   /* User data segment (index 4) */
#define GDT_TSS          0x28   /* TSS segment (index 5, 16 bytes) */

/* ── Functions ───────────────────────────────────────────────────────────── */

/* Initialize the GDT with kernel/user segments and TSS */
void gdt_init(void);

/* Set the RSP0 field in the TSS (kernel stack for ring transitions) */
void tss_set_rsp0(uint64_t rsp0);

/* Assembly function to flush GDT and reload segment registers */
extern void gdt_flush(uint64_t gdtr_ptr);

#endif /* NEXUS_HAL_GDT_H */
