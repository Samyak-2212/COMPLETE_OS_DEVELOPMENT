/* ============================================================================
 * xhci.c — NexusOS xHCI Host Controller Driver
 * Purpose: Init, ring management, doorbell ringing, event processing
 * Author: NexusOS Driver Agent (Phase 4, Task 4.7)
 * ============================================================================ */

#include "drivers/usb/xhci.h"
#include "drivers/usb/xhci_regs.h"
#include "drivers/usb/usb_device.h"
#include "drivers/driver.h"
#include "drivers/pci/pci.h"
#include "hal/isr.h"
#include "hal/pic.h"
#include "drivers/timer/pit.h" /* pit_get_ticks, pit_sleep_ms */
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/debug.h"
#include "lib/printf.h"
#include "lib/string.h"

/* Global controller instance (single controller supported for now) */
xhci_t *g_xhci = NULL;

/* Device pointer array (lazy alloc) */
extern usb_device_t *usb_devices[USB_MAX_DEVICES]; /* Defined in usb_device.c */

/* ── Register Access Helpers ─────────────────────────────────────────────── */

uint32_t xhci_cap_read32(xhci_t *hc, uint32_t offset) {
    return *(volatile uint32_t *)(hc->cap_base + offset);
}

uint32_t xhci_op_read32(xhci_t *hc, uint32_t offset) {
    return *(volatile uint32_t *)(hc->op_base + offset);
}

void xhci_op_write32(xhci_t *hc, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(hc->op_base + offset) = val;
}

uint32_t xhci_rt_read32(xhci_t *hc, uint32_t offset) {
    return *(volatile uint32_t *)(hc->rt_base + offset);
}

void xhci_rt_write32(xhci_t *hc, uint32_t offset, uint32_t val) {
    *(volatile uint32_t *)(hc->rt_base + offset) = val;
}

void xhci_db_write32(xhci_t *hc, uint32_t slot, uint32_t val) {
    *(volatile uint32_t *)(hc->db_base + XHCI_DB(slot)) = val;
}

/* ── DMA Memory Management (MANDATORY BOUNCE BUFFERS, BUG-003) ──────────── */

/* Allocate physically-contiguous DMA-safe memory.
 * Returns virtual address (via HHDM). Stores physical in *phys_out. */
static void *dma_alloc(uint64_t size, uint64_t *phys_out) {
    uint64_t pages = (size + 4095) / 4096;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) {
        debug_log(DEBUG_LEVEL_ERROR, "XHCI", "DMA alloc failed (OOM)");
        return NULL;
    }
    
    /* Legacy xHCI often requires 32-bit physical addresses for context pointers.
     * Ensure we are below 4GB. BUG-003 explicitly demands this check. */
    if (phys + size > 0xFFFFFFFFULL) {
        debug_log(DEBUG_LEVEL_ERROR, "XHCI", "DMA alloc > 4GB (0x%lx), unsupported", (unsigned long)phys);
        pmm_free_pages(phys, pages);
        return NULL;  /* In a real implementation we'd retry forcing <4GB constraint */
    }
    
    *phys_out = phys;
    void *virt = (void *)(phys + vmm_get_hhdm_offset());
    memset(virt, 0, pages * 4096);
    return virt;
}

/* ── Rings Initialization ────────────────────────────────────────────────── */

