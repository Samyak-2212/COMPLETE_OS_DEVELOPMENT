/* ============================================================================
 * gdt.c — NexusOS Global Descriptor Table
 * Purpose: Setup 64-bit GDT with kernel/user segments and TSS
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "hal/gdt.h"
#include "lib/string.h"
#include "lib/printf.h"

/* ── GDT table ──────────────────────────────────────────────────────────── */
/* Layout: Null | KCode64 | KData64 | UCode64 | UData64 | TSS (16 bytes) */
/* That's 5 regular entries + 1 TSS entry (= 2 slots) = 7 slots worth     */

/* We use a raw byte array to accommodate the 16-byte TSS descriptor */
static struct __attribute__((packed)) {
    gdt_entry_t     entries[5];     /* Null + 4 segment descriptors */
    gdt_tss_entry_t tss_entry;      /* TSS descriptor (16 bytes) */
} gdt_table;

static gdt_pointer_t gdtr;
static tss_t tss;

/* IST1 stack for double-fault handler (16 KiB) */
static uint8_t ist1_stack[16384] __attribute__((aligned(16)));

/* ── Helpers ────────────────────────────────────────────────────────────── */

static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt_entry_t *e = &gdt_table.entries[index];
    e->base_low    = (uint16_t)(base & 0xFFFF);
    e->base_mid    = (uint8_t)((base >> 16) & 0xFF);
    e->base_high   = (uint8_t)((base >> 24) & 0xFF);
    e->limit_low   = (uint16_t)(limit & 0xFFFF);
    e->granularity  = (uint8_t)(((limit >> 16) & 0x0F) | (gran & 0xF0));
    e->access      = access;
}

static void gdt_set_tss(uint64_t base, uint32_t limit) {
    gdt_tss_entry_t *e = &gdt_table.tss_entry;
    e->length     = (uint16_t)(limit & 0xFFFF);
    e->base_low   = (uint16_t)(base & 0xFFFF);
    e->base_mid   = (uint8_t)((base >> 16) & 0xFF);
    e->flags1     = 0x89;  /* Present, 64-bit TSS Available */
    e->flags2     = (uint8_t)((limit >> 16) & 0x0F);
    e->base_high  = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper = (uint32_t)((base >> 32) & 0xFFFFFFFF);
    e->reserved   = 0;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void gdt_init(void) {
    /* Clear everything */
    memset(&gdt_table, 0, sizeof(gdt_table));
    memset(&tss, 0, sizeof(tss));

    /* Entry 0: Null descriptor — required by x86 */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Entry 1: Kernel Code 64-bit
     * Access: Present=1, DPL=0, S=1, Type=Execute/Read = 0x9A
     * Granularity: Long mode = 0x20 (L=1, Db=0) */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x20);

    /* Entry 2: Kernel Data 64-bit
     * Access: Present=1, DPL=0, S=1, Type=Read/Write = 0x92
     * Granularity: 0x00 (data segments ignore L bit in long mode) */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x00);

    /* Entry 3: User Code 64-bit
     * Access: Present=1, DPL=3, S=1, Type=Execute/Read = 0xFA
     * Granularity: Long mode = 0x20 */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0x20);

    /* Entry 4: User Data 64-bit
     * Access: Present=1, DPL=3, S=1, Type=Read/Write = 0xF2
     * Granularity: 0x00 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x00);

    /* Setup TSS */
    tss.iopb_offset = sizeof(tss_t);
    tss.ist1 = (uint64_t)(ist1_stack + sizeof(ist1_stack)); /* Top of IST1 stack */

    /* Entry 5-6: TSS descriptor (16 bytes, spans two GDT slots) */
    gdt_set_tss((uint64_t)&tss, sizeof(tss_t) - 1);

    /* Load GDTR */
    gdtr.limit = sizeof(gdt_table) - 1;
    gdtr.base  = (uint64_t)&gdt_table;

    gdt_flush((uint64_t)&gdtr);

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("GDT loaded: %u bytes, TSS at 0x%016llx\n",
            (unsigned int)sizeof(gdt_table),
            (unsigned long long)(uint64_t)&tss);
}

/* Set the kernel stack pointer for ring 3 → ring 0 transitions */
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
