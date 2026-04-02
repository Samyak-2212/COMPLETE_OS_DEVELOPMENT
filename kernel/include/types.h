/* ============================================================================
 * types.h — NexusOS Kernel Global Types
 * Purpose: Common type definitions (supplements freestanding headers)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_TYPES_H
#define NEXUS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Physical address type */
typedef uint64_t phys_addr_t;

/* Virtual address type */
typedef uint64_t virt_addr_t;

/* Process/thread identifiers */
typedef int32_t  pid_t;
typedef int32_t  tid_t;

/* Status return type */
typedef int      status_t;

/* ── Status codes ────────────────────────────────────────────────────────── */

#define STATUS_OK       0
#define STATUS_ERROR   (-1)
#define STATUS_NOMEM   (-2)
#define STATUS_INVAL   (-3)
#define STATUS_NOENT   (-4)
#define STATUS_BUSY    (-5)
#define STATUS_PERM    (-6)

/* ── Useful macros ───────────────────────────────────────────────────────── */

#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))
#define ALIGN_UP(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a)   ((x) & ~((a) - 1))
#define MIN(a, b)          ((a) < (b) ? (a) : (b))
#define MAX(a, b)          ((a) > (b) ? (a) : (b))

/* Page size constant */
#define PAGE_SIZE          4096ULL
#define PAGE_SHIFT         12

/* Higher-half kernel virtual base */
#define KERNEL_VIRT_BASE   0xFFFFFFFF80000000ULL

#endif /* NEXUS_TYPES_H */
