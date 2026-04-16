/* ============================================================================
 * dbg_breakpoint.c — NexusOS Debugger Hardware Breakpoints
 * Purpose: Manage x86_64 DR0-DR3 hardware breakpoints and DR7 watchpoints.
 *          Registered as 'bp' command in the interactive console.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

void dbg_serial_printf(const char *fmt, ...);

/* ── Internal state ───────────────────────────────────────────────────────── */

static uint64_t bp_addr[4]   = {0, 0, 0, 0};
static bool     bp_active[4] = {false, false, false, false};

/* ── DR register helpers ──────────────────────────────────────────────────── */

static void dr_write(int slot, uint64_t val) {
    switch (slot) {
        case 0: __asm__ volatile ("mov %0, %%dr0" : : "r"(val)); break;
        case 1: __asm__ volatile ("mov %0, %%dr1" : : "r"(val)); break;
        case 2: __asm__ volatile ("mov %0, %%dr2" : : "r"(val)); break;
        case 3: __asm__ volatile ("mov %0, %%dr3" : : "r"(val)); break;
    }
}

static uint64_t dr_read(int slot) {
    uint64_t val = 0;
    switch (slot) {
        case 0: __asm__ volatile ("mov %%dr0, %0" : "=r"(val)); break;
        case 1: __asm__ volatile ("mov %%dr1, %0" : "=r"(val)); break;
        case 2: __asm__ volatile ("mov %%dr2, %0" : "=r"(val)); break;
        case 3: __asm__ volatile ("mov %%dr3, %0" : "=r"(val)); break;
    }
    return val;
}

static uint64_t dr7_read(void) {
    uint64_t val;
    __asm__ volatile ("mov %%dr7, %0" : "=r"(val));
    return val;
}

static void dr7_write(uint64_t val) {
    __asm__ volatile ("mov %0, %%dr7" : : "r"(val));
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * dbg_bp_set — Set a hardware execution breakpoint on addr.
 * Returns the slot index (0-3) or -1 if all four slots are occupied.
 */
int dbg_bp_set(uint64_t addr) {
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (!bp_active[i]) { slot = i; break; }
    }
    if (slot < 0) {
        dbg_serial_printf("bp: all 4 slots occupied\r\n");
        return -1;
    }

    bp_addr[slot]   = addr;
    bp_active[slot] = true;
    dr_write(slot, addr);

    /* Enable local enable bit for this slot in DR7 */
    uint64_t dr7 = dr7_read();
    dr7 |= (1ULL << (slot * 2));   /* Local enable Ln */
    dr7_write(dr7);

    dbg_serial_printf("bp: slot %d set at 0x%llx\r\n", slot, addr);
    return slot;
}

/*
 * dbg_bp_clear — Clear hardware breakpoint in the given slot (0-3).
 */
void dbg_bp_clear(int slot) {
    if (slot < 0 || slot > 3) {
        dbg_serial_printf("bp: invalid slot %d\r\n", slot);
        return;
    }
    bp_addr[slot]   = 0;
    bp_active[slot] = false;
    dr_write(slot, 0);

    uint64_t dr7 = dr7_read();
    dr7 &= ~(1ULL << (slot * 2));  /* Clear local enable Ln */
    dr7_write(dr7);

    dbg_serial_printf("bp: slot %d cleared\r\n", slot);
}

/*
 * dbg_bp_list — Print status of all four hardware breakpoint slots.
 */
void dbg_bp_list(void) {
    dbg_serial_printf("Hardware breakpoints:\r\n");
    for (int i = 0; i < 4; i++) {
        if (bp_active[i]) {
            dbg_serial_printf("  [%d] ACTIVE  addr=0x%llx\r\n", i, bp_addr[i]);
        } else {
            dbg_serial_printf("  [%d] empty\r\n", i);
        }
    }
}

/*
 * dbg_watch_set — Set a data watchpoint in DR slot 0 (simplified).
 * len_code: 0=1byte 1=2bytes 2=8bytes 3=4bytes
 * type: 1=write  3=read/write
 */
void dbg_watch_set(uint64_t addr, int len_code, int type) {
    /* Use slot 0 for watchpoints */
    bp_addr[0]   = addr;
    bp_active[0] = true;
    dr_write(0, addr);

    uint64_t dr7 = dr7_read();
    dr7 |=  (1ULL << 0);                   /* L0 enable */
    dr7 &= ~(3ULL << 16);
    dr7 |=  ((uint64_t)(type & 3) << 16);  /* RW0 */
    dr7 &= ~(3ULL << 18);
    dr7 |=  ((uint64_t)(len_code & 3) << 18); /* LEN0 */
    dr7_write(dr7);

    dbg_serial_printf("watchpoint set: addr=0x%llx len=%d type=%d\r\n",
                      addr, len_code, type);
}

/* ── Command handlers ─────────────────────────────────────────────────────── */

/* Convert ASCII hex string to uint64_t (simple, no stdlib) */
static uint64_t parse_hex(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    uint64_t val = 0;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9')      val = val * 16 + (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') val = val * 16 + (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') val = val * 16 + (uint64_t)(c - 'A' + 10);
        else break;
    }
    return val;
}

static uint64_t parse_uint(const char *s) {
    uint64_t val = 0;
    while (*s >= '0' && *s <= '9') val = val * 10 + (uint64_t)(*s++ - '0');
    return val;
}

/*
 * bp command: bp set <addr> | bp clear <n> | bp list
 */
static int dbg_cmd_bp(int argc, char **argv) {
    if (argc < 2) {
        dbg_bp_list();
        return 0;
    }
    if (argv[1][0] == 's' && argc >= 3) {          /* set */
        dbg_bp_set(parse_hex(argv[2]));
    } else if (argv[1][0] == 'c' && argc >= 3) {   /* clear */
        dbg_bp_clear((int)parse_uint(argv[2]));
    } else if (argv[1][0] == 'l') {                 /* list */
        dbg_bp_list();
    } else {
        dbg_serial_printf("Usage: bp [set <addr> | clear <n> | list]\r\n");
    }
    return 0;
}
DBG_REGISTER_COMMAND(bp, "bp [set <addr>|clear <n>|list]", "Hardware breakpoints", dbg_cmd_bp);

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
