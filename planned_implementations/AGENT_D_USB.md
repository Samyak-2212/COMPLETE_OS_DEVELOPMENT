# NexusOS Phase 4 — Agent D: USB Stack (xHCI + HID)

> **Role**: Driver Agent  
> **Tasks**: 4.7 (xHCI host controller), 4.8 (USB enumeration), 4.9 (USB HID driver)  
> **Files Owned**: `kernel/src/drivers/usb/`  
> **Depends On**: Nothing from A/B/C (parallel track — only needs PCI done, which is Phase 3 COMPLETE)  
> **Estimated Complexity**: HIGH  
> **Efficiency Mandate**: Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` FIRST

---

## 0. Mandatory Pre-Reading

In order:
1. `AGENTS.md`
2. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md`
3. `knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md`
4. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md`
5. `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md` — esp. BUG-003 (DMA bounce buffer pattern)
6. `kernel/src/drivers/driver.h` — driver_t interface you MUST implement
7. `kernel/src/drivers/driver_manager.c` — how drivers register
8. `kernel/src/drivers/pci/pci.h` + `pci.c` — how to read PCI config space + BARs
9. `kernel/src/drivers/pci/pci_ids.h` — PCI class/subclass/progif constants
10. `kernel/src/mm/vmm.h` — vmm_map_page
11. `kernel/src/mm/pmm.h` — pmm_alloc_pages (for DMA memory)
12. `kernel/src/mm/heap.h` — kmalloc/kfree
13. `kernel/src/drivers/input/input_manager.c` — how to push input events
14. `kernel/src/drivers/input/input_event.h` — input_event_t struct
15. `kernel/src/config/nexus_config.h` — config constants (Agent A creates this)

**You do NOT touch**: kernel.c, hal/, mm/ source, pci.c, input_manager.c, sched/, syscall/

---

## 1. Overview

You build a layered USB stack:
```
Layer 3: USB HID Class Driver    → translates HID reports → input_event_t
Layer 2: USB Device Enumeration  → descriptor parsing, device table
Layer 1: xHCI Host Controller    → TRBs, rings, DMA, interrupt handling
Layer 0: PCI                     → already done (Phase 3)
```

Each layer is its own file pair. They communicate via well-defined interfaces.
The stack plugs into the existing `driver_t` / `driver_manager` infrastructure.

---

## 2. File Layout to Create

```
kernel/src/drivers/usb/
├── xhci.h             ← xHCI register map, TRB structs, public API
├── xhci.c             ← Host controller init, command ring, event processing
├── xhci_regs.h        ← All xHCI MMIO register offsets (constants only)
├── xhci_trb.h         ← TRB type codes and struct definitions
├── usb_device.h       ← USB device struct, descriptor structs, enumeration API
├── usb_device.c       ← Enumeration: GET_DESCRIPTOR, SET_ADDRESS, SET_CONFIG
├── usb_hid.h          ← HID class driver API
├── usb_hid.c          ← HID report parsing, keyboard/mouse → input_event_t
├── usb_hub.h          ← Hub support (basic)
└── usb_hub.c          ← Hub port management

docs/phase4/
└── usb.md             ← your documentation (write LAST)
```

---

## 3. xHCI Register Map (`xhci_regs.h`)

```c
/* xhci_regs.h — xHCI MMIO Register Offsets
 * Reference: xHCI Specification 1.2, Section 5
 * All offsets relative to MMIO base address. */

#ifndef XHCI_REGS_H
#define XHCI_REGS_H

/* ── Capability Registers (at MMIO base) ─────────────────────────────────── */
#define XHCI_CAP_CAPLENGTH      0x00   /* 1 byte: capability register length */
#define XHCI_CAP_HCIVERSION     0x02   /* 2 bytes: interface version number */
#define XHCI_CAP_HCSPARAMS1     0x04   /* 4 bytes: structural parameters 1 */
#define XHCI_CAP_HCSPARAMS2     0x08   /* 4 bytes: structural parameters 2 */
#define XHCI_CAP_HCSPARAMS3     0x0C   /* 4 bytes: structural parameters 3 */
#define XHCI_CAP_HCCPARAMS1     0x10   /* 4 bytes: capability parameters 1 */
#define XHCI_CAP_DBOFF          0x14   /* 4 bytes: doorbell offset */
#define XHCI_CAP_RTSOFF         0x18   /* 4 bytes: runtime register space offset */
#define XHCI_CAP_HCCPARAMS2     0x1C   /* 4 bytes: capability parameters 2 */

/* From HCSPARAMS1: */
#define XHCI_HCSP1_MAXSLOTS(x)  ((x) & 0xFF)         /* max device slots */
#define XHCI_HCSP1_MAXINTRS(x)  (((x) >> 8) & 0x7FF) /* max interrupters */
#define XHCI_HCSP1_MAXPORTS(x)  (((x) >> 24) & 0xFF) /* max ports */

