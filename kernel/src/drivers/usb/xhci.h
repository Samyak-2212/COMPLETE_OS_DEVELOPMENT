/* ============================================================================
 * xhci.h — NexusOS xHCI Host Controller Driver Public API
 * Purpose: Public types and functions for the xHCI host controller layer
 * Author: NexusOS Driver Agent (Phase 4, Task 4.7)
 * ============================================================================ */

#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include "drivers/usb/xhci_trb.h"
#include "drivers/pci/pci.h"

/* Ring sizes (efficiency-tuned per AGENT_D_USB.md §8) */
#define XHCI_CMD_RING_SIZE       64    /* command ring TRB count */
#define XHCI_EVENT_RING_SIZE     64    /* event ring TRB count   */
#define XHCI_XFER_RING_SIZE      16    /* per-endpoint transfer ring */

/* Maximum USB devices we track (lazy alloc — this is just a pointer table) */
#define USB_MAX_DEVICES          32

/* Timeout constants (milliseconds) */
#define USB_RESET_TIMEOUT_MS     250
#define USB_ENUM_TIMEOUT_MS      5000

/* ── xHCI controller state ───────────────────────────────────────────────── */

typedef struct {
    /* PCI device info */
    uint8_t  pci_bus;
    uint8_t  pci_dev;
    uint8_t  pci_func;
    uint8_t  irq_line;

    /* MMIO regions (virtual addresses) */
    uint64_t mmio_virt;      /* Base of entire MMIO window */
    uint64_t cap_base;       /* = mmio_virt (capability regs) */
    uint64_t op_base;        /* = mmio_virt + CAPLENGTH */
    uint64_t rt_base;        /* = mmio_virt + RTSOFF */
    uint64_t db_base;        /* = mmio_virt + DBOFF */
    uint64_t mmio_size;      /* Total MMIO window size */

    /* Controller parameters */
    uint8_t  max_slots;
    uint8_t  max_ports;

    /* ── Command Ring ── */
    xhci_trb_t *cmd_ring;       /* Virtual address */
    uint64_t    cmd_ring_phys;  /* Physical address (DMA address) */
    uint32_t    cmd_enqueue;    /* Producer index */
    uint8_t     cmd_cycle;      /* Producer cycle bit */

    /* ── Event Ring ── */
    xhci_trb_t *event_ring;      /* Virtual address */
    uint64_t    event_ring_phys; /* Physical address */
    uint32_t    evt_dequeue;     /* Consumer index */
    uint8_t     evt_cycle;       /* Consumer cycle bit */

    /* ── Event Ring Segment Table (1 entry) ── */
    xhci_erst_t *erst;           /* Virtual */
    uint64_t     erst_phys;      /* Physical */

    /* ── Device Context Base Address Array (256 × 8 bytes) ── */
    uint64_t    *dcbaa;          /* Virtual  */
    uint64_t     dcbaa_phys;     /* Physical */

    /* Rings page: packs cmd_ring + event_ring + erst into 1 page */
    uint64_t  rings_page_phys;

    /* DCBAA page */
    uint64_t  dcbaa_page_phys;
} xhci_t;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Register xHCI driver with the driver manager.
 * Call once during kernel init BEFORE pci_init(). */
void xhci_init_subsystem(void);

/* Enqueue a command TRB and ring doorbell 0.
 * Returns 0 on success, negative on ring full. */
int xhci_command_ring_push(xhci_t *hc, xhci_trb_t *trb);

/* Block-poll for a CommandCompletion event matching the given command TRB phys.
 * Returns 0 on success, -1 on timeout.
 * On success: *slot_id_out = assigned slot, *cc_out = completion code. */
int xhci_wait_command(xhci_t *hc, uint64_t cmd_trb_phys,
                      uint8_t *slot_id_out, uint8_t *cc_out);

/* Read a 32-bit operational register */
uint32_t xhci_op_read32(xhci_t *hc, uint32_t offset);

/* Write a 32-bit operational register */
void xhci_op_write32(xhci_t *hc, uint32_t offset, uint32_t val);

/* Read a 32-bit capability register */
uint32_t xhci_cap_read32(xhci_t *hc, uint32_t offset);

/* Read a 32-bit runtime register */
uint32_t xhci_rt_read32(xhci_t *hc, uint32_t offset);

/* Write a 32-bit runtime register */
void xhci_rt_write32(xhci_t *hc, uint32_t offset, uint32_t val);

/* Write a 32-bit doorbell register */
void xhci_db_write32(xhci_t *hc, uint32_t slot, uint32_t val);

/* Process event ring — called from IRQ handler and polling loops */
void xhci_process_events(xhci_t *hc);

/* Global xHCI controller pointer (single-controller system) */
extern xhci_t *g_xhci;

#endif /* XHCI_H */
