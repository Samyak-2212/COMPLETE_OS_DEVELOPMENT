#include "debugger.h"
#include <mm/vmm.h>
#include <mm/pmm.h>

/* 
 * Check if a virtual address is present and readable.
 * Traverses the 4-level page tables.
 */
static bool is_addr_safe(uintptr_t addr, bool write) {
    /* Get current CR3 (PML4 physical address) */
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    
    /* 
     * In a full implementation, we would traverse:
     * PML4 -> PDPT -> PD -> PT
     * and check the 'Present' (bit 0) and 'Writable' (bit 1) bits.
     * 
     * For Phase 1, we'll use a simplified check: 
     * If it's in the kernel's higher-half (0xffffffff80000000+), we assume it's mapped.
     * This is NOT safe enough for production, but better than nothing.
     */
    if (addr >= 0xffffffff80000000ULL) return true;
    
    return false;
}

bool debugger_mem_read(void *dest, const void *src, size_t len) {
    if (!is_addr_safe((uintptr_t)src, false)) return false;
    
    /* TODO: Implement robust byte-by-byte copy with safety check for every page boundary */
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; i++) {
        /* Re-check safety if we cross a 4KB boundary */
        if (((uintptr_t)(s + i) & 0xFFF) == 0) {
            if (!is_addr_safe((uintptr_t)(s + i), false)) return false;
        }
        d[i] = s[i];
    }
    return true;
}

bool debugger_mem_write(void *dest, const void *src, size_t len) {
    if (!is_addr_safe((uintptr_t)dest, true)) return false;
    
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; i++) {
        /* Re-check safety if we cross a 4KB boundary */
        if (((uintptr_t)(d + i) & 0xFFF) == 0) {
            if (!is_addr_safe((uintptr_t)(d + i), true)) return false;
        }
        d[i] = s[i];
    }
    return true;
}