/* ── Operational Registers (at MMIO base + CAPLENGTH) ───────────────────── */
#define XHCI_OP_USBCMD          0x00   /* USB Command */
#define XHCI_OP_USBSTS          0x04   /* USB Status */
#define XHCI_OP_PAGESIZE        0x08   /* Page Size */
#define XHCI_OP_DNCTRL          0x14   /* Device Notification Control */
#define XHCI_OP_CRCR_LO        0x18   /* Command Ring Control (low 32) */
#define XHCI_OP_CRCR_HI        0x1C   /* Command Ring Control (high 32) */
#define XHCI_OP_DCBAAP_LO      0x30   /* Device Context Base Address Array Pointer (low) */
#define XHCI_OP_DCBAAP_HI      0x34   /* Device Context Base Address Array Pointer (high) */
#define XHCI_OP_CONFIG          0x38   /* Configure */
/* Port Registers: base + 0x400 + 0x10 * port_index */
#define XHCI_PORT_BASE          0x400
#define XHCI_PORT_STRIDE        0x10
#define XHCI_PORTSC(n)          (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x00)
#define XHCI_PORTPMSC(n)        (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x04)
#define XHCI_PORTLI(n)          (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x08)
#define XHCI_PORTHLPMC(n)       (XHCI_PORT_BASE + (n) * XHCI_PORT_STRIDE + 0x0C)

/* USBCMD bits */
#define XHCI_USBCMD_RUN         (1 << 0)   /* Run/Stop */
#define XHCI_USBCMD_HCRST       (1 << 1)   /* Host Controller Reset */
#define XHCI_USBCMD_INTE        (1 << 2)   /* Interrupter Enable */
#define XHCI_USBCMD_HSEE        (1 << 3)   /* Host System Error Enable */
#define XHCI_USBCMD_EWE        (1 << 10)   /* Enable Wrap Event */

/* USBSTS bits */
#define XHCI_USBSTS_HCH         (1 << 0)   /* Host Controller Halted */
#define XHCI_USBSTS_HSE         (1 << 2)   /* Host System Error */
#define XHCI_USBSTS_EINT        (1 << 3)   /* Event Interrupt */
#define XHCI_USBSTS_PCD         (1 << 4)   /* Port Change Detect */
#define XHCI_USBSTS_CNR         (1 << 11)  /* Controller Not Ready */

/* PORTSC bits */
#define XHCI_PORTSC_CCS         (1 << 0)   /* Current Connect Status */
#define XHCI_PORTSC_PED         (1 << 1)   /* Port Enabled/Disabled */
#define XHCI_PORTSC_OCA         (1 << 3)   /* Over-current Active */
#define XHCI_PORTSC_PR          (1 << 4)   /* Port Reset */
#define XHCI_PORTSC_PLS_MASK    (0xF << 5) /* Port Link State */
#define XHCI_PORTSC_PP          (1 << 9)   /* Port Power */
#define XHCI_PORTSC_SPEED_MASK  (0xF << 10)/* Port Speed */
#define XHCI_PORTSC_CSC         (1 << 17)  /* Connect Status Change */
#define XHCI_PORTSC_PEC         (1 << 18)  /* Port Enable/Disable Change */
#define XHCI_PORTSC_PRC         (1 << 21)  /* Port Reset Change */
#define XHCI_PORTSC_WPR         (1 << 31)  /* Warm Port Reset */

/* ── Runtime Registers (at MMIO base + RTSOFF) ──────────────────────────── */
#define XHCI_RT_MFINDEX         0x000  /* Microframe Index */
/* Interrupter 0 Register Set at RTSOFF + 0x020 */
#define XHCI_IMAN(n)            (0x020 + (n) * 0x20 + 0x00) /* Interrupt Management */
#define XHCI_IMOD(n)            (0x020 + (n) * 0x20 + 0x04) /* Interrupt Moderation */
#define XHCI_ERSTSZ(n)          (0x020 + (n) * 0x20 + 0x08) /* Event Ring Segment Table Size */
#define XHCI_ERSTBA_LO(n)       (0x020 + (n) * 0x20 + 0x10) /* Event Ring Segment Table Base Address (low) */
#define XHCI_ERSTBA_HI(n)       (0x020 + (n) * 0x20 + 0x14) /* high */
#define XHCI_ERDP_LO(n)         (0x020 + (n) * 0x20 + 0x18) /* Dequeue Pointer (low) */
#define XHCI_ERDP_HI(n)         (0x020 + (n) * 0x20 + 0x1C) /* high */

/* ── Doorbell Registers (at MMIO base + DBOFF) ──────────────────────────── */
/* DB[0] = host command. DB[n] = slot n endpoint. */
#define XHCI_DB(n)              ((n) * 4)

