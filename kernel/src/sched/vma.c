/* ============================================================================
 * vma.c — NexusOS Virtual Memory Area Management
 * Purpose: Track valid user memory regions (BSS, Code, Data)
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "sched/process.h"
#include "mm/heap.h"
#include "lib/debug.h"

void process_add_vma(process_t *proc, uint64_t start, uint64_t end, uint32_t flags) {
    if (!proc) return;

    vma_t *vma = (vma_t *)kmalloc(sizeof(vma_t));
    if (!vma) {
        debug_log(DEBUG_LEVEL_ERROR, "VMA", "Failed to allocate VMA for PID %d", proc->pid);
        return;
    }

    vma->virt_start = start;
    vma->virt_end   = end;
    vma->flags      = flags;
    vma->next       = proc->vmas;
    proc->vmas      = vma;

    debug_log(DEBUG_LEVEL_INFO, "VMA", "PID %d: Added VMA [0x%llx - 0x%llx] flags=0x%x",
              proc->pid, (unsigned long long)start, (unsigned long long)end, flags);
}

vma_t *process_find_vma(process_t *proc, uint64_t addr) {
    if (!proc) return NULL;

    vma_t *curr = proc->vmas;
    while (curr) {
        if (addr >= curr->virt_start && addr < curr->virt_end) {
            return curr;
        }
        curr = curr->next;
    }

    return NULL;
}
