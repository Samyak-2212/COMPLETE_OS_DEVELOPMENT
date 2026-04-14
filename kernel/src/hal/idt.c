/* ============================================================================
 * idt.c — NexusOS Interrupt Descriptor Table
 * Purpose: Initialize 256-entry IDT, wire up ISR stubs, handle interrupts
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "hal/idt.h"
#include "hal/isr.h"
#include "hal/gdt.h"
#include "hal/pic.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "lib/debug.h"

/* ── IDT and handler table ──────────────────────────────────────────────── */

static idt_entry_t idt[256];
static idt_pointer_t idtr;
static isr_handler_t isr_handlers[256];

/* ── External ISR stubs (defined in isr_stubs.asm) ─────────────────────── */

extern void isr_stub_0(void);   extern void isr_stub_1(void);
extern void isr_stub_2(void);   extern void isr_stub_3(void);
extern void isr_stub_4(void);   extern void isr_stub_5(void);

#ifdef DEBUGGER_ENABLED
extern void debugger_trap_entry_int1(void);
extern void debugger_trap_entry_int3(void);
extern void debugger_trap_entry_int8(void);
#endif
extern void isr_stub_6(void);   extern void isr_stub_7(void);
extern void isr_stub_8(void);   extern void isr_stub_9(void);
extern void isr_stub_10(void);  extern void isr_stub_11(void);
extern void isr_stub_12(void);  extern void isr_stub_13(void);
extern void isr_stub_14(void);  extern void isr_stub_15(void);
extern void isr_stub_16(void);  extern void isr_stub_17(void);
extern void isr_stub_18(void);  extern void isr_stub_19(void);
extern void isr_stub_20(void);  extern void isr_stub_21(void);
extern void isr_stub_22(void);  extern void isr_stub_23(void);
extern void isr_stub_24(void);  extern void isr_stub_25(void);
extern void isr_stub_26(void);  extern void isr_stub_27(void);
extern void isr_stub_28(void);  extern void isr_stub_29(void);
extern void isr_stub_30(void);  extern void isr_stub_31(void);

/* IRQ stubs (32-47) */
extern void isr_stub_32(void);  extern void isr_stub_33(void);
extern void isr_stub_34(void);  extern void isr_stub_35(void);
extern void isr_stub_36(void);  extern void isr_stub_37(void);
extern void isr_stub_38(void);  extern void isr_stub_39(void);
extern void isr_stub_40(void);  extern void isr_stub_41(void);
extern void isr_stub_42(void);  extern void isr_stub_43(void);
extern void isr_stub_44(void);  extern void isr_stub_45(void);
extern void isr_stub_46(void);  extern void isr_stub_47(void);

/* ── Exception names ────────────────────────────────────────────────────── */

static const char *exception_names[32] = {
    "Division Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FP Exception", "Alignment Check", "Machine Check", "SIMD FP Exception",
    "Virtualization Exception", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

/* ── IDT gate setup ─────────────────────────────────────────────────────── */

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector,
                  uint8_t ist, uint8_t type_attr) {
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = ist & 0x07;
    idt[num].type_attr   = type_attr;
    idt[num].reserved    = 0;
}

/* ── C-level ISR dispatcher (called from assembly stubs) ────────────────── */

