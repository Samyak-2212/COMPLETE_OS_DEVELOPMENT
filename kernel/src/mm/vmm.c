/* ============================================================================
 * vmm.c — NexusOS Virtual Memory Manager
 * Purpose: 4-level x86_64 paging: map/unmap pages, walk page tables
 * Author: NexusOS Kernel Team
 *
 * NOTE: During Phase 2, we continue using the Limine-provided page tables
 * (which already map the kernel in higher-half and provide HHDM).
 * We augment them by providing map/unmap functionality for future use.
 * A full page table rebuild from scratch is deferred to when we actually
 * need to create new address spaces (Phase 4 — userspace).
 * ============================================================================ */

#include "mm/vmm.h"
#include "mm/pmm.h"
#include "hal/io.h"
#include "boot/limine_requests.h"
#include "lib/string.h"
#include "lib/printf.h"
#include "sched/scheduler.h"
#include "sched/process.h"

extern void kpanic(const char *msg);

/* ── State ──────────────────────────────────────────────────────────────── */

static uint64_t hhdm_offset = 0;

/* ── Helpers ────────────────────────────────────────────────────────────── */

static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + hhdm_offset);
}

static inline uint64_t virt_to_phys_hhdm(void *virt) {
    return (uint64_t)virt - hhdm_offset;
}

static inline uint64_t *get_current_pml4(void) {
    return (uint64_t *)phys_to_virt(read_cr3() & 0x000FFFFFFFFFF000ULL);
}

/* Extract page table indices from a virtual address */
#define PML4_INDEX(addr)  (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x1FF)

/* Get or create a page table entry at a given level.
 * If create=1 and the entry is not present, allocate a new table. */
static uint64_t *vmm_get_next_level(uint64_t *table, uint64_t index, int create) {
    if (table[index] & PAGE_PRESENT) {
        /* Entry exists — follow it */
        uint64_t phys = table[index] & 0x000FFFFFFFFFF000ULL;
        return (uint64_t *)phys_to_virt(phys);
    }

    if (!create) {
        return (void *)0;
    }

    /* Allocate a new page table */
    uint64_t new_page = pmm_alloc_page();
    if (new_page == 0) {
        return (void *)0; /* Out of memory */
    }

    /* Zero it out (pmm_alloc_page already zeros, but be safe) */
    memset(phys_to_virt(new_page), 0, 4096);

    /* Install the entry with present + writable + user (permissive at higher levels) */
    table[index] = new_page | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    return (uint64_t *)phys_to_virt(new_page);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void vmm_init(void) {
    struct limine_hhdm_response *hhdm = limine_get_hhdm();
    if (!hhdm) {
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] VMM: No HHDM response\n");
        return;
    }

    hhdm_offset = hhdm->offset;

    /* Read current CR3 — Limine already set up valid page tables */
    uint64_t cr3_val = read_cr3();

    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[OK] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("VMM: Using Limine page tables, PML4 at phys 0x%016llx\n",
            (unsigned long long)(cr3_val & 0x000FFFFFFFFFF000ULL));
}

void vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pdpt = vmm_get_next_level(get_current_pml4(), PML4_INDEX(virt), 1);
    if (!pdpt) return;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 1);
    if (!pd) return;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 1);
    if (!pt) return;

    pt[PT_INDEX(virt)] = (phys & 0x000FFFFFFFFFF000ULL) | flags;
    invlpg(virt);
}

