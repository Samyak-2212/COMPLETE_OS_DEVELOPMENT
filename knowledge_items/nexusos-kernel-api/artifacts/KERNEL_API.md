# NexusOS — Kernel API Reference

> **Version**: 2.0 | **Last Updated**: 2026-04-21
> **Status**: Phase 4 complete. APIs below are STABLE unless marked [PLANNED].

---

## 1. Memory Management (`kernel/src/mm/`)

### PMM — Physical Memory Manager (`pmm.h`)
```c
void     pmm_init(void);                    // Parse Limine memmap, build bitmap
uint64_t pmm_alloc_page(void);              // Returns physical address of 4 KiB page (0 on OOM)
void     pmm_free_page(uint64_t phys_addr); // Free a previously allocated page
uint64_t pmm_alloc_pages(uint64_t count);   // Allocate N contiguous pages
void     pmm_free_pages(uint64_t phys, uint64_t count);
uint64_t pmm_get_free_page_count(void);
uint64_t pmm_get_total_page_count(void);
// COW refcount API (Phase 4):
void     pmm_page_ref(uint64_t phys);       // Increment page refcount (COW clone)
void     pmm_page_unref(uint64_t phys);     // Decrement; free physical page if refcount hits 0
uint16_t pmm_page_refcount(uint64_t phys);  // Read current refcount
```

### VMM — Virtual Memory Manager (`vmm.h`)
```c
void     vmm_init(void);
void     vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void     vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);                // Walk page tables
uint64_t vmm_get_hhdm_offset(void);                  // HHDM base for phys→virt
// Phase 4 address space API:
uint64_t vmm_clone_kernel_space(void);               // New PML4 with kernel upper-half copied
uint64_t vmm_cow_clone(uint64_t parent_cr3);         // Full COW clone of user address space
void     vmm_destroy_address_space(uint64_t cr3);    // Free all user pages + page tables (COW-safe)
void     page_fault_handler(registers_t *regs);      // Handles COW, stack growth, heap demand-paging
// Flags: PAGE_PRESENT, PAGE_WRITABLE, PAGE_USER, PAGE_NOCACHE, PAGE_NX
// HHDM: phys_to_virt = phys + vmm_get_hhdm_offset()
```

### Heap (`heap.h`)
```c
void  heap_init(uint64_t initial_pages);    // Initialize kernel heap
void *kmalloc(size_t size);                 // Allocate (NOT zeroed)
void *kcalloc(size_t count, size_t size);   // Allocate + zero (STABLE Phase 4)
void  kfree(void *ptr);                     // Free allocation
void *kmalloc_aligned(size_t size, size_t alignment);
void  heap_get_stats(uint64_t *total, uint64_t *used, uint64_t *free_bytes);
```

---

## 2. Hardware Abstraction (`kernel/src/hal/`)

### I/O Ports (`io.h`)
```c
static inline uint8_t  inb(uint16_t port);
static inline void     outb(uint16_t port, uint8_t val);
static inline uint16_t inw(uint16_t port);
static inline void     outw(uint16_t port, uint16_t val);
static inline uint32_t ind(uint16_t port);
static inline void     outd(uint16_t port, uint32_t val);
static inline void     io_wait(void);       // Short delay via port 0x80
static inline void     cli(void);           // Disable interrupts
static inline void     sti(void);           // Enable interrupts
static inline uint64_t read_cr3(void);
static inline void     write_cr3(uint64_t val);
static inline void     invlpg(uint64_t addr);
```

### GDT (`gdt.h`)
```c
void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);   // Update TSS RSP0 on every context switch
// Segment Selectors (GDT index × 8 | RPL):
//   0x00 = null
//   0x08 = kernel code64    (Ring 0)
//   0x10 = kernel data64   (Ring 0)
//   0x18 = user data64     (Ring 3) — SS on SYSRET/IRETQ
//   0x20 = user code64     (Ring 3) — CS on SYSRET/IRETQ
//   0x28 = TSS (16-byte entry spanning two slots)
// With RPL=3: user_data64=0x1B, user_code64=0x23
// IA32_STAR[47:32]=0x0008 (SYSCALL CS), IA32_STAR[63:48]=0x0013 (SYSRET CSS base)
```

