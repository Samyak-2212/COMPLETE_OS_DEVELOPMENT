# NexusOS — Kernel API Reference

> **Version**: 1.0 | **Last Updated**: 2026-03-29
> **Status**: Phase 2 complete. APIs below are STABLE unless marked [PLANNED].

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
```

### VMM — Virtual Memory Manager (`vmm.h`)
```c
void     vmm_init(void);
void     vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
void     vmm_unmap_page(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);                // Walk page tables
// Flags: PAGE_PRESENT, PAGE_WRITABLE, PAGE_USER, PAGE_NX
// HHDM: use extern uint64_t hhdm_offset; phys_to_virt = phys + hhdm_offset
```

### Heap (`heap.h`)
```c
void  heap_init(uint64_t initial_pages);    // Initialize kernel heap
void *kmalloc(uint64_t size);               // Allocate (NOT zeroed)
void *kcalloc(uint64_t count, uint64_t size); // Allocate + zero [PLANNED]
void  kfree(void *ptr);                     // Free allocation
void *kmalloc_aligned(uint64_t size, uint64_t alignment);
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
// Segments: 0x00=null, 0x08=kernel_code64, 0x10=kernel_data64,
//           0x18=user_code64, 0x20=user_data64, 0x28=TSS
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
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color);
void framebuffer_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

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

## 11. Syscall Interface [PLANNED — Phase 4]
```c
// Syscall numbers (in RDI via syscall instruction)
#define SYS_EXIT   0
#define SYS_WRITE  1   // fd, buf, count → bytes_written
#define SYS_READ   2   // fd, buf, count → bytes_read
#define SYS_OPEN   3   // path, flags → fd
#define SYS_CLOSE  4   // fd → 0
#define SYS_FORK   5   // → pid (child=0, parent=child_pid)
#define SYS_EXEC   6   // path, argv → doesn't return on success
#define SYS_MMAP   7   // addr, len, prot, flags → mapped_addr
#define SYS_GETPID 8   // → pid
#define SYS_WAIT   9   // pid → exit_status
#define SYS_KILL   10  // pid, signal → 0
#define SYS_IOCTL  11  // fd, cmd, arg → result (used for /dev/fb0 VT switching)

---

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