/* IMAN bits */
#define XHCI_IMAN_IP            (1 << 0)  /* Interrupt Pending */
#define XHCI_IMAN_IE            (1 << 1)  /* Interrupt Enable */

/* CRCR bits */
#define XHCI_CRCR_RCS           (1 << 0)  /* Ring Cycle State */
#define XHCI_CRCR_CS            (1 << 1)  /* Command Stop */
#define XHCI_CRCR_CA            (1 << 2)  /* Command Abort */
#define XHCI_CRCR_CRR           (1 << 3)  /* Command Ring Running */

#endif /* XHCI_REGS_H */
```

---

## 4. TRB Structures (`xhci_trb.h`)

```c
/* xhci_trb.h — Transfer Request Block definitions
 * All TRBs are 16 bytes. Cycle bit in bit position 0 of parameter[3]. */

#ifndef XHCI_TRB_H
#define XHCI_TRB_H

#include <stdint.h>

/* Generic TRB layout */
typedef struct {
    uint64_t parameter;   /* TRB-specific data */
    uint32_t status;      /* TRB-specific status */
    uint32_t control;     /* Type[15:10] | Flags[9:0] | Cycle bit[0] */
} __attribute__((packed)) xhci_trb_t;

/* TRB Type codes (bits 15:10 of control field) */
#define TRB_TYPE_NORMAL          1
#define TRB_TYPE_SETUP_STAGE     2
#define TRB_TYPE_DATA_STAGE      3
#define TRB_TYPE_STATUS_STAGE    4
#define TRB_TYPE_ISOCH           5
#define TRB_TYPE_LINK            6
#define TRB_TYPE_EVENT_DATA      7
#define TRB_TYPE_NOOP            8
#define TRB_TYPE_ENABLE_SLOT     9
#define TRB_TYPE_DISABLE_SLOT    10
#define TRB_TYPE_ADDRESS_DEVICE  11
#define TRB_TYPE_CONFIG_ENDPOINT 12
#define TRB_TYPE_EVALUATE_CONTEXT 13
#define TRB_TYPE_RESET_ENDPOINT  14
#define TRB_TYPE_STOP_ENDPOINT   15
#define TRB_TYPE_SET_TR_DEQUEUE  16
#define TRB_TYPE_RESET_DEVICE    17
#define TRB_TYPE_TRANSFER_EVENT  32
#define TRB_TYPE_COMMAND_COMPLETION 33
#define TRB_TYPE_PORT_STATUS_CHANGE 34
#define TRB_TYPE_BANDWIDTH_REQUEST  35

/* TRB Control flags */
#define TRB_CYCLE               (1 << 0)   /* Cycle bit */
#define TRB_TOGGLE_CYCLE        (1 << 1)   /* Toggle Cycle (Link TRB) */
#define TRB_INT_ON_COMPLETION   (1 << 5)   /* IOC */
#define TRB_INT_ON_SHORT_PACKET (1 << 2)   /* ISP */
#define TRB_IMMEDIATE_DATA      (1 << 6)   /* IDT */
#define TRB_DIR_IN              (1 << 16)   /* Direction IN (for data stage) */

/* TRB type encoded into control field */
#define TRB_TYPE_FIELD(t)       ((uint32_t)(t) << 10)

/* Completion Codes (status field bits 31:24) */
#define TRB_CC_INVALID          0
#define TRB_CC_SUCCESS          1
#define TRB_CC_DATA_BUFFER      2
#define TRB_CC_BABBLE           3
#define TRB_CC_USB_TRANSACTION  4
#define TRB_CC_TRB_ERROR        5
#define TRB_CC_STALL            6
#define TRB_CC_RESOURCE         7
#define TRB_CC_BANDWIDTH        8
#define TRB_CC_NO_SLOTS         9
#define TRB_CC_SHORT_PACKET     13

#define TRB_GET_CC(status)      (((status) >> 24) & 0xFF)
#define TRB_GET_TYPE(ctrl)      (((ctrl) >> 10) & 0x3F)

#endif /* XHCI_TRB_H */
```

---

## 5. xHCI Driver High-Level Implementation Guide

### 5.1 DMA Memory Allocation

> **CRITICAL**: Learn from BUG-003 in bug_pool. All DMA buffers must use physical addresses
> the HBA can access (below 4 GB). Use this pattern throughout:

```c
/* Allocate physically-contiguous DMA-safe memory.
 * Returns virtual address (via HHDM). Stores physical in *phys_out. */
