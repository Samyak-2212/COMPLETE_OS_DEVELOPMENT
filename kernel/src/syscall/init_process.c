/* ============================================================================
 * init_process.c — NexusOS Init Process Launch
 * Purpose: Create PID 1 from Limine-provided ELF module, transition to Ring 3
 * ============================================================================ */

#include "syscall.h"
#include "sched/process.h"
#include "sched/thread.h"
#include "sched/scheduler.h"
#include "fs/vfs.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "elf_loader.h"
#include "lib/printf.h"
#include "lib/string.h"
#include "boot/limine_requests.h"
#include "hal/io.h"

/* We need an external function to construct the user return frame.
   For simplicity, we populate the new thread's kernel stack simulating an iretq frame.
   Normally, Agent A's thread_create might expose something, but we'll do it manually on top of the thread's stack. */

/* The /init binary will be provided by an actual filesystem later. */
void usermode_entry_stub(uint64_t entry_point, uint64_t user_sp) {
    /* Transition to Ring 3 using IRETQ */
    /* GDT configuration: User Data is 0x18 (0x1B with RPL3), User Code is 0x20 (0x23 with RPL3) */
    __asm__ volatile (
        "mov $0x1B, %%ax \n"
        "mov %%ax, %%ds \n"
        "mov %%ax, %%es \n"
        "mov %%ax, %%fs \n"
        "mov %%ax, %%gs \n"
        "push $0x1B \n"       /* SS */
        "push %0 \n"          /* RSP */
        "push $0x200 \n"      /* RFLAGS (IF=1) */
        "push $0x23 \n"       /* CS */
        "push %1 \n"          /* RIP */
        "iretq \n"
        : : "r"(user_sp), "r"(entry_point) : "memory", "ax"
    );
}

static uint64_t setup_init_stack(process_t *proc, uint64_t stack_top) {
    uint64_t saved_cr3 = read_cr3();
    write_cr3(proc->cr3);

    /* Map the initial stack page */
    uint64_t top_page = stack_top & ~0xFFFULL;
    uint64_t phys = pmm_alloc_page();
    if (!phys) {
        write_cr3(saved_cr3);
        return stack_top;
    }
    vmm_map_page(top_page, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    uint64_t hhdm = vmm_get_hhdm_offset();
    /* We work from the top of the physical page down */
    uint8_t *kernel_stack_ptr = (uint8_t *)(phys + hhdm + 4096);

    /* 1. Push argv[0] string data */
    const char *argv0_str = "/init";
    size_t argv0_len = strlen(argv0_str) + 1;
    kernel_stack_ptr -= argv0_len;
    strcpy((char*)kernel_stack_ptr, argv0_str);
    
    /* Calculate the user address of this string */
    uint64_t argv0_user_addr = top_page + (4096 - ((uint8_t*)(phys + hhdm + 4096) - kernel_stack_ptr));

    /* 2. Align stack to 16 bytes for SysV ABI */
    uint64_t sp_val = (uint64_t)kernel_stack_ptr;
    sp_val &= ~0xF;
    kernel_stack_ptr = (uint8_t*)sp_val;

    /* 3. Push EnvP (NULL) */
    kernel_stack_ptr -= 8;
    *(uint64_t*)kernel_stack_ptr = 0;

    /* 4. Push ArgV array (ArgV[1]=NULL, ArgV[0]=ptr) */
    kernel_stack_ptr -= 8;
    *(uint64_t*)kernel_stack_ptr = 0;
    kernel_stack_ptr -= 8;
    *(uint64_t*)kernel_stack_ptr = argv0_user_addr;

    /* 5. Push ArgC = 1 */
    kernel_stack_ptr -= 8;
    *(uint64_t*)kernel_stack_ptr = 1;

    /* Calculate final user RSP */
    uint64_t final_user_sp = top_page + (4096 - ((uint8_t*)(phys + hhdm + 4096) - kernel_stack_ptr));

    write_cr3(saved_cr3);
    return final_user_sp;
}


void init_process_start(void) {
    /* Create PCB */
    process_t *init = process_create(0);
    if (!init) return;

    init->cr3 = vmm_clone_kernel_space();
    init->fd_table = kcalloc(1, sizeof(fd_table_t));
    vfs_node_t *tty0 = vfs_resolve_path("/dev/tty0");
    if (tty0) {
        vfs_open(tty0);
        init->fd_table->fds[0] = tty0;
        vfs_open(tty0);
        init->fd_table->fds[1] = tty0;
        vfs_open(tty0);
        init->fd_table->fds[2] = tty0;
        init->fd_table->open_count = 3;
    } else {
        kprintf("[SYSCALL] Warn: /dev/tty0 not found for init, ignoring FDs\n");
    }

    /* Look for the /init Limine module and install it to VFS */
    struct limine_module_response *mod_resp = limine_get_modules();
    if (mod_resp && mod_resp->module_count > 0) {
        for (uint64_t i = 0; i < mod_resp->module_count; i++) {
            struct limine_file *f = mod_resp->modules[i];
            /* We expect the module to be named something ending in "init" or specifically "/boot/init" */
            if (f->path && strstr(f->path, "init") != NULL) {
                vfs_node_t *init_node = vfs_create("/init");
                if (init_node) {
                    vfs_open(init_node);
                    vfs_write(init_node, 0, f->size, (uint8_t*)f->address);
                    vfs_close(init_node);
                    kprintf("[SYSCALL] Embedded init binary size: %llu bytes.\n", (unsigned long long)f->size);
                }
                break;
            }
        }
    }

    vfs_node_t *init_bin = vfs_resolve_path("/init");
    if (!init_bin) {
        kprintf("[SYSCALL] Warn: /init not found! User space won't start.\n");
        return;
    }

    uint64_t entry = 0;
    if (elf_load(init, init_bin, &entry) != 0) {
        kprintf("[SYSCALL] Error loading /init ELF!\n");
        return;
    }
    vfs_close(init_bin);

    /* Setup user stack with argc/argv */
    uint64_t stack_top = 0x00007FFFFFFFF000ULL;
    uint64_t user_sp = setup_init_stack(init, stack_top);

    init->stack_top_virt = stack_top;
    init->stack_limit_virt = stack_top;

    /* Create thread targeting our usermode stub */
    thread_t *t = thread_create(init, (uint64_t)usermode_entry_stub, entry, user_sp);
    if (t) {
        t->priority = SCHED_PRIORITY_HIGH;
        scheduler_enqueue(t);
    }
}