### IDT / ISR (`idt.h`, `isr.h`)
```c
void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t ist, uint8_t flags);
void isr_register_handler(uint8_t vector, void (*handler)(registers_t *));
// IRQ vectors: 32=timer, 33=keyboard, 44=mouse
```

### PIC (`pic.h`)
```c
void pic_init(void);
void pic_send_eoi(uint8_t irq);    // 0-15
void pic_set_mask(uint8_t irq);    // Mask (disable) IRQ
void pic_clear_mask(uint8_t irq);  // Unmask (enable) IRQ
```

---

## 3. Input System (`kernel/src/drivers/input/`)

### Input Events (`input_event.h`)
```c
typedef struct input_event {
    uint8_t  type;      // INPUT_EVENT_KEY_PRESS, INPUT_EVENT_KEY_RELEASE,
                        // INPUT_EVENT_MOUSE_MOVE, INPUT_EVENT_MOUSE_BUTTON
    uint8_t  keycode;   // Raw scancode / keycode
    char     ascii;     // ASCII character (0 if non-printable)
    int16_t  mouse_dx;  // Mouse X delta
    int16_t  mouse_dy;  // Mouse Y delta
    uint8_t  mouse_btn; // Mouse button state (bit0=left, bit1=right, bit2=middle)
} input_event_t;

// Event types
#define INPUT_EVENT_KEY_PRESS     0x01
#define INPUT_EVENT_KEY_RELEASE   0x02
#define INPUT_EVENT_MOUSE_MOVE    0x03
#define INPUT_EVENT_MOUSE_BUTTON  0x04
```

### Input Manager
```c
void input_manager_init(void);
void input_push_event(input_event_t *event); // Called by drivers (KB, mouse, USB HID)
int  input_poll_event(input_event_t *out);   // Returns 1 if event available, 0 if empty
int  input_has_event(void);
```

---

## 4. Timer (`kernel/src/drivers/timer/pit.h`)
```c
void     pit_init(void);          // 1000 Hz (1ms tick)
uint64_t pit_get_ticks(void);     // Ticks since boot
void     pit_sleep_ms(uint64_t ms); // Busy-wait sleep
```

---

## 5. Framebuffer (`kernel/src/drivers/video/framebuffer.h`)
```c
int  framebuffer_init(void);      // Parse Limine framebuffer response
void framebuffer_clear(uint32_t color);
void framebuffer_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

// UI Theme Constants
#define FB_DEFAULT_BG   0x00300347
#define FB_DEFAULT_FG   0x00AAAAAA

typedef struct framebuffer_info {
    uint64_t address;             // Virtual address of framebuffer
    uint32_t width, height;
    uint32_t pitch;               // Bytes per scanline
    uint32_t bpp;                 // Bits per pixel (usually 32)
} framebuffer_info_t;

const framebuffer_info_t *framebuffer_get_info(void);
```

---

## 6. Printing (`kernel/src/lib/printf.h`)
```c
void kprintf_init(void);
void kprintf(const char *fmt, ...);           // Format: %d, %u, %x, %llx, %s, %p, %c
void kprintf_set_color(uint32_t fg, uint32_t bg);
void kputchar(char c);
```

---

## 7. String/Memory Utils (`kernel/src/lib/string.h`)
```c
void   *memcpy(void *dest, const void *src, uint64_t n);
void   *memset(void *dest, int val, uint64_t n);
int     memcmp(const void *a, const void *b, uint64_t n);
uint64_t strlen(const char *s);
int     strcmp(const char *a, const char *b);
int     strncmp(const char *a, const char *b, uint64_t n);
char   *strcpy(char *dest, const char *src);
char   *strncpy(char *dest, const char *src, uint64_t n);
char   *strcat(char *dest, const char *src);  // [PLANNED]
char   *strchr(const char *s, int c);         // [PLANNED]
```

---

## 8. Spinlocks (`kernel/src/lib/spinlock.h`)
```c
typedef struct { volatile int locked; } spinlock_t;
#define SPINLOCK_INIT {0}
void spinlock_acquire(spinlock_t *lock);  // cli + spin
void spinlock_release(spinlock_t *lock);  // clear + sti
```

---

## 9. Driver Interface (`kernel/src/drivers/driver.h`)
```c
typedef struct driver {
    const char *name;
    uint32_t   type;            // DRIVER_TYPE_INPUT, _STORAGE, _NETWORK, _VIDEO, _USB
    int  (*init)(void);
    void (*deinit)(void);
    int  (*probe)(uint16_t vendor_id, uint16_t device_id);
    void (*irq_handler)(void);
} driver_t;

void driver_register(driver_t *drv);
void driver_probe_all(void);
```

