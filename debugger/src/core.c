#include "debugger.h"
#include <lib/string.h>

void debugger_serial_init(void);
bool debugger_receive_packet(char *out_buf, size_t max_len);
void debugger_send_response(const char *json);
void debugger_send_event(const char *type, const char *msg);

static bool debugger_present = false;

void debugger_init(void) {
    debugger_serial_init();
    debugger_present = true;
    debugger_send_event("init", "NexusDebugger Active");
}

bool debugger_is_present(void) {
    return debugger_present;
}

/* 
 * Main interaction loop when the CPU hits a breakpoint/exception.
 */
void debugger_main(debugger_context_t *ctx) {
    if (!debugger_present) return;

    /* Tell the agent we've hit a trap */
    char trap_msg[64];
    // Simple placeholder for formatting
    strcpy(trap_msg, "Hit trap number ");
    // Convert int_no to string manually or use kprintf if available
    // For now, just send the event
    debugger_send_event("trap", "Breakpoint or Debug Exception");

    char cmd_buf[1024];
    bool continuing = false;

    while (!continuing) {
        if (debugger_receive_packet(cmd_buf, sizeof(cmd_buf))) {
            /* 
             * Simple command processor for the agent.
             * In a full implementation, we'd use a JSON parser.
             * For this Phase 1, we'll do string matching on keys.
             */
            if (strstr(cmd_buf, "\"cmd\":\"get_regs\"")) {
                /* Return JSON of registers (placeholder logic) */
                debugger_send_response("{\"status\":\"ok\",\"regs\":{\"rax\":\"0x...\"}}");
            } 
            else if (strstr(cmd_buf, "\"cmd\":\"continue\"")) {
                debugger_send_response("{\"status\":\"ok\",\"msg\":\"continuing\"}");
                continuing = true;
            }
            else {
                debugger_send_response("{\"status\":\"error\",\"msg\":\"unknown command\"}");
            }
        }
    }
}

void debugger_panic_hook(const char *msg, debugger_context_t *ctx) {
    if (!debugger_present) return;
    debugger_send_event("panic", msg);
    debugger_main(ctx);
}

/* Legacy compatibility stubs */
void debug_init(void) { debugger_init(); }
void debug_set_mode(int mode) { (void)mode; }
void debug_log(debug_level_t level, const char *component, const char *fmt, ...) {
    /* For now, just use our event system */
    debugger_send_event("log", fmt);
}
void debug_dump_mem(const void *addr, size_t len) { (void)addr; (void)len; }
void debug_backtrace(void) { (void)0; }