void vmm_unmap_page(uint64_t virt) {
    uint64_t *pdpt = vmm_get_next_level(get_current_pml4(), PML4_INDEX(virt), 0);
    if (!pdpt) return;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 0);
    if (!pd) return;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 0);
    if (!pt) return;

    pt[PT_INDEX(virt)] = 0;
    invlpg(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t *pdpt = vmm_get_next_level(get_current_pml4(), PML4_INDEX(virt), 0);
    if (!pdpt) return 0;

    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(virt), 0);
    if (!pd) return 0;

    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(virt), 0);
    if (!pt) return 0;

    if (!(pt[PT_INDEX(virt)] & PAGE_PRESENT)) return 0;

    return (pt[PT_INDEX(virt)] & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

uint64_t vmm_get_hhdm_offset(void) {
    return hhdm_offset;
}

uint64_t vmm_clone_kernel_space(void) {
    uint64_t new_pml4_phys = pmm_alloc_page();
    if (!new_pml4_phys) return 0;
    uint64_t *new_pml4 = (uint64_t *)phys_to_virt(new_pml4_phys);
    
    /* Copy kernel entries (indices 256-511) */
    uint64_t *cur_pml4 = get_current_pml4();
    for (int i = 256; i < 512; i++) {
        new_pml4[i] = cur_pml4[i];
    }
    
    return new_pml4_phys;
}

static void vmm_clone_pt(uint64_t *old_pt, uint64_t *new_pt) {
    for (int i = 0; i < 512; i++) {
        if (old_pt[i] & PAGE_PRESENT) {
            uint64_t phys = old_pt[i] & 0x000FFFFFFFFFF000ULL;
            if ((old_pt[i] & PAGE_USER) && (old_pt[i] & PAGE_WRITABLE)) {
                /* COW: mark read-only and increment refcount */
                old_pt[i] &= ~PAGE_WRITABLE;
                new_pt[i] = old_pt[i];
                pmm_page_ref(phys);
            } else {
                new_pt[i] = old_pt[i];
                if (old_pt[i] & PAGE_USER) {
                    pmm_page_ref(phys);
                }
            }
        }
    }
}

static void vmm_clone_pd(uint64_t *old_pd, uint64_t *new_pd) {
    for (int i = 0; i < 512; i++) {
        if ((old_pd[i] & PAGE_PRESENT) && (old_pd[i] & PAGE_USER)) {
            uint64_t pt_phys = pmm_alloc_page();
            if (!pt_phys) continue;
            uint64_t *new_pt = (uint64_t *)phys_to_virt(pt_phys);
            uint64_t *old_pt = (uint64_t *)phys_to_virt(old_pd[i] & 0x000FFFFFFFFFF000ULL);
            vmm_clone_pt(old_pt, new_pt);
            new_pd[i] = pt_phys | PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
        }
    }
}

static void vmm_clone_pdpt(uint64_t *old_pdpt, uint64_t *new_pdpt) {
    for (int i = 0; i < 512; i++) {
        if ((old_pdpt[i] & PAGE_PRESENT) && (old_pdpt[i] & PAGE_USER)) {
            uint64_t pd_phys = pmm_alloc_page();
            if (!pd_phys) continue;
            uint64_t *new_pd = (uint64_t *)phys_to_virt(pd_phys);
            uint64_t *old_pd = (uint64_t *)phys_to_virt(old_pdpt[i] & 0x000FFFFFFFFFF000ULL);
            vmm_clone_pd(old_pd, new_pd);
            new_pdpt[i] = pd_phys | PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
        }
    }
}

uint64_t vmm_cow_clone(uint64_t parent_cr3) {
    uint64_t new_pml4_phys = vmm_clone_kernel_space();
    if (!new_pml4_phys) return 0;
    
    uint64_t *new_pml4 = (uint64_t *)phys_to_virt(new_pml4_phys);
    uint64_t *old_pml4 = (uint64_t *)phys_to_virt(parent_cr3);
    
    /* Walk user entries 0-255 */
    for (int i = 0; i < 256; i++) {
        if ((old_pml4[i] & PAGE_PRESENT) && (old_pml4[i] & PAGE_USER)) {
            uint64_t pdpt_phys = pmm_alloc_page();
            if (!pdpt_phys) continue; /* Should handle cleanup in production */
            uint64_t *new_pdpt = (uint64_t *)phys_to_virt(pdpt_phys);
            uint64_t *old_pdpt = (uint64_t *)phys_to_virt(old_pml4[i] & 0x000FFFFFFFFFF000ULL);
            vmm_clone_pdpt(old_pdpt, new_pdpt);
            new_pml4[i] = pdpt_phys | PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE;
        }
    }
    
    /* Flush TLB */
    uint64_t cr3_val = read_cr3();
    write_cr3(cr3_val);
    
    return new_pml4_phys;
}

static bool is_cow_page(process_t *proc, uint64_t fault_addr) {
    (void)proc;
    /* In our implementation, a cow page is a present user page that is not writable */
    uint64_t *pdpt = vmm_get_next_level(get_current_pml4(), PML4_INDEX(fault_addr), 0);
    if (!pdpt) return false;
    uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(fault_addr), 0);
    if (!pd) return false;
    uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(fault_addr), 0);
    if (!pt) return false;
    
    uint64_t entry = pt[PT_INDEX(fault_addr)];
    if ((entry & PAGE_PRESENT) && (entry & PAGE_USER) && !(entry & PAGE_WRITABLE)) {
        return true;
    }
    return false;
}

static void map_user_page(process_t *proc, uint64_t virt, uint64_t flags) {
    uint64_t saved_cr3 = read_cr3();
    if (saved_cr3 != proc->cr3) write_cr3(proc->cr3);
    
    uint64_t phys = pmm_alloc_page();
    if (phys) {
        vmm_map_page(virt, phys, flags);
    }
    
    if (saved_cr3 != proc->cr3) write_cr3(saved_cr3);
}