---

## 10. VFS Interface (`kernel/src/fs/vfs.h`)
```c
typedef struct vfs_node vfs_node_t;

typedef struct vfs_ops {
    int     (*open)(vfs_node_t *node, uint32_t flags);
    int     (*close)(vfs_node_t *node);
    int64_t (*read)(vfs_node_t *node, void *buf, uint64_t size, uint64_t offset);
    int64_t (*write)(vfs_node_t *node, const void *buf, uint64_t size, uint64_t offset);
    int     (*readdir)(vfs_node_t *dir, uint32_t index, vfs_node_t *out);
    int     (*mkdir)(vfs_node_t *parent, const char *name);
    int     (*unlink)(vfs_node_t *parent, const char *name);
    int     (*stat)(vfs_node_t *node, vfs_stat_t *out);
} vfs_ops_t;

// VFS API
vfs_node_t *vfs_open(const char *path, uint32_t flags);
int64_t     vfs_read(vfs_node_t *node, void *buf, uint64_t size, uint64_t offset);
int64_t     vfs_write(vfs_node_t *node, const void *buf, uint64_t size, uint64_t offset);
void        vfs_close(vfs_node_t *node);
int         vfs_readdir(vfs_node_t *dir, uint32_t index, vfs_node_t *out);
int         vfs_mkdir(const char *path);
int         vfs_unlink(const char *path);
int         vfs_mount(const char *path, const char *fstype, void *device);
void        vfs_register_fs(const char *name, vfs_ops_t *ops);
```

---

## 11. Syscall Interface (Phase 4 — STABLE)

Linux x86_64 ABI. Invoke via `syscall` instruction: RAX=num, args in RDI/RSI/RDX/R10/R8/R9.
Return value in RAX. Negative = `-errno`.

```c
// Linux-compatible syscall numbers (subset implemented in Phase 4):
#define SYS_READ          0    // fd, buf, count → bytes_read
#define SYS_WRITE         1    // fd, buf, count → bytes_written
#define SYS_OPEN          2    // path, flags, mode → fd
#define SYS_CLOSE         3    // fd → 0
#define SYS_STAT          4    // path, statbuf → -ENOSYS (stub)
#define SYS_MMAP          9    // addr, len, prot, flags → mapped_virt
#define SYS_MPROTECT      10   // stub → 0
#define SYS_MUNMAP        11   // stub → 0
#define SYS_BRK           12   // new_brk (0=query) → current_brk
#define SYS_RT_SIGACTION  13   // stub → 0
#define SYS_RT_SIGPROCMASK 14  // stub → 0
#define SYS_IOCTL         16   // stub → -ENOSYS
#define SYS_FORK          57   // → child_pid (parent), 0 (child, deferred)
#define SYS_EXECVE        59   // path, argv, envp → -ENOSYS (context replacement Phase 5)
#define SYS_EXIT          60   // code → no return
#define SYS_WAIT4         61   // pid, *status, options → reaped_pid
#define SYS_GETPID        39   // → pid
#define SYS_GETPPID       110  // → ppid
#define SYS_GETUID        102  // → uid
#define SYS_GETTID        186  // → tid
#define SYS_ARCH_PRCTL    158  // code=ARCH_SET_FS(0x1002) → 0
#define SYS_SET_TID_ADDR  218  // → tid
#define SYS_EXIT_GROUP    231  // code → no return
#define SYS_OPENAT        257  // stub → -ENOSYS
#define SYS_GETDENTS64    217  // stub → -ENOSYS
```

---

## 15. Process/Thread/Scheduler (Phase 4 — STABLE)

