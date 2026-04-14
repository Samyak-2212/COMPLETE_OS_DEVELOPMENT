#ifndef NEXUS_DEBUGGER_H
#define NEXUS_DEBUGGER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Debug levels (Legacy compatibility) */
typedef enum {
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_WARN,
    DEBUG_LEVEL_ERROR,
    DEBUG_LEVEL_PANIC
} debug_level_t;

/* Debug modes */
#define DEBUG_MODE_JSON    0
#define DEBUG_MODE_HR      1

/* Trap context structure (saved by trap.asm) */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} debugger_context_t;

#ifdef DEBUGGER_ENABLED

/* Core API */
void debugger_init(void);
void debugger_main(debugger_context_t *ctx);
void debugger_panic_hook(const char *msg, debugger_context_t *ctx);

/* Legacy Compatibility API */
void debug_init(void);
void debug_set_mode(int mode);
void debug_log(debug_level_t level, const char *component, const char *fmt, ...);
void debug_dump_mem(const void *addr, size_t len);
void debug_backtrace(void);

/* Extensions for Agents */
void debugger_break(void);
void debugger_watch(void *addr, size_t len, int type);
bool debugger_is_present(void);

#else

/* No-Op Implementations for Release Builds */
#define debugger_init()            ((void)0)
#define debugger_main(ctx)         ((void)0)
#define debugger_panic_hook(m, c)  ((void)0)

#define debug_init()               ((void)0)
#define debug_set_mode(m)          ((void)0)
#define debug_log(l, c, f, ...)    ((void)0)
#define debug_dump_mem(a, l)       ((void)a, (void)l)
#define debug_backtrace()          ((void)0)

#define debugger_break()           ((void)0)
#define debugger_watch(a, l, t)    ((void)0)
#define debugger_is_present()      (false)

#endif /* DEBUGGER_ENABLED */

#endif /* NEXUS_DEBUGGER_H */