static void *dma_alloc(uint64_t size, uint64_t *phys_out) {
    uint64_t pages = (size + 0xFFF) / 4096;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return NULL;
    /* Verify below 4GB for legacy compatibility */
    if (phys + size > 0xFFFFFFFFULL) {
        pmm_free_pages(phys, pages);
        return NULL;  /* Try pmm_alloc_page in a loop seeking low memory */
    }
    *phys_out = phys;
    void *virt = (void *)(phys + hhdm_offset);
    memset(virt, 0, pages * 4096);
    return virt;
}
```

### 5.2 xHCI Initialization Sequence

```
1. Find xHCI PCI device (class=0x0C, subclass=0x03, prog_if=0x30)
2. Read BAR0 → MMIO base address
3. Map MMIO into kernel virtual space (vmm_map_page with NoCache flag)
4. Calculate operational register base: op_base = mmio_base + CAPLENGTH
5. Calculate runtime register base: rt_base = mmio_base + RTSOFF
6. Calculate doorbell array base: db_base = mmio_base + DBOFF
7. Stop controller: USBCMD &= ~RUN; wait for USBSTS.HCH=1 (timeout 16ms)
8. Reset controller: USBCMD |= HCRST; wait for HCRST=0 (timeout 1s)
9. Read HCSPARAMS1 → max_slots, max_ports
10. Set CONFIG register: max_slots_enabled
11. Allocate DCBAA (device context base address array): 256 entries × 8 bytes
    → Write physical addr to DCBAAP
12. Allocate Command Ring (256 TRBs, one page): set cycle state=1
    → Write physical addr to CRCR with RCS=1
13. Allocate Event Ring (256 TRBs) + ERST (1 entry)
    → Write ERST phys to ERSTBA[0]
    → Write ERSTSZ[0] = 1
    → Write ERDP[0] = event ring phys | 0x8 (dequeue_pointer bit)
14. Enable Interrupter 0: IMAN[0] |= IE | IP; IMOD[0] = 0
15. Register IRQ handler (xHCI uses MSI or INTx)
    → Read XHCI BAR0's PCI interrupt line
    → isr_register_handler(32 + irq, xhci_irq_handler)
    → pic_clear_mask(irq)  ← unmask IRQ
16. Start controller: USBCMD |= RUN | INTE
17. Check USBSTS.HCH=0 (running)
18. For each port: check PORTSC.CCS → if device present, enumerate
```

### 5.3 Command Ring Operation

```c
/* Enqueue a TRB into the command ring and ring doorbell 0 */
static void xhci_command_ring_push(xhci_trb_t *trb) {
    /* Set cycle bit */
    trb->control |= (cmd_ring_cycle ? TRB_CYCLE : 0);
    /* Write TRB */
    cmd_ring[cmd_enqueue] = *trb;
    cmd_enqueue++;
    if (cmd_enqueue >= CMD_RING_SIZE - 1) {
        /* Write Link TRB to wrap around */
        xhci_trb_t link = {0};
        link.parameter = cmd_ring_phys;
        link.control = TRB_TYPE_FIELD(TRB_TYPE_LINK) | TRB_TOGGLE_CYCLE;
        if (cmd_ring_cycle) link.control |= TRB_CYCLE;
        cmd_ring[CMD_RING_SIZE - 1] = link;
        cmd_enqueue = 0;
        cmd_ring_cycle ^= 1;
    }
    /* Ring doorbell 0 (host command) */
    volatile uint32_t *db = (volatile uint32_t *)(db_base + XHCI_DB(0));
    *db = 0;
}

/* Block-wait for command completion event — polling, no sleep needed */
static int xhci_wait_command(uint32_t slot_id, uint8_t *cc_out) {
    uint64_t deadline = pit_get_ticks() + 5000;  /* 5 second timeout */
    while (pit_get_ticks() < deadline) {
        /* Process event ring */
        xhci_trb_t *evt = &event_ring[evt_dequeue];
        if ((evt->control & TRB_CYCLE) != evt_ring_cycle) continue; /* no event yet */

        if (TRB_GET_TYPE(evt->control) == TRB_TYPE_COMMAND_COMPLETION) {
            *cc_out = TRB_GET_CC(evt->status);
            /* Advance dequeue */
            evt_dequeue++;
            if (evt_dequeue >= EVENT_RING_SIZE) { evt_dequeue = 0; evt_ring_cycle ^= 1; }
            /* Update ERDP register */
            volatile uint64_t *erdp = (volatile uint64_t *)(rt_base + XHCI_ERDP_LO(0));
            *erdp = (event_ring_phys + evt_dequeue * sizeof(xhci_trb_t)) | 0x8;
            return 0;
        }
        /* Advance past non-matching events */
        evt_dequeue++;
        if (evt_dequeue >= EVENT_RING_SIZE) { evt_dequeue = 0; evt_ring_cycle ^= 1; }
    }
    return -1;  /* timeout */
}
```

### 5.4 USB Enumeration Sequence

For each port where CCS=1:
```
1. Assert port reset: PORTSC |= PR
2. Wait for PRC (Port Reset Change): poll PORTSC until PRC=1 (100ms timeout)
3. Clear change bits: write 1 to CSC, PRC, etc. to clear them
4. Issue ENABLE_SLOT command → get slot_id from completion event
5. Allocate Input Context (32 bytes control + 2×32 bytes endpoint contexts)
6. Fill Slot Context: route string=0, speed from PORTSC, root hub port number
7. Fill Endpoint 0 Context: ep_type=4 (control), max_packet_size per speed
   (LS/FS: 8, HS: 64, SS: 512)