void isr_handler(registers_t *regs) {
    uint64_t int_no = regs->int_no;

    static int log_count = 0;
    if (log_count < 20) {
        debug_log(DEBUG_LEVEL_INFO, "IDT", "Interrupt %d received (count=%d)", (int)int_no, log_count++);
    }

    /* Hardware IRQ unhandled? Log and EOI */
    if (int_no >= 32 && int_no <= 47 && !isr_handlers[int_no]) {
        debug_log(DEBUG_LEVEL_WARN, "IDT", "Unhandled IRQ %d. Sending EOI.", (int)(int_no - 32));
        pic_send_eoi((uint8_t)(int_no - 32));
        return;
    }

    /* If we have a registered handler, call it */
    if (int_no < 256 && isr_handlers[int_no]) {
        isr_handlers[int_no](regs);
        return;
    }

    /* Unhandled CPU exception → panic */
    if (int_no < 32) {
        kprintf_set_color(0x00FF4444, 0x00000000);
        kprintf("\n!!! EXCEPTION: %s (#%llu)\n", exception_names[int_no],
                (unsigned long long)int_no);
        kprintf("    Error code: 0x%016llx\n", (unsigned long long)regs->err_code);
        kprintf("    RIP: 0x%016llx  RSP: 0x%016llx\n",
                (unsigned long long)regs->rip,
                (unsigned long long)regs->rsp);
        kprintf("    RAX: 0x%016llx  RBX: 0x%016llx\n",
                (unsigned long long)regs->rax,
                (unsigned long long)regs->rbx);
        kprintf("    RCX: 0x%016llx  RDX: 0x%016llx\n",
                (unsigned long long)regs->rcx,
                (unsigned long long)regs->rdx);
        kprintf("    CS:  0x%04llx  SS: 0x%04llx  RFLAGS: 0x%016llx\n",
                (unsigned long long)regs->cs,
                (unsigned long long)regs->ss,
                (unsigned long long)regs->rflags);

        /* If page fault, print CR2 (faulting address) */
        if (int_no == 14) {
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            debug_log(DEBUG_LEVEL_PANIC, "IDT", "PAGE FAULT at address: 0x%016llx", (unsigned long long)cr2);
            kprintf("    CR2 (fault addr): 0x%016llx\n", (unsigned long long)cr2);
        }

        /* Halt forever */
        for (;;) {
            __asm__ volatile ("cli; hlt");
        }
    }

    /* Unhandled IRQ — send EOI and return */
    if (int_no >= 32 && int_no <= 47) {
        debug_log(DEBUG_LEVEL_WARN, "IDT", "Unhandled IRQ: %d (INT %d). Sending EOI.", (int)(int_no - 32), (int)int_no);
        pic_send_eoi((uint8_t)(int_no - 32));
    }
}

/* ── Register handler ───────────────────────────────────────────────────── */

void isr_register_handler(uint8_t n, isr_handler_t handler) {
    isr_handlers[n] = handler;
}

/* ── IDT initialization ─────────────────────────────────────────────────── */

void idt_init(void) {
    memset(&idt, 0, sizeof(idt));
    memset(isr_handlers, 0, sizeof(isr_handlers));

    /* Wire all ISR stub pointers into the IDT */
    /* Use a helper array of function pointers */
    void (*stubs[48])(void) = {
        isr_stub_0,  isr_stub_1,  isr_stub_2,  isr_stub_3,
        isr_stub_4,  isr_stub_5,  isr_stub_6,  isr_stub_7,
        isr_stub_8,  isr_stub_9,  isr_stub_10, isr_stub_11,
        isr_stub_12, isr_stub_13, isr_stub_14, isr_stub_15,
        isr_stub_16, isr_stub_17, isr_stub_18, isr_stub_19,
        isr_stub_20, isr_stub_21, isr_stub_22, isr_stub_23,
        isr_stub_24, isr_stub_25, isr_stub_26, isr_stub_27,
        isr_stub_28, isr_stub_29, isr_stub_30, isr_stub_31,
        isr_stub_32, isr_stub_33, isr_stub_34, isr_stub_35,
        isr_stub_36, isr_stub_37, isr_stub_38, isr_stub_39,
        isr_stub_40, isr_stub_41, isr_stub_42, isr_stub_43,
        isr_stub_44, isr_stub_45, isr_stub_46, isr_stub_47
    };

    /* CPU exceptions (0-31): interrupt gates, IST=0 except double fault */
    for (int i = 0; i < 32; i++) {
        uint8_t ist = (i == 8) ? 1 : 0; /* Double fault uses IST1 */
        uint64_t handler = (uint64_t)stubs[i];

#ifdef DEBUGGER_ENABLED
        if (i == 1) handler = (uint64_t)debugger_trap_entry_int1;
        if (i == 3) handler = (uint64_t)debugger_trap_entry_int3;
        if (i == 8) handler = (uint64_t)debugger_trap_entry_int8;
#endif

        idt_set_gate((uint8_t)i, handler, GDT_KERNEL_CODE,
                     ist, IDT_GATE_INTERRUPT);
    }

    /* IRQs (32-47): interrupt gates */
    for (int i = 32; i < 48; i++) {
        idt_set_gate((uint8_t)i, (uint64_t)stubs[i], GDT_KERNEL_CODE,
                     0, IDT_GATE_INTERRUPT);
    }

    /* Load IDTR */
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)&idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("IDT loaded: 256 entries (%u bytes)\n",
            (unsigned int)sizeof(idt));
}
