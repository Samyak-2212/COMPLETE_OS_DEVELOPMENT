/* ============================================================================
 * dbg_shell_bridge.c — NexusOS Debugger Shell Command Bridge
 * Purpose: Inject commands into the kernel shell from the debugger console.
 *          Output is automatically captured because kprintf → dbg_serial_putc.
 * Author: NexusOS Debugger Team
 * ============================================================================ */

#include "nexus_debug.h"

#if DEBUG_LEVEL >= DBG_LODBUG

void dbg_serial_printf(const char *fmt, ...);

/* Forward declaration — defined in shell_core.c */
extern int shell_execute(const char *line);

/*
 * dbg_shell_inject — Execute a shell command as if the user typed it.
 * All kprintf output from the command appears on the serial terminal
 * automatically since kprintf calls dbg_serial_putc.
 */
void dbg_shell_inject(const char *cmd) {
    dbg_serial_printf("$ %s\r\n", cmd);
    shell_execute(cmd);
}

#endif /* DEBUG_LEVEL >= DBG_LODBUG */
