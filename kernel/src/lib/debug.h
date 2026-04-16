/* ============================================================================
 * debug.h — NexusOS Kernel Debug Compatibility Shim
 * Purpose: Thin wrapper that maps legacy debug_*() names to the new dbg_*()
 *          API in nexus_debug.h. Allows existing call sites (40+) to work
 *          without any source changes.
 *
 *          When DEBUG_LEVEL >= 1, includes nexus_debug.h and aliases names.
 *          When DEBUG_LEVEL == 0 (or undefined), provides standalone no-ops.
 * ============================================================================ */

#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

/* ── Log level constants (always available) ───────────────────────────────── */

#define DEBUG_LEVEL_INFO   1
#define DEBUG_LEVEL_WARN   2
#define DEBUG_LEVEL_ERROR  3
#define DEBUG_LEVEL_PANIC  4

#if defined(DEBUG_LEVEL) && DEBUG_LEVEL >= 1

/* Debugger enabled — include real API and alias old names */
#include <nexus_debug.h>

#define debug_init()                dbg_init()
#define debug_log(l, c, f, ...)     dbg_log(l, c, f, ##__VA_ARGS__)
#define debug_backtrace()           dbg_backtrace()
#define debug_dump_mem(a, l)        dbg_mem_dump(a, l)
#define debugger_panic_hook(m, c)   dbg_panic_hook(m)

#else

/* Debugger disabled — all no-ops, zero serial code */
#define debug_init()                ((void)0)
#define debug_set_mode(m)           ((void)0)
#define debug_log(l, c, f, ...)     ((void)0)
#define debug_backtrace()           ((void)0)
#define debug_dump_mem(a, l)        ((void)0)
#define debugger_panic_hook(m, c)   ((void)0)

#endif /* DEBUG_LEVEL >= 1 */

#endif /* DEBUG_H */
