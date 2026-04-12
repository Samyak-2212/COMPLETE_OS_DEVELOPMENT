#ifndef NEXUS_LIB_KLOG_H
#define NEXUS_LIB_KLOG_H

#include <stdint.h>
#include <stddef.h>

/* Kernel Log API */
void klog_init(void);
void klog_putc(char c);

/* Dump current logs to kprintf */
void klog_dump(void);

/* Stop capturing logs (to isolate boot process) */
void klog_stop_recording(void);

#endif /* NEXUS_LIB_KLOG_H */
