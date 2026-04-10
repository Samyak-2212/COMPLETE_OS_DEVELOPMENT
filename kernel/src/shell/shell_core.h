/* ============================================================================
 * shell_core.h — NexusOS Kernel Shell Public API
 * Purpose: Declares the two functions kernel.c calls to start the shell.
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_SHELL_CORE_H
#define NEXUS_SHELL_CORE_H

/* Launch the interactive kernel shell — blocks forever */
void shell_run(void);

/* Execute a single null-terminated command line — returns handler ret or -1 */
int shell_execute(const char *line);

#endif /* NEXUS_SHELL_CORE_H */