```c
/* process.h */
typedef int32_t pid_t;
process_t *process_create(pid_t ppid);          // Allocate PCB + PML4
process_t *process_get(pid_t pid);              // O(n) PID lookup
void       process_destroy(process_t *proc);    // Free VMAs, addr space, FD table, TCBs
int        process_exit(process_t *proc, int code); // ZOMBIE + schedule()
pid_t      process_waitpid(pid_t parent, pid_t target, int *status);
void       process_add_vma(process_t *p, uint64_t start, uint64_t end, uint32_t flags);
vma_t     *process_find_vma(process_t *p, uint64_t addr);
extern process_t *current_process;  /* Updated by scheduler on every switch */
extern thread_t  *current_thread;

/* thread.h */
thread_t *thread_create(process_t *proc, uint64_t entry_rip, uint64_t arg1, uint64_t arg2);
void      thread_destroy(thread_t *t);
void      thread_sleep_ms(uint64_t ms);         // Block + schedule for at least ms ms

/* scheduler.h */
int  scheduler_init(void);                      // Creates idle+shell threads, hooks IRQ0
void scheduler_start(void);                     // Bootstrap: load first thread. Never returns.
void scheduler_enqueue(thread_t *t);            // Add thread to priority queue
void scheduler_dequeue(thread_t *t);            // Remove thread from priority queue
void schedule(void);                            // Pick next thread, switch context
void scheduler_block(thread_state_t reason);    // Block current thread + schedule()
void scheduler_wake(thread_t *t);               // Wake BLOCKED/SLEEPING thread
void scheduler_exit_current(int code);          // Exit current process
void scheduler_tick(void);                      // Called from IRQ0 every ms
thread_t  *scheduler_current_thread(void);
process_t *scheduler_current_process(void);
// Priority levels (nexus_config.h):
//   SCHED_PRIORITY_IDLE=0, LOW=2, NORMAL=4, HIGH=6, RT=7
```

---

## 16. USB Subsystem (Phase 4 — STABLE)

```c
/* xhci.h */
void xhci_init_subsystem(void);              // Register xHCI driver with PnP manager
int  xhci_command_ring_push(xhci_t *hc, xhci_trb_t *trb);
int  xhci_wait_command(xhci_t *hc, uint64_t cmd_trb_phys, uint8_t *slot_out, uint8_t *cc_out);
void xhci_process_events(xhci_t *hc);        // Process event ring (called from IRQ handler)

/* usb_device.h */
int usb_enumerate_device(xhci_t *hc, uint8_t port, uint8_t speed);
int usb_control_transfer(xhci_t *hc, usb_device_t *dev, usb_setup_t *setup,
                         uint64_t data_phys, uint16_t data_len, int dir_in);
extern usb_device_t *usb_devices[USB_MAX_DEVICES];  // Lazy: NULL until enumerated

/* usb_hid.h */
int usb_hid_probe(xhci_t *hc, usb_device_t *dev);  // Attach HID driver to device
```


## 13. Userspace Display API (Phase 6 provisions)
```c
// When developing a Desktop Environment in Userspace, use the following:
// 1. Open framebuffer and map to userspace memory:
int fb_fd = open("/dev/fb0", O_RDWR);
void *framebuffer = mmap(NULL, width * height * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);

// 2. Open input events:
int input_fd = open("/dev/input/event0", O_RDONLY);
// read() input_event_t structs from input_fd

// 3. Take control of display (pause kernel VT100 console):
ioctl(fb_fd, FB_IOCTL_ACQUIRE_VT, 0);

// 4. Release control back to kernel terminal:
ioctl(fb_fd, FB_IOCTL_RELEASE_VT, 0);
```

---

## 12. Boot Info (`kernel/src/boot/limine_requests.h`)
```c
struct limine_memmap_response    *limine_get_memmap(void);
struct limine_framebuffer       *limine_get_framebuffer(void);
struct limine_hhdm_response     *limine_get_hhdm(void);
struct limine_rsdp_response     *limine_get_rsdp(void);
struct limine_firmware_type_response *limine_get_firmware_type(void);
int limine_base_revision_supported(void);
```

---

## 14. Debugger Subsystem (`debugger/include/debugger.h`)
```c
void debugger_init(void);                    // Initialize COM1 + Protocol
void debugger_break(void);                   // Trigger software breakpoint (INT 3)
void debugger_watch(void *addr, size_t len, int type); // x64 DR0 Hardware Watchpoint
bool debugger_is_present(void);              // Check if DEBUGGER=1 is active

/* Symbolic Resolution */
const char* debugger_resolve_symbol(uint64_t addr, uint64_t *offset);

/* Safe Memory Access */
bool debugger_mem_read(void *dest, const void *src, size_t len);
bool debugger_mem_write(void *dest, const void *src, size_t len);
```