8. Set Input Control Context: add flags for slot + ep0
9. Issue ADDRESS_DEVICE command with BSR=0 (Block Set Address = false)
10. Issue GET_DESCRIPTOR (control transfer) for Device Descriptor (18 bytes)
11. Parse device descriptor: vendor_id, product_id, device_class, num_configs
12. Issue SET_CONFIGURATION(1)
13. Parse Configuration Descriptor → find interfaces → find endpoints
14. For HID class (bInterfaceClass=0x03): call usb_hid_probe()
```

### 5.5 Control Transfers (GET_DESCRIPTOR, SET_ADDRESS)

```
Control transfers use 3 TRBs: Setup Stage + Data Stage + Status Stage

Setup Stage TRB:
  parameter = 8-byte SETUP packet (bmRequestType, bRequest, wValue, wIndex, wLength)
  status = (8 << 16) | (0 << 22)   /* TRB Transfer Length = 8, Interrupter Target = 0 */
  control = TRB_TYPE_SETUP_STAGE | IDT | (transfer_type << 16)

Data Stage TRB (if wLength > 0):
  parameter = phys address of data buffer
  status = (wLength << 16)
  control = TRB_TYPE_DATA_STAGE | IOC | TRB_DIR_IN (for reads)

Status Stage TRB:
  parameter = 0
  status = 0
  control = TRB_TYPE_STATUS_STAGE | IOC | DIR (opposite of data)
```

---

## 6. USB HID Driver (`usb_hid.c`)

### 6.1 HID Keyboard (Boot Protocol)

For boot protocol keyboards (HID class, subclass=1, protocol=1):
- Interrupt IN endpoint, polling rate typically 10ms
- 8-byte report: [modifier, reserved, keycode0..keycode5]
- Modifier byte: bit0=LCtrl, bit1=LShift, bit2=LAlt, bit3=LGUI, bit4=RCtrl, bit5=RShift, bit6=RAlt, bit7=RGUI

```c
/* USB HID keyboard boot protocol → input_event_t */
static void hid_process_keyboard(uint8_t *report, uint8_t prev_report[8]) {
    uint8_t modifiers = report[0];

    /* Process new keycodes */
    for (int i = 2; i < 8; i++) {
        if (report[i] == 0) continue;
        /* Check if this keycode is new (not in prev_report) */
        int is_new = 1;
        for (int j = 2; j < 8; j++) {
            if (prev_report[j] == report[i]) { is_new = 0; break; }
        }
        if (!is_new) continue;

        input_event_t evt = {0};
        evt.type = INPUT_EVENT_KEY_PRESS;
        evt.keycode = report[i];
        /* Translate HID keycode → ASCII */
        evt.ascii = hid_keycode_to_ascii(report[i], modifiers);
        input_push_event(&evt);
    }

    /* Process released keycodes */
    for (int i = 2; i < 8; i++) {
        if (prev_report[i] == 0) continue;
        int released = 1;
        for (int j = 2; j < 8; j++) {
            if (report[j] == prev_report[i]) { released = 0; break; }
        }
        if (!released) continue;
        input_event_t evt = {0};
        evt.type = INPUT_EVENT_KEY_RELEASE;
        evt.keycode = prev_report[i];
        input_push_event(&evt);
    }

    memcpy(prev_report, report, 8);
}
```

### HID Keycode → ASCII table
Standard USB HID Usage Page 0x07 (keyboard) table. Key entries:
```c
/* Partial table — implement fully */
static const char hid_ascii_noshft[128] = {
    /* 0x00 */ 0, 0, 0, 0,
    /* 0x04 */ 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
    /* 0x11 */ 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    /* 0x1E */ '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    /* 0x28 */ '\n', 0x1B, '\b', '\t', ' ',
    /* 0x2D */ '-', '=', '[', ']', '\\', 0, ';', '\'', '`', ',', '.', '/',
};
```

### 6.2 HID Mouse (Boot Protocol)

3-byte report: [buttons, x_delta, y_delta] (or 4-byte with scroll):
```c
static void hid_process_mouse(uint8_t *report) {
    input_event_t evt = {0};
    int8_t dx = (int8_t)report[1];
    int8_t dy = (int8_t)report[2];
    if (dx != 0 || dy != 0) {
        evt.type = INPUT_EVENT_MOUSE_MOVE;
        evt.mouse_dx = dx;
        evt.mouse_dy = dy;
        input_push_event(&evt);
    }
    uint8_t btns = report[0] & 0x07;
    if (btns) {
        evt.type = INPUT_EVENT_MOUSE_BUTTON;
        evt.mouse_btn = btns;
        input_push_event(&evt);
    }
}
```

---

## 7. driver_t Registration

```c
/* In xhci.c — register with driver_manager */

