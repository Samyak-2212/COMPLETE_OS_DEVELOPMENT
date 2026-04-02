/* ============================================================================
 * io.h — NexusOS I/O Port Helpers
 * Purpose: Inline assembly wrappers for x86 port I/O instructions
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_HAL_IO_H
#define NEXUS_HAL_IO_H

#include <stdint.h>

/* Read a byte from an I/O port */
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write a byte to an I/O port */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a 16-bit word from an I/O port */
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write a 16-bit word to an I/O port */
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* Read a 32-bit dword from an I/O port */
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* Write a 32-bit dword to an I/O port */
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

/* Small delay for slow I/O devices (writes to unused port 0x80) */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Disable interrupts */
static inline void cli(void) {
    __asm__ volatile ("cli");
}

/* Enable interrupts */
static inline void sti(void) {
    __asm__ volatile ("sti");
}

/* Halt the CPU until next interrupt */
static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

/* Read CR3 (page table base) */
static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(val));
    return val;
}

/* Write CR3 (page table base) — flushes entire TLB */
static inline void write_cr3(uint64_t val) {
    __asm__ volatile ("mov %0, %%cr3" : : "r"(val) : "memory");
}

/* Invalidate a single TLB entry */
static inline void invlpg(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Read MSR */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

/* Write MSR */
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

#endif /* NEXUS_HAL_IO_H */
