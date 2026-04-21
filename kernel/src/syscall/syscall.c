/* syscall.c — System Call Interface Initialization */

#include "syscall.h"
#include "hal/io.h"     /* rdmsr/wrmsr */
#include "hal/gdt.h"    /* segment selectors */
#include "lib/printf.h"

/* MSR addresses */
#define MSR_EFER      0xC0000080
#define MSR_STAR      0xC0000081
#define MSR_LSTAR     0xC0000082
#define MSR_FMASK     0xC0000084

/* No local debug_log, we just use kprintf */

/* External: assembly entry point in syscall_entry.asm */
extern void syscall_entry(void);

int syscall_init(void) {
    /* 1. Enable SCE (System Call Extensions) bit in EFER */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= (1ULL << 0);  /* SCE bit */
    wrmsr(MSR_EFER, efer);

    /* 2. Set STAR */
    /* After swap: GDT_USER_DATA is 0x18, GDT_USER_CODE is 0x20 */
    /* SYSRET_CS should be GDT_USER_DATA - 8 = 0x18 - 8 = 0x10.
       Wait, if SYSRET_CS is 0x10:
       SYSRET SS = 0x10 + 8 = 0x18 (User Data) -> Correct!
       SYSRET CS = 0x10 + 16 = 0x20 (User Code) -> Correct!
       So User CS in STAR[63:48] should just be 0x10!
       Wait, wait.
       Actually, normally people use the RPL | 3 in STAR[63:48] due to documentation implying it's needed, even if ignored by CPU. So 0x10 | 3 = 0x13.
       Let's use 0x13.
    */
    uint64_t star = 0;
    star |= ((uint64_t)0x0008 << 32);  /* kernel code selector (CS=0x8, SS=0x10) */
    star |= ((uint64_t)0x0013 << 48);  /* 0x10 | 3 */
    wrmsr(MSR_STAR, star);

    /* 3. Set LSTAR: kernel syscall entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. Set FMASK: mask IF */
    wrmsr(MSR_FMASK, (1ULL << 9));

    kprintf("[SYSCALL] Interface initialized (LSTAR=0x%016llx)\n", (unsigned long long)syscall_entry);
    return 0;
}
