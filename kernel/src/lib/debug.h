#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stddef.h>

/* Debug levels */
typedef enum {
    DEBUG_LEVEL_INFO,
    DEBUG_LEVEL_WARN,
    DEBUG_LEVEL_ERROR,
    DEBUG_LEVEL_PANIC
} debug_level_t;

/* Debug modes */
#define DEBUG_MODE_JSON    0
#define DEBUG_MODE_HR      1

/* Set global debug mode (JSON vs Human Readable) */
void debug_set_mode(int mode);

/* Initialize Debugger */
void debug_init(void);

/* Formatted log to serial */
void debug_log(debug_level_t level, const char *component, const char *fmt, ...);

/* Memory dump */
void debug_dump_mem(const void *addr, size_t len);

/* Stack trace from current location */
void debug_backtrace(void);

#endif /* DEBUG_H */