#include "../driver.h"

static int xhci_probe(uint16_t vendor_id, uint16_t device_id) {
    (void)vendor_id; (void)device_id;
    /* PCI probe is class-based: class=0x0C, subclass=0x03, prog_if=0x30 */
    /* driver_manager calls probe for every PCI device;
     * we check class in xhci_init instead, return 1=match always */
    return 1;
}

static driver_t xhci_driver = {
    .name        = "xhci",
    .type        = DRIVER_TYPE_USB,
    .init        = xhci_init,
    .deinit      = xhci_deinit,
    .probe       = xhci_probe,
    .irq_handler = xhci_irq_handler,
};

/* Called by kernel at startup (before pci_init, or during pci probe) */
void xhci_init_subsystem(void) {
    driver_register(&xhci_driver);
}
```

The PCI class code for xHCI: `class=0x0C`, `subclass=0x03`, `prog_if=0x30`.

In `xhci_init()`:
```c
int xhci_init(void) {
    /* Find xHCI PCI device */
    pci_device_t *dev = pci_find_by_class(0x0C, 0x03, 0x30);
    if (!dev) {
        debug_log(DEBUG_LEVEL_INFO, "XHCI", "No xHCI controller found");
        return -1;
    }
    /* Enable bus mastering + memory space */
    pci_enable_bus_mastering(dev);
    /* ... full init sequence ... */
}
```

You will need to add `pci_find_by_class` and `pci_enable_bus_mastering` to `pci.c`, OR use existing APIs. Check `pci.h` first — if those functions don't exist, add them to `pci.c` (this is a shared file — coordinate, or use existing `pci_read_config32` to implement locally).

---

## 8. Configuration (`nexus_config.h` additions)

These defaults are already set in `nexus_config.h` by Agent A with efficiency-tuned values:

```c
/* ── USB ───────────────────────────────────────────────────────────────────── */

/* Maximum USB devices. 32 covers typical workstation setup. */
#define USB_MAX_DEVICES           32    /* was 64 — halved, saves 256 B per entry */

/* Ring sizes tuned to minimum functional size (per efficiency mandate):
 * 64 TRBs × 16B = 1024B per ring. One ring fits in 1 page with partner ring. */
#define XHCI_CMD_RING_SIZE        64    /* was 256 — command ring rarely has more than 10 queued */
#define XHCI_EVENT_RING_SIZE      64    /* was 256 — events processed immediately in IRQ */
#define XHCI_TRANSFER_RING_SIZE   16    /* was 64 — sufficient for HID (8-byte reports) */

/* Timeout constants */
#define USB_ENUM_TIMEOUT_MS       5000
#define USB_RESET_TIMEOUT_MS      250
#define USB_HID_POLL_INTERVAL_MS  10
```

### DMA Memory Budget for USB

| Buffer | Size | Physical Allocation |
|---|---|---|
| Command ring (64 TRBs) | 1 KB | 1 page shared with event ring |
| Event ring (64 TRBs) | 1 KB | shares page with command ring |
| Event Ring Segment Table | 64 B | shares page with above |
| Device Context array (256 ptrs) | 2 KB | 1 page |
| Per-device Input Context | 512 B | 1 page per active device |
| Per-endpoint Transfer ring (16) | 256 B | pack 16 rings per page |
| **Total per controller** | **~8 KB physical** | **2–4 pages** |

**Packing trick**: Allocate one 4 KB page and place both command ring (1 KB) + event ring (1 KB) + ERST (64 B) in it:
```c
static void xhci_alloc_rings(xhci_t *hc) {
    uint64_t phys;
    uint8_t *page = dma_alloc(4096, &phys);
    hc->cmd_ring       = (xhci_trb_t *)page;           /* offset 0, 64×16=1024B */
    hc->event_ring     = (xhci_trb_t *)(page + 1024);  /* offset 1024, 1024B */
    hc->erst           = (xhci_erst_t *)(page + 2048); /* offset 2048, 64B */
    hc->cmd_ring_phys  = phys;
    hc->event_ring_phys = phys + 1024;
    hc->erst_phys      = phys + 2048;
    /* Total: 1 page (4 KB) for all rings */
}
```

### Lazy Device Struct Allocation

Allocate `usb_device_t` structs ONLY when a device is actually enumerated:
```c
/* In xhci.c: */
static usb_device_t *usb_devices[USB_MAX_DEVICES] = {NULL};  /* 32 pointers = 256 B */
/* NOT: static usb_device_t usb_devices[USB_MAX_DEVICES]; ← would waste 32*sizeofdevice */

