#include "debugger.h"

/* 
 * Hardware Breakpoint Manager (x86_64)
 */

void debugger_hw_set_breakpoint(int slot, uintptr_t addr) {
    if (slot < 0 || slot > 3) return;

    switch (slot) {
        case 0: __asm__ volatile ("mov %0, %%dr0" : : "r"(addr)); break;
        case 1: __asm__ volatile ("mov %0, %%dr1" : : "r"(addr)); break;
        case 2: __asm__ volatile ("mov %0, %%dr2" : : "r"(addr)); break;
        case 3: __asm__ volatile ("mov %0, %%dr3" : : "r"(addr)); break;
    }

    /* Enable in DR7 */
    uint64_t dr7;
    __asm__ volatile ("mov %%dr7, %0" : "=r"(dr7));
    dr7 |= (1ULL << (slot * 2)); /* Local enable */
    __asm__ volatile ("mov %0, %%dr7" : : "r"(dr7));
}

void debugger_hw_clear_breakpoint(int slot) {
    if (slot < 0 || slot > 3) return;

    uint64_t dr7;
    __asm__ volatile ("mov %%dr7, %0" : "=r"(dr7));
    dr7 &= ~(1ULL << (slot * 2));
    __asm__ volatile ("mov %0, %%dr7" : : "r"(dr7));
}

void debugger_watch(void *addr, size_t len, int type) {
    /* 
     * Simplified: Use slot 0 for the watchpoint.
     * type: 1 = write, 3 = read/write
     * len: 0=1byte, 1=2byte, 2=8byte, 3=4byte
     */
    uintptr_t address = (uintptr_t)addr;
    __asm__ volatile ("mov %0, %%dr0" : : "r"(address));

    uint64_t dr7;
    __asm__ volatile ("mov %%dr7, %0" : "=r"(dr7));
    
    dr7 |= (1ULL << 0);    /* Local enable L0 */
    dr7 |= (3ULL << 16);   /* RW0 = 11 (Read/Write) */
    dr7 |= (2ULL << 18);   /* LEN0 = 10 (8 bytes for x64) */
    
    __asm__ volatile ("mov %0, %%dr7" : : "r"(dr7));
}