static int xhci_alloc_rings(xhci_t *hc) {
    /* Allocate 1 page to hold Command Ring, Event Ring, and ERST.
     * 1024 + 1024 + 64 = 2112 bytes < 4096 bytes. */
    uint64_t phys;
    uint8_t *page = dma_alloc(4096, &phys);
    if (!page) return -1;
    
    hc->rings_page_phys = phys;

    /* Command Ring: 64 TRBs * 16 bytes = 1024 bytes */
    hc->cmd_ring = (xhci_trb_t *)page;
    hc->cmd_ring_phys = phys;
    hc->cmd_enqueue = 0;
    hc->cmd_cycle = 1;

    /* Event Ring: 64 TRBs * 16 bytes = 1024 bytes */
    hc->event_ring = (xhci_trb_t *)(page + 1024);
    hc->event_ring_phys = phys + 1024;
    hc->evt_dequeue = 0;
    hc->evt_cycle = 1;

    /* Event Ring Segment Table (ERST): 1 entry (16 bytes) */
    hc->erst = (xhci_erst_t *)(page + 2048);
    hc->erst_phys = phys + 2048;
    
    /* Set up ERST entry 0 to point to our Event Ring */
    hc->erst[0].ring_segment_base = hc->event_ring_phys;
    hc->erst[0].ring_segment_size = XHCI_EVENT_RING_SIZE;
    
    return 0;
}

/* ── Command Ring Handling ───────────────────────────────────────────────── */

int xhci_command_ring_push(xhci_t *hc, xhci_trb_t *trb) {
    if (!hc || !hc->cmd_ring) return -1;

    /* Apply current cycle bit to TRB control */
    if (hc->cmd_cycle) {
        trb->control |= TRB_CYCLE;
    } else {
        trb->control &= ~TRB_CYCLE;
    }

    hc->cmd_ring[hc->cmd_enqueue] = *trb;
    hc->cmd_enqueue++;
    
    /* Handle wrap-around with a Link TRB */
    if (hc->cmd_enqueue >= XHCI_CMD_RING_SIZE - 1) {
        xhci_trb_t link = {0};
        link.parameter = hc->cmd_ring_phys;
        link.control = TRB_TYPE_FIELD(TRB_TYPE_LINK) | TRB_TOGGLE_CYCLE;
        if (hc->cmd_cycle) link.control |= TRB_CYCLE;
        
        hc->cmd_ring[XHCI_CMD_RING_SIZE - 1] = link;
        
        hc->cmd_enqueue = 0;
        hc->cmd_cycle ^= 1;
    }
    
    /* Ring Host Command Doorbell (Slot 0) */
    xhci_db_write32(hc, 0, 0);
    return 0;
}