/* On device connect: */
usb_devices[slot] = kmalloc(sizeof(usb_device_t));  /* only when needed */
memset(usb_devices[slot], 0, sizeof(usb_device_t));

/* On device disconnect: */
kfree(usb_devices[slot]);
usb_devices[slot] = NULL;
```



---

## 9. QEMU Testing

```bash
# Test xHCI + USB keyboard:
make run QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0"

# Test xHCI + USB mouse:
make run QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-mouse,bus=xhci.0"

# Both:
make run QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-mouse,bus=xhci.0"

# With serial debug (recommended):
make all lodbug && make run lodbug QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0"
```

### Expected Serial Output (LODBUG mode)
```
[XHCI] Controller found: PCI 00:04.0
[XHCI] MMIO base: 0xFEBF0000, ports: 4, slots: 64
[XHCI] Controller reset OK
[XHCI] Command ring @ phys=0x00500000
[XHCI] Event ring @ phys=0x00501000
[XHCI] Controller started
[USB]  Port 0: device connected (FullSpeed)
[USB]  Port 0: reset complete
[USB]  USB: ENABLE_SLOT → slot_id=1
[USB]  USB: ADDRESS_DEVICE → CC=1 (Success)
[USB]  USB: GET_DESCRIPTOR Device (18 bytes)
[USB]  USB: Device 045E:0745 (class=0x00)
[USB]  USB: SET_CONFIGURATION(1) OK
[USB]  USB: Interface 0: class=03 (HID), subclass=01, protocol=01 (Keyboard)
[HID]  Keyboard attached on slot 1
[HID]  Key press: HID=0x04 ASCII='a'
```

---

## 10. Debugging Common xHCI Issues

If controller never starts:
```
1. Check PCI BAR0 — should be non-zero MMIO address
2. Verify pci_enable_bus_mastering() was called
3. Check USBSTS.CNR — if 1, controller not ready after reset (wait longer)
4. Verify MMIO is mapped as uncacheable (PAGE_NOCACHE flag in vmm_map_page)
```

If enumeration fails:
```
1. Check PORTSC.PED after reset — must be 1 for port to be enabled
2. Check PORTSC.SPEED — 1=FS, 2=LS, 3=HS, 4=SS
3. Enable slot completion event must CC=1 (Success)
4. ADDRESS_DEVICE fails if Input Context is malformatted — dump all fields via LODBUG
```

Use the LODBUG `nxdbg>` console if stuck:
```
nxdbg> mem 0xFEBF0000 64    ← dump xHCI cap regs
nxdbg> mem <dcbaa_phys> 32  ← dump device context array
```

---

## 11. Memory Efficiency Requirements (MANDATORY)

> Read `planned_implementations/MEMORY_EFFICIENCY_MANDATE.md` BEFORE implementing.

See section 8 above for the updated ring size constants and DMA packing strategy.

Additional efficiency rules for this agent:

**No static UCS2 keyboard tables in BSS**:
```c
/* BAD: sits in BSS always, even if no keyboard: */
static char hid_ascii_table[256];  /* 256 bytes BSS */

/* GOOD: read-only const in .rodata (shared in page table, not duplicated per process): */
static const char hid_ascii_noshft[128] = { ... };  /* .rodata, shared */
```

**Interrupt-driven polling, not polling loop**:
```c
/* BAD: wasted CPU checking for USB events in a loop */
for (;;) { process_events(); pit_sleep(10); }