static void cow_handle_fault(process_t *proc, uint64_t fault_addr) {
    uint64_t phys = vmm_get_phys(fault_addr & ~0xFFF);
    if (!phys) return;
    
    uint16_t refcount = pmm_page_refcount(phys);
    
    uint64_t saved_cr3 = read_cr3();
    if (saved_cr3 != proc->cr3) write_cr3(proc->cr3);
    
    if (refcount == 1) {
        /* Sole owner, just restore writability */
        uint64_t *pdpt = vmm_get_next_level(get_current_pml4(), PML4_INDEX(fault_addr), 0);
        uint64_t *pd = vmm_get_next_level(pdpt, PDPT_INDEX(fault_addr), 0);
        uint64_t *pt = vmm_get_next_level(pd, PD_INDEX(fault_addr), 0);
        if (pt) {
            pt[PT_INDEX(fault_addr)] |= PAGE_WRITABLE;
            invlpg(fault_addr);
        }
    } else {
        /* More than one owner: allocate new and deep copy */
        uint64_t new_phys = pmm_alloc_page();
        if (new_phys) {
            memcpy(phys_to_virt(new_phys), phys_to_virt(phys), 4096);
            vmm_map_page(fault_addr & ~0xFFF, new_phys, PAGE_PRESENT | PAGE_USER | PAGE_WRITABLE);
            pmm_page_unref(phys);
        }
    }
    
    if (saved_cr3 != proc->cr3) write_cr3(saved_cr3);
}

void page_fault_handler(registers_t *regs) {
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
    uint32_t error = regs->err_code;
    bool write = (error >> 1) & 1;
    bool user  = (error >> 2) & 1;

    if (!user) {
        kprintf("KERNEL PF at 0x%016llx. EIP: 0x%016llx\n", (unsigned long long)fault_addr, (unsigned long long)regs->rip);
        kpanic("Kernel page fault");
    }

    process_t *proc = current_process;
    if (!proc) kpanic("User page fault but no current_process!");

    /* 1. COW fault */
    if (write && is_cow_page(proc, fault_addr)) {
        cow_handle_fault(proc, fault_addr);
        return;
    }

    /* 2. Stack growth */
    if (fault_addr <= proc->stack_top_virt && fault_addr >= proc->stack_top_virt - PROC_USER_STACK_MAX) {
        if (fault_addr < proc->stack_limit_virt) {
            map_user_page(proc, fault_addr & ~0xFFF, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            proc->stack_limit_virt = fault_addr & ~0xFFF;
            return;
        } else if (vmm_get_phys(fault_addr & ~0xFFF) == 0) {
            /* Fault within already established stack region but unmapped (just lazy) */
            map_user_page(proc, fault_addr & ~0xFFF, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            return;
        }
    }

    /* 3. Anonymous heap / VMA */
    vma_t *vma = process_find_vma(proc, fault_addr);
    if (vma) {
        uint64_t vma_flags = PAGE_PRESENT | PAGE_USER;
        if (vma->flags & PAGE_WRITABLE) vma_flags |= PAGE_WRITABLE;
        map_user_page(proc, fault_addr & ~0xFFFULL, vma_flags);
        return;
    }

    if (fault_addr >= proc->heap_base && fault_addr < proc->heap_end) {
        map_user_page(proc, fault_addr & ~0xFFFULL, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        return;
    }

    /* 4. Kill process (SIGSEGV) */
    kprintf("SIGSEGV PID %d: FAULT 0x%016llx EIP 0x%016llx\n", (int)proc->pid, (unsigned long long)fault_addr, (unsigned long long)regs->rip);
    process_exit(proc, 139);
    schedule(); /* Force reschedule immediately */
}

/* vmm_destroy_address_space — free all user-space physical pages and page tables
 * for a process. Safe to call on COW clones: uses pmm_page_unref to decrement
 * refcounts rather than unconditionally freeing.
 * Kernel entries (PML4 indices 256-511) are NEVER freed (shared across all procs). */
void vmm_destroy_address_space(uint64_t cr3) {
    if (!cr3) return;
    uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3);

    for (int i = 0; i < 256; i++) {  /* user half only */
        if (!(pml4[i] & PAGE_PRESENT)) continue;

        uint64_t pdpt_phys = pml4[i] & 0x000FFFFFFFFFF000ULL;
        uint64_t *pdpt = (uint64_t *)phys_to_virt(pdpt_phys);

        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PAGE_PRESENT) || !(pdpt[j] & PAGE_USER)) continue;

            uint64_t pd_phys = pdpt[j] & 0x000FFFFFFFFFF000ULL;
            uint64_t *pd = (uint64_t *)phys_to_virt(pd_phys);

            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PAGE_PRESENT) || !(pd[k] & PAGE_USER)) continue;

                uint64_t pt_phys = pd[k] & 0x000FFFFFFFFFF000ULL;
                uint64_t *pt = (uint64_t *)phys_to_virt(pt_phys);

                for (int l = 0; l < 512; l++) {
                    if (!(pt[l] & PAGE_PRESENT) || !(pt[l] & PAGE_USER)) continue;
                    uint64_t page_phys = pt[l] & 0x000FFFFFFFFFF000ULL;
                    pmm_page_unref(page_phys);  /* decrement refcount; frees if 0 */
                }
                pmm_free_page(pt_phys);  /* free the PT itself */
            }
            pmm_free_page(pd_phys);  /* free the PD itself */
        }
        pmm_free_page(pdpt_phys);  /* free the PDPT itself */
    }
    pmm_free_page(cr3);  /* free the PML4 itself */
}