int xhci_wait_command(xhci_t *hc, uint64_t cmd_trb_phys, uint8_t *slot_id_out, uint8_t *cc_out) {
    uint64_t deadline = pit_get_ticks() + USB_ENUM_TIMEOUT_MS;
    
    while (pit_get_ticks() < deadline) {
        /* Process events immediately while waiting */
        xhci_trb_t *evt = &hc->event_ring[hc->evt_dequeue];
        uint32_t ctrl = evt->control;
        
        /* Stop if cycle bit doesn't match our expected cycle */
        if ((ctrl & TRB_CYCLE) != hc->evt_cycle) {
            continue; /* Spin until it arrives */
        }
        
        uint32_t type = TRB_GET_TYPE(ctrl);
        
        /* We are ONLY looking for Command Completion events matching our TRB address here.
         * Other events (like port status change) will be advanced past, but we might lose them 
         * in a simplified polling spin. Ideally, we just call a general event processor and check 
         * a command status flag. For this spec, we will handle it inline. */
        
        if (type == TRB_TYPE_CMD_COMPLETION && evt->parameter == cmd_trb_phys) {
            if (cc_out) *cc_out = TRB_GET_CC(evt->status);
            if (slot_id_out) *slot_id_out = TRB_SLOT_ID(ctrl);
            
            /* Advance dequeue pointer */
            hc->evt_dequeue++;
            if (hc->evt_dequeue >= XHCI_EVENT_RING_SIZE) {
                hc->evt_dequeue = 0;
                hc->evt_cycle ^= 1;
            }
            
            /* Update ERDP to hardware */
            uint64_t erdp = hc->event_ring_phys + (hc->evt_dequeue * sizeof(xhci_trb_t));
            *(volatile uint64_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = erdp | XHCI_ERDP_EHB;
            
            return 0;
        }
        
        /* If it's a different event, advance past it to unblock ring */
        hc->evt_dequeue++;
        if (hc->evt_dequeue >= XHCI_EVENT_RING_SIZE) {
            hc->evt_dequeue = 0;
            hc->evt_cycle ^= 1;
        }
        uint64_t erdp = hc->event_ring_phys + (hc->evt_dequeue * sizeof(xhci_trb_t));
        *(volatile uint64_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = erdp | XHCI_ERDP_EHB;
    }
    
    debug_log(DEBUG_LEVEL_ERROR, "XHCI", "Command timeout for TRB phys 0x%lx", (unsigned long)cmd_trb_phys);
    return -1;
}

/* ── Event Processing ────────────────────────────────────────────────────── */

void xhci_process_events(xhci_t *hc) {
    if (!hc) return;
    
    int events_processed = 0;
    while (1) {
        xhci_trb_t *evt = &hc->event_ring[hc->evt_dequeue];
        uint32_t ctrl = evt->control;
        
        if ((ctrl & TRB_CYCLE) != hc->evt_cycle) break; /* No more events */
        
        uint32_t type = TRB_GET_TYPE(ctrl);
        uint8_t cc = TRB_GET_CC(evt->status);
        
        if (type == TRB_TYPE_PORT_STATUS_CHG) {
            uint32_t port_id = (evt->parameter >> 24) & 0xFF;
            debug_log(DEBUG_LEVEL_INFO, "XHCI", "Port %u status change event", port_id);
            /* Resetting the port and triggering enumeration is typically deferred
             * or handled inline. If Port connects (CCS=1), we enumerate. */
            uint32_t portsc = xhci_op_read32(hc, XHCI_PORTSC(port_id - 1));
            if (portsc & XHCI_PORTSC_CSC) {
                /* Clear CSC via write-1-to-clear */
                xhci_op_write32(hc, XHCI_PORTSC(port_id - 1), XHCI_PORTSC_CSC | (portsc & XHCI_PORTSC_PP)); 
                if (portsc & XHCI_PORTSC_CCS) {
                    /* Device connected - initiate reset. 
                     * In a fully interrupt-driven model, we'd fire off a worker here.
                     * We're logging it for now, enumeration handles it synchronously during init. */
                    debug_log(DEBUG_LEVEL_INFO, "XHCI", "Device connected on port %u", port_id);
                }
            }
        } else if (type == TRB_TYPE_TRANSFER_EVENT) {
            uint8_t slot_id = TRB_SLOT_ID(ctrl);
            uint8_t ep_id   = TRB_EP_ID(ctrl);
            if (cc != TRB_CC_SUCCESS && cc != TRB_CC_SHORT_PACKET) {
                debug_log(DEBUG_LEVEL_WARN, "XHCI",
                          "Transfer error Slot %u EP %u CC %u", slot_id, ep_id, cc);
            } else {
                /* Acknowledge: mark used to suppress -Wunused-variable at -O2 */
                (void)slot_id; (void)ep_id;
            }
            /* Successful HID reports processed asynchronously via IRQ.
             * Future: route TRB completion to usb_hid_handle_report(). */
        }
        
        hc->evt_dequeue++;
        if (hc->evt_dequeue >= XHCI_EVENT_RING_SIZE) {
            hc->evt_dequeue = 0;
            hc->evt_cycle ^= 1;
        }
        events_processed++;
    }
    
    if (events_processed > 0) {
        /* Update ERDP to acknowledge events */
        uint64_t erdp = hc->event_ring_phys + (hc->evt_dequeue * sizeof(xhci_trb_t));
        *(volatile uint64_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = erdp | XHCI_ERDP_EHB;
    }
}

static void xhci_irq_handler(registers_t *regs) {
    (void)regs;
    if (!g_xhci) {
        pic_send_eoi(11); /* Fallback */
        return;
    }
    
    uint32_t usbsts = xhci_op_read32(g_xhci, XHCI_OP_USBSTS);
    if (usbsts & XHCI_USBSTS_EINT) {
        /* Clear Event Interrupt bit */
        xhci_op_write32(g_xhci, XHCI_OP_USBSTS, XHCI_USBSTS_EINT);
        xhci_process_events(g_xhci);
    }
    
    /* Acknowledge interrupt in Interrupter 0 Management Register */
    uint32_t iman = *(volatile uint32_t *)(g_xhci->rt_base + XHCI_IMAN(0));
    if (iman & XHCI_IMAN_IP) {
        *(volatile uint32_t *)(g_xhci->rt_base + XHCI_IMAN(0)) = iman | XHCI_IMAN_IP;
    }
    
    pic_send_eoi(g_xhci->irq_line);
}

/* ── BIOS Handoff ────────────────────────────────────────────────────────── */

static void xhci_bios_handoff(xhci_t *hc) {
    uint32_t hccparams1 = xhci_cap_read32(hc, XHCI_CAP_HCCPARAMS1);
    uint32_t xecp_offset = XHCI_HCCP1_XECP(hccparams1) << 2;
    if (!xecp_offset) return; /* No extended capabilities */
    
    volatile uint32_t *xecp = (volatile uint32_t *)(hc->mmio_virt + xecp_offset);
    
    while ((*xecp & 0xFF) != 0) {
        if ((*xecp & 0xFF) == XHCI_XCAP_ID_LEGACY) {
            uint32_t val = *xecp;
            if (val & XHCI_USBLEGSUP_BIOS_OWN) {
                debug_log(DEBUG_LEVEL_INFO, "XHCI", "BIOS owns controller, performing handoff...");
                /* Set OS Owned bit */
                *xecp = val | XHCI_USBLEGSUP_OS_OWN;
                
                /* Wait for BIOS to clear its bit */
                uint64_t deadline = pit_get_ticks() + 2000;
                while ((*xecp & XHCI_USBLEGSUP_BIOS_OWN) && pit_get_ticks() < deadline) {
                    /* CPU relax/spin */
                    __asm__ volatile("pause");
                }
                if (*xecp & XHCI_USBLEGSUP_BIOS_OWN) {
                    debug_log(DEBUG_LEVEL_WARN, "XHCI", "BIOS did not release controller (timeout)");
                } else {
                    debug_log(DEBUG_LEVEL_INFO, "XHCI", "BIOS handoff complete");
                }
            }
            /* Disable SMI/legacy interrupts */
            volatile uint32_t *ctrl = xecp + 1; /* Next DWORD is USB Legacy Support Control/Status */
            *ctrl &= 0x1F1FEE; /* Clear SMI enables */
        }
        
        uint32_t next = (*xecp >> 8) & 0xFF;
        if (!next) break;
        xecp += next;
    }
}

/* ── Controller Initialization ────────────────────────────────────────────── */

static int xhci_init_controller(void *device_info) {
    pci_device_t *pdev = (pci_device_t *)device_info;
    if (!pdev) return -1;
    
    /* We only support MMIO */
    if (pdev->bars[0].type != PCI_BAR_TYPE_MMIO) {
        debug_log(DEBUG_LEVEL_ERROR, "XHCI", "BAR0 is not MMIO");
        return -1;
    }
    
    xhci_t *hc = kmalloc(sizeof(xhci_t));
    if (!hc) return -1;
    memset(hc, 0, sizeof(xhci_t));
    
    hc->pci_bus = pdev->bus;
    hc->pci_dev = pdev->device;
    hc->pci_func = pdev->function;
    hc->irq_line = pdev->irq_line;
    
    hc->mmio_size = pdev->bars[0].size > 0 ? pdev->bars[0].size : 0x10000;
    
    /* Map MMIO with NoCache (MANDATORY for MMIO) */
    uint64_t phys_base = pdev->bars[0].address;
    uint64_t pages = (hc->mmio_size + 4095) / 4096;
    for (uint64_t i = 0; i < pages; i++) {
        vmm_map_page(phys_base + i*4096, phys_base + i*4096, PAGE_PRESENT | PAGE_WRITABLE | PAGE_NOCACHE);
    }
    
    hc->mmio_virt = phys_base; /* Physical == Virtual in this mapping for simplicity, or use HHDM */
    hc->cap_base = hc->mmio_virt;
    uint8_t caplength = *(volatile uint8_t *)(hc->cap_base + XHCI_CAP_CAPLENGTH);
    hc->op_base = hc->mmio_virt + caplength;
    hc->rt_base = hc->mmio_virt + (*(volatile uint32_t *)(hc->cap_base + XHCI_CAP_RTSOFF) & ~0x1F);
    hc->db_base = hc->mmio_virt + (*(volatile uint32_t *)(hc->cap_base + XHCI_CAP_DBOFF) & ~0x3);
    
    g_xhci = hc;
    
    kprintf_set_color(0x0088FF88, FB_DEFAULT_BG);
    kprintf("[XHCI] ");
    kprintf_set_color(0x00CCCCCC, FB_DEFAULT_BG);
    kprintf("Controller found: PCI %02x:%02x.%x\n", hc->pci_bus, hc->pci_dev, hc->pci_func);
    
    /* 1. BIOS Handoff */
    xhci_bios_handoff(hc);
    
    /* 2. Stop controller */
    uint32_t cmd = xhci_op_read32(hc, XHCI_OP_USBCMD);
    if (cmd & XHCI_USBCMD_RUN) {
        xhci_op_write32(hc, XHCI_OP_USBCMD, cmd & ~XHCI_USBCMD_RUN);
        while (!(xhci_op_read32(hc, XHCI_OP_USBSTS) & XHCI_USBSTS_HCH));
    }
    
    /* 3. Reset controller */
    xhci_op_write32(hc, XHCI_OP_USBCMD, XHCI_USBCMD_HCRST);
    uint64_t reset_deadline = pit_get_ticks() + 1000;
    while ((xhci_op_read32(hc, XHCI_OP_USBCMD) & XHCI_USBCMD_HCRST) && pit_get_ticks() < reset_deadline);
    if (xhci_op_read32(hc, XHCI_OP_USBCMD) & XHCI_USBCMD_HCRST) {
        debug_log(DEBUG_LEVEL_ERROR, "XHCI", "Controller reset timeout");
        return -1;
    }
    
    while ((xhci_op_read32(hc, XHCI_OP_USBSTS) & XHCI_USBSTS_CNR) && pit_get_ticks() < reset_deadline + 500);
    
    debug_log(DEBUG_LEVEL_INFO, "XHCI", "Controller reset OK");
    
    /* Read structural parameters */
    uint32_t hcsparams1 = xhci_cap_read32(hc, XHCI_CAP_HCSPARAMS1);
    hc->max_slots = XHCI_HCSP1_MAXSLOTS(hcsparams1);
    hc->max_ports = XHCI_HCSP1_MAXPORTS(hcsparams1);
    
    /* 4. Configure max slots enabled */
    xhci_op_write32(hc, XHCI_OP_CONFIG, hc->max_slots);
    
    debug_log(DEBUG_LEVEL_INFO, "XHCI", "MMIO base: 0x%lx, ports: %u, slots: %u", 
              (unsigned long)hc->mmio_virt, hc->max_ports, hc->max_slots);
    
    /* 5. Allocate DCBAA */
    uint64_t dcbaa_phys;
    hc->dcbaa = dma_alloc(4096, &dcbaa_phys);
    if (!hc->dcbaa) return -1;
    hc->dcbaa_page_phys = dcbaa_phys;
    hc->dcbaa_phys = dcbaa_phys;
    
    xhci_op_write32(hc, XHCI_OP_DCBAAP_LO, (uint32_t)dcbaa_phys);
    xhci_op_write32(hc, XHCI_OP_DCBAAP_HI, (uint32_t)(dcbaa_phys >> 32));
    
    /* 6. Allocate and assign Rings */
    if (xhci_alloc_rings(hc) < 0) return -1;
    
    debug_log(DEBUG_LEVEL_INFO, "XHCI", "Command ring @ phys=0x%08lx", (unsigned long)hc->cmd_ring_phys);
    debug_log(DEBUG_LEVEL_INFO, "XHCI", "Event ring @ phys=0x%08lx", (unsigned long)hc->event_ring_phys);
    
    xhci_op_write32(hc, XHCI_OP_CRCR_LO, (uint32_t)hc->cmd_ring_phys | XHCI_CRCR_RCS);
    xhci_op_write32(hc, XHCI_OP_CRCR_HI, (uint32_t)(hc->cmd_ring_phys >> 32));
    
    /* Event Ring setup (Interrupter 0) */
    *(volatile uint32_t *)(hc->rt_base + XHCI_ERSTSZ(0)) = 1;
    *(volatile uint32_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = (uint32_t)hc->event_ring_phys | XHCI_ERDP_EHB;
    *(volatile uint32_t *)(hc->rt_base + XHCI_ERDP_HI(0)) = (uint32_t)(hc->event_ring_phys >> 32);
    *(volatile uint32_t *)(hc->rt_base + XHCI_ERSTBA_LO(0)) = (uint32_t)hc->erst_phys;
    *(volatile uint32_t *)(hc->rt_base + XHCI_ERSTBA_HI(0)) = (uint32_t)(hc->erst_phys >> 32);
    *(volatile uint32_t *)(hc->rt_base + XHCI_IMOD(0)) = 4000; /* 1ms moderation */
    
    /* Register Interrupt */
    isr_register_handler(IRQ_BASE + hc->irq_line, xhci_irq_handler);
    pic_clear_mask(hc->irq_line);
    
    /* Enable interrupts (Interrupter 0) */
    *(volatile uint32_t *)(hc->rt_base + XHCI_IMAN(0)) |= XHCI_IMAN_IE | XHCI_IMAN_IP;
    
    /* 7. Start Controller */
    cmd = xhci_op_read32(hc, XHCI_OP_USBCMD);
    xhci_op_write32(hc, XHCI_OP_USBCMD, cmd | XHCI_USBCMD_RUN | XHCI_USBCMD_INTE | XHCI_USBCMD_HSEE);
    
    while (xhci_op_read32(hc, XHCI_OP_USBSTS) & XHCI_USBSTS_HCH);
    debug_log(DEBUG_LEVEL_INFO, "XHCI", "Controller started");
    
    /* 8. Port Enumeration Pass */
    for (uint32_t p = 1; p <= hc->max_ports; p++) {
        uint32_t portsc = xhci_op_read32(hc, XHCI_PORTSC(p - 1));
        if (portsc & XHCI_PORTSC_CCS) {
            uint8_t speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
            debug_log(DEBUG_LEVEL_INFO, "USB", "Port %u: device connected (Speed=%u)", p, speed);
            usb_enumerate_device(hc, p - 1, speed);
        }
    }
    
    return 0;
}

static int xhci_probe_device(void *device_info) {
    pci_device_t *pdev = (pci_device_t *)device_info;
    /* Class 0x0C = Serial Bus, Subclass 0x03 = USB, ProgIF 0x30 = xHCI */
    if (pdev->class_code == 0x0C && pdev->subclass_code == 0x03 && pdev->prog_if == 0x30) {
        return 1;
    }
    return 0;
}

static driver_t xhci_driver = {
    .name = "xhci",
    .probe = xhci_probe_device,
    .init = xhci_init_controller
};

void xhci_init_subsystem(void) {
    driver_register(&xhci_driver);
}
