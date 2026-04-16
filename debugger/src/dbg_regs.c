/* ============================================================================
 * dbg_regs.c — NexusOS Debugger Register Inspector
 * Purpose: Read and print all CPU registers. Registered as 'regs' command.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

void dbg_serial_printf(const char *fmt, ...);

/*
 * dbg_regs_dump — Read live CPU registers via inline asm and print them.
 */
void dbg_regs_dump(void) {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8,  r9,  r10, r11, r12, r13, r14, r15;
    uint64_t rflags, cr0, cr2, cr3, cr4;
    uint64_t dr0, dr1, dr2, dr3, dr7;

    /* Read general-purpose registers */
    __asm__ volatile ("mov %%rax, %0" : "=r"(rax));
    __asm__ volatile ("mov %%rbx, %0" : "=r"(rbx));
    __asm__ volatile ("mov %%rcx, %0" : "=r"(rcx));
    __asm__ volatile ("mov %%rdx, %0" : "=r"(rdx));
    __asm__ volatile ("mov %%rsi, %0" : "=r"(rsi));
    __asm__ volatile ("mov %%rdi, %0" : "=r"(rdi));
    __asm__ volatile ("mov %%rbp, %0" : "=r"(rbp));
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    __asm__ volatile ("mov %%r8,  %0" : "=r"(r8));
    __asm__ volatile ("mov %%r9,  %0" : "=r"(r9));
    __asm__ volatile ("mov %%r10, %0" : "=r"(r10));
    __asm__ volatile ("mov %%r11, %0" : "=r"(r11));
    __asm__ volatile ("mov %%r12, %0" : "=r"(r12));
    __asm__ volatile ("mov %%r13, %0" : "=r"(r13));
    __asm__ volatile ("mov %%r14, %0" : "=r"(r14));
    __asm__ volatile ("mov %%r15, %0" : "=r"(r15));

    /* Read control registers */
    __asm__ volatile ("pushfq; pop %0"   : "=r"(rflags));
    __asm__ volatile ("mov %%cr0, %0"    : "=r"(cr0));
    __asm__ volatile ("mov %%cr2, %0"    : "=r"(cr2));
    __asm__ volatile ("mov %%cr3, %0"    : "=r"(cr3));
    __asm__ volatile ("mov %%cr4, %0"    : "=r"(cr4));

    /* Read debug registers */
    __asm__ volatile ("mov %%dr0, %0"    : "=r"(dr0));
    __asm__ volatile ("mov %%dr1, %0"    : "=r"(dr1));
    __asm__ volatile ("mov %%dr2, %0"    : "=r"(dr2));
    __asm__ volatile ("mov %%dr3, %0"    : "=r"(dr3));
    __asm__ volatile ("mov %%dr7, %0"    : "=r"(dr7));

    dbg_serial_printf("--- Registers ---\r\n");
    dbg_serial_printf("RAX=%llx  RBX=%llx  RCX=%llx  RDX=%llx\r\n", rax, rbx, rcx, rdx);
    dbg_serial_printf("RSI=%llx  RDI=%llx  RBP=%llx  RSP=%llx\r\n", rsi, rdi, rbp, rsp);
    dbg_serial_printf("R8 =%llx  R9 =%llx  R10=%llx  R11=%llx\r\n", r8,  r9,  r10, r11);
    dbg_serial_printf("R12=%llx  R13=%llx  R14=%llx  R15=%llx\r\n", r12, r13, r14, r15);
    dbg_serial_printf("RFLAGS=%llx\r\n", rflags);
    dbg_serial_printf("CR0=%llx  CR2=%llx  CR3=%llx  CR4=%llx\r\n", cr0, cr2, cr3, cr4);
    dbg_serial_printf("DR0=%llx  DR1=%llx  DR2=%llx  DR3=%llx  DR7=%llx\r\n",
                      dr0, dr1, dr2, dr3, dr7);
    dbg_serial_printf("--- End Registers ---\r\n");
}

/* Command handler registered via DBG_REGISTER_COMMAND in dbg_console.c */
int dbg_cmd_regs(int argc, char **argv) {
    (void)argc; (void)argv;
    dbg_regs_dump();
    return 0;
}
DBG_REGISTER_COMMAND(regs, "regs", "Dump all CPU registers", dbg_cmd_regs);

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
