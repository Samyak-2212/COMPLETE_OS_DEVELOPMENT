/* ============================================================================
 * klog.c — NexusOS Kernel Log Buffer
 * Purpose: Circular buffer for capturing all kprintf output
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "lib/klog.h"
#include "lib/printf.h"
#include <stdbool.h>

#define KLOG_BUFFER_SIZE (64 * 1024)  /* 64 KB */

static char klog_buffer[KLOG_BUFFER_SIZE];
static uint32_t klog_head = 0;
static bool klog_wrapped = false;
static bool klog_active = false;

void klog_init(void) {
    klog_head = 0;
    klog_wrapped = false;
    klog_active = true;
}

void klog_putc(char c) {
    if (!klog_active) return;
    
    klog_buffer[klog_head++] = c;
    if (klog_head >= KLOG_BUFFER_SIZE) {
        klog_head = 0;
        klog_wrapped = true;
    }
}

void klog_dump(void) {
    klog_active = false; /* Disable logging while dumping to avoid loops */
    
    if (klog_wrapped) {
        /* Print from head to end */
        for (uint32_t i = klog_head; i < KLOG_BUFFER_SIZE; i++) {
            kputchar(klog_buffer[i]);
        }
    }
    
    /* Print from start to head */
    for (uint32_t i = 0; i < klog_head; i++) {
        kputchar(klog_buffer[i]);
    }
    
    klog_active = true;
}