/* GOOD: IRQ-driven. Event ring processed in IRQ handler ONLY */
void xhci_irq_handler(registers_t *regs) {
    if (read_reg32(op_base + XHCI_OP_USBSTS) & XHCI_USBSTS_EINT) {
        write_reg32(op_base + XHCI_OP_USBSTS, XHCI_USBSTS_EINT); /* clear */
        xhci_process_events();
    }
    pic_send_eoi(irq_line);
}
```

**HID previous-report storage: per-device not global**:
```c
typedef struct usb_hid {
    uint8_t  prev_report[8];   /* last report — 8 bytes, in usb_device struct */
    int      type;             /* HID_KEYBOARD or HID_MOUSE */
} usb_hid_t;
/* Cost: 8+4 bytes per HID device. Allocated lazily on device attach. */
```

### Memory Budget (document in usb.md):

| Allocation | Size | When | Freed |
|---|---|---|---|
| MMIO mapping (virtual) | 64 KB virtual | boot | never |
| DMA rings page (cmd+evt+erst) | 4 KB physical | boot | never |
| Device Context Array | 4 KB physical | boot | never |
| Per-device Input Context | 4 KB physical | on connect | on disconnect |
| per-endpoint transfer ring (pack 16/page) | 256 B/device | on connect | on disconnect |
| usb_device_t (lazy) | ~128 B | on connect | on disconnect |
| HID prev_report buffer | 8 B | on attach | on detach |
| **Total at boot (no devices)** | **~12 KB physical** | | |
| **Per USB device attached** | **~4.5 KB physical** | | |

---

## 12. Documentation Required (write LAST)

Create `docs/phase4/usb.md`:
- xHCI hardware overview (caps/op/runtime/doorbell regions)
- TRB ring structure diagram
- Initialization sequence step-by-step
- DMA memory allocation pattern (why bounce buffers, explain BUG-003)
- USB enumeration flow diagram
- HID class driver: keyboard + mouse report formats
- HID keycode table reference
- Config keys and their effects
- QEMU test commands
- Physical hardware notes (potential BIOS handoff issues)
- Future: USB MSC (mass storage), audio, xHCI MSI-X, USB 3.0 SuperSpeed

---

## 12. BIOS Handoff

On real hardware, the BIOS may own the xHCI controller. You must perform BIOS handoff:
```c
/* Check XECP (Extended Capabilities) for USBLEGSUP */
uint32_t hccparams1 = read_reg32(cap_base + XHCI_CAP_HCCPARAMS1);
uint32_t xecp_offset = ((hccparams1 >> 16) & 0xFFFF) << 2;
if (xecp_offset) {
    volatile uint32_t *xecp = (volatile uint32_t *)(mmio_virt + xecp_offset);
    /* Capability ID 1 = USB Legacy Support */
    while ((*xecp & 0xFF) != 0) {
        if ((*xecp & 0xFF) == 0x01) {
            /* USBLEGSUP: set OS owned bit */
            *xecp |= (1 << 24);
            /* Wait for BIOS to release (bit 16 = BIOS owns) */
            uint64_t t = pit_get_ticks() + 2000;
            while ((*xecp & (1 << 16)) && pit_get_ticks() < t);
        }
        uint32_t next = (*xecp >> 8) & 0xFF;
        if (!next) break;
        xecp += next;
    }
}
```

---

## 13. Starting Agent Prompt

```
You are a Driver Agent implementing NexusOS Phase 4 Tasks 4.7 (xHCI host controller),
4.8 (USB device enumeration), and 4.9 (USB HID driver).

You can START IMMEDIATELY — no dependency on Agent A or B.

READ FIRST (mandatory, in order):
1. planned_implementations/AGENT_D_USB.md (this document — full spec)
2. AGENTS.md
3. knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md
4. knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md
5. knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md
   (CRITICAL: BUG-003 describes DMA issue — apply bounce buffer pattern)
6. kernel/src/drivers/driver.h
7. kernel/src/drivers/driver_manager.c
8. kernel/src/drivers/pci/pci.h + pci.c
9. kernel/src/drivers/pci/pci_ids.h
10. kernel/src/mm/pmm.h, vmm.h, heap.h
11. kernel/src/drivers/input/input_manager.c
12. kernel/src/drivers/input/input_event.h
13. kernel/src/drivers/input/ps2_keyboard.c (reference for key translation)

YOUR TASK:
Implement the NexusOS USB stack as specified in AGENT_D_USB.md.

Create these files:
- kernel/src/drivers/usb/xhci_regs.h (MMIO offsets — constants only)
- kernel/src/drivers/usb/xhci_trb.h (TRB types + structs)
- kernel/src/drivers/usb/xhci.h + xhci.c (host controller driver)
- kernel/src/drivers/usb/usb_device.h + usb_device.c (enumeration)
- kernel/src/drivers/usb/usb_hid.h + usb_hid.c (HID class driver)
- kernel/src/drivers/usb/usb_hub.h + usb_hub.c (basic hub support)

RULES:
- ALL DMA buffers: use physical addresses, verify < 4GB, zero after alloc
  (Apply bounce buffer pattern from BUG-003 — never use vmm_get_phys on heap ptrs)
- ALL MMIO: volatile pointers, NoCache page mapping flag
- Build must pass 0 errors, 0 C warnings after every file
- Use debug_log for all xHCI init steps and USB events
- Register driver via driver_register() in xhci_init_subsystem()
- Search web if stuck on xHCI spec details — xhci spec 1.2 is public

VERIFICATION:
Run: make all lodbug && make run lodbug QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0"
Expected: USB keyboard detected in serial log, keystrokes produce input_event_t

BUILD: 0 errors, 0 warnings.

WHEN DONE:
1. Write docs/phase4/usb.md
2. Report: files created, QEMU test output, build status
3. Signal: "AGENT D COMPLETE — USB keyboard/mouse producing input events"
```
