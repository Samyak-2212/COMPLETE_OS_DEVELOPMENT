/* ============================================================================
 * usb_device.c — NexusOS USB Enumeration and Device Management
 * Purpose: Port reset, ENABLE_SLOT, ADDRESS_DEVICE, descriptor parsing
 * Author: NexusOS Driver Agent (Phase 4, Task 4.8)
 * ============================================================================ */

#include "drivers/usb/usb_device.h"
#include "drivers/usb/xhci.h"
#include "drivers/usb/xhci_regs.h"
#include "drivers/usb/usb_hid.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "drivers/timer/pit.h"
#include "lib/debug.h"
#include "lib/printf.h"
#include "lib/string.h"

/* Global device array (lazy allocation) */
usb_device_t *usb_devices[USB_MAX_DEVICES] = {NULL};

/* ── DMA Helper ──────────────────────────────────────────────────────────── */

static void *dma_alloc(uint64_t size, uint64_t *phys_out) {
    uint64_t pages = (size + 4095) / 4096;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return NULL;
    if (phys + size > 0xFFFFFFFFULL) {
        pmm_free_pages(phys, pages);
        return NULL;
    }
    *phys_out = phys;
    void *virt = (void *)(phys + vmm_get_hhdm_offset());
    memset(virt, 0, pages * 4096);
    return virt;
}

/* ── Control Transfer ────────────────────────────────────────────────────── */

int usb_control_transfer(xhci_t *hc, usb_device_t *dev,
                         usb_setup_t *setup,
                         uint64_t data_phys, uint16_t data_len, int dir_in) {
    if (!hc || !dev || !setup) return -1;

    usb_endpoint_t *ep0 = &dev->endpoints[0];
    
    /* 1. Setup Stage TRB */
    xhci_trb_t setup_trb = {0};
    /* Safe 8-byte read from packed struct — avoids unaligned access UB */
    uint64_t setup_raw;
    __builtin_memcpy(&setup_raw, setup, 8);
    setup_trb.parameter = setup_raw;
    setup_trb.status = (8u << 16); /* TRB Transfer Length = 8 */
    setup_trb.control = TRB_TYPE_FIELD(TRB_TYPE_SETUP_STAGE) | TRB_IDT;
    
    if (data_len > 0) {
        setup_trb.control |= (dir_in ? TRB_TRT_IN_DATA : TRB_TRT_OUT_DATA);
    } else {
        setup_trb.control |= TRB_TRT_NO_DATA;
    }

    /* Apply cycle bit and enqueue */
    if (ep0->xfer_cycle) setup_trb.control |= TRB_CYCLE;
    xhci_trb_t *ring = (xhci_trb_t *)ep0->xfer_ring_virt;
    ring[ep0->xfer_enqueue++] = setup_trb;

    /* 2. Data Stage TRB (optional) */
    if (data_len > 0) {
        xhci_trb_t data_trb = {0};
        data_trb.parameter = data_phys;
        data_trb.status = TRB_XFER_LEN(data_len);
        data_trb.control = TRB_TYPE_FIELD(TRB_TYPE_DATA_STAGE);
        if (dir_in) data_trb.control |= TRB_DIR_IN;
        
        if (ep0->xfer_cycle) data_trb.control |= TRB_CYCLE;
        ring[ep0->xfer_enqueue++] = data_trb;
    }

    /* 3. Status Stage TRB */
    xhci_trb_t status_trb = {0};
    status_trb.control = TRB_TYPE_FIELD(TRB_TYPE_STATUS_STAGE) | TRB_IOC;
    if (data_len == 0 || dir_in) {
        /* If data was IN or no data, status is OUT (DIR=0) */
        /* TRB_DIR_IN is 0 by default so we do nothing */
    } else {
        /* If data was OUT, status is IN */
        status_trb.control |= TRB_DIR_IN;
    }
    
    if (ep0->xfer_cycle) status_trb.control |= TRB_CYCLE;
    ring[ep0->xfer_enqueue++] = status_trb;
    
    /* Save phys of status TRB to wait on it */
    uint64_t status_trb_phys = ep0->xfer_ring_phys + ((ep0->xfer_enqueue - 1) * sizeof(xhci_trb_t));

    /* Wrap handling for EP0 ring (simplified: assuming Size=16 is enough for init) */
    if (ep0->xfer_enqueue >= XHCI_XFER_RING_SIZE - 1) {
        xhci_trb_t link = {0};
        link.parameter = ep0->xfer_ring_phys;
        link.control = TRB_TYPE_FIELD(TRB_TYPE_LINK) | TRB_TOGGLE_CYCLE;
        if (ep0->xfer_cycle) link.control |= TRB_CYCLE;
        ring[XHCI_XFER_RING_SIZE - 1] = link;
        ep0->xfer_enqueue = 0;
        ep0->xfer_cycle ^= 1;
    }

    /* Ring Doorbell for this device/endpoint (Slot ID, Endpoint ID 1 for EP0) */
    xhci_db_write32(hc, dev->slot_id, 1);

    /* Wait for transfer completion */
    uint64_t deadline = pit_get_ticks() + USB_ENUM_TIMEOUT_MS;
    while (pit_get_ticks() < deadline) {
        xhci_trb_t *evt = &hc->event_ring[hc->evt_dequeue];
        if ((evt->control & TRB_CYCLE) != hc->evt_cycle) continue;
        
        uint32_t type = TRB_GET_TYPE(evt->control);
        if (type == TRB_TYPE_TRANSFER_EVENT && evt->parameter == status_trb_phys) {
            uint8_t cc = TRB_GET_CC(evt->status);
            
            /* Advance dequeue */
            hc->evt_dequeue++;
            if (hc->evt_dequeue >= XHCI_EVENT_RING_SIZE) {
                hc->evt_dequeue = 0;
                hc->evt_cycle ^= 1;
            }
            uint64_t erdp = hc->event_ring_phys + (hc->evt_dequeue * sizeof(xhci_trb_t));
            *(volatile uint64_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = erdp | XHCI_ERDP_EHB;
            
            if (cc == TRB_CC_SUCCESS || cc == TRB_CC_SHORT_PACKET) return 0;
            debug_log(DEBUG_LEVEL_WARN, "USB", "Control transfer failed, CC=%u", cc);
            return -cc;
        }
        
        hc->evt_dequeue++;
        if (hc->evt_dequeue >= XHCI_EVENT_RING_SIZE) {
            hc->evt_dequeue = 0;
            hc->evt_cycle ^= 1;
        }
        uint64_t erdp = hc->event_ring_phys + (hc->evt_dequeue * sizeof(xhci_trb_t));
        *(volatile uint64_t *)(hc->rt_base + XHCI_ERDP_LO(0)) = erdp | XHCI_ERDP_EHB;
    }
    
    debug_log(DEBUG_LEVEL_ERROR, "USB", "Control transfer timeout on slot %u", dev->slot_id);
    return -1;
}

/* ── Enumeration ─────────────────────────────────────────────────────────── */

int usb_enumerate_device(xhci_t *hc, uint8_t port, uint8_t speed) {
    debug_log(DEBUG_LEVEL_INFO, "USB", "Enumerating port %u", port + 1);

    /* 1. Port Reset */
    uint32_t portsc = xhci_op_read32(hc, XHCI_PORTSC(port));
    xhci_op_write32(hc, XHCI_PORTSC(port), (portsc & XHCI_PORTSC_PP) | XHCI_PORTSC_PR);
    
    uint64_t dl = pit_get_ticks() + USB_RESET_TIMEOUT_MS;
    while (pit_get_ticks() < dl) {
        portsc = xhci_op_read32(hc, XHCI_PORTSC(port));
        if (portsc & XHCI_PORTSC_PRC) break;
    }
    if (!(portsc & XHCI_PORTSC_PRC)) {
        debug_log(DEBUG_LEVEL_ERROR, "USB", "Port %u reset timeout", port + 1);
        return -1;
    }
    
    /* Clear reset change */
    xhci_op_write32(hc, XHCI_PORTSC(port), (portsc & XHCI_PORTSC_PP) | XHCI_PORTSC_PRC | XHCI_PORTSC_CSC);
    debug_log(DEBUG_LEVEL_INFO, "USB", "Port %u: reset complete", port + 1);

    /* Re-read speed after reset (might change for some hubs/devices) */
    portsc = xhci_op_read32(hc, XHCI_PORTSC(port));
    speed = (portsc & XHCI_PORTSC_SPEED_MASK) >> XHCI_PORTSC_SPEED_SHIFT;
    
    if (!(portsc & XHCI_PORTSC_PED)) {
        debug_log(DEBUG_LEVEL_ERROR, "USB", "Port %u failed to enable", port + 1);
        return -1;
    }

    /* 2. ENABLE_SLOT Command */
    xhci_trb_t cmd_trb = {0};
    cmd_trb.control = TRB_TYPE_FIELD(TRB_TYPE_ENABLE_SLOT);
    
    uint64_t cmd_phys = hc->cmd_ring_phys + (hc->cmd_enqueue * sizeof(xhci_trb_t));
    if (xhci_command_ring_push(hc, &cmd_trb) < 0) return -1;
    
    uint8_t slot_id = 0, cc = 0;
    if (xhci_wait_command(hc, cmd_phys, &slot_id, &cc) < 0 || cc != TRB_CC_SUCCESS) {
        debug_log(DEBUG_LEVEL_ERROR, "USB", "ENABLE_SLOT failed (CC=%u)", cc);
        return -1;
    }
    
    debug_log(DEBUG_LEVEL_INFO, "USB", "USB: ENABLE_SLOT → slot_id=%u", slot_id);

    /* 3. Allocate tracking structures */
    usb_device_t *dev = kmalloc(sizeof(usb_device_t));
    if (!dev) return -1;
    memset(dev, 0, sizeof(usb_device_t));
    dev->slot_id = slot_id;
    dev->port = port + 1; /* 1-based */
    dev->speed = speed;
    usb_devices[slot_id] = dev;

    /* 4. Allocate Contexts */
    uint64_t out_ctx_phys, in_ctx_phys;
    dev->dev_ctx_virt = dma_alloc(4096, &out_ctx_phys);    /* 2048 bytes needed for 64-byte ctx, we allocate 1 page */
    dev->input_ctx_virt = dma_alloc(4096, &in_ctx_phys);
    dev->dev_ctx_phys = out_ctx_phys;
    dev->input_ctx_phys = in_ctx_phys;
    
    /* Assign output context to DCBAA */
    hc->dcbaa[slot_id] = dev->dev_ctx_phys;

    /* 5. Set up Input Context (Slot + EP0) */
    xhci_input_ctx_t *in_ctx = (xhci_input_ctx_t *)dev->input_ctx_virt;
    /* Add Context flags: Slot(bit 0) and EP0(bit 1) */
    in_ctx->ctrl.add_flags = (1 << 0) | (1 << 1);

    /* Setup Slot Context */
    in_ctx->slot.dw0 = SLOT_CTX_SPEED(speed) | SLOT_CTX_CTX_ENTRIES(1);
    in_ctx->slot.dw1 = SLOT_CTX_ROOT_PORT(dev->port);

    /* Setup EP0 Context */
    uint16_t max_packet = (speed == XHCI_SPEED_SS) ? 512 : (speed == XHCI_SPEED_LS ? 8 : 64);
    
    in_ctx->ep[0].dw1 = EP_CTX_EP_TYPE(EP_TYPE_CTRL) | EP_CTX_CERR(3) | EP_CTX_MPS(max_packet);
    
    /* Allocate EP0 Transfer Ring */
    dev->endpoints[0].ep_type = USB_EP_TYPE_CTRL;
    dev->endpoints[0].max_packet = max_packet;
    dev->endpoints[0].xhci_ep_id = 1; /* EP0 is index 1 in xHCI arrays */
    dev->endpoints[0].xfer_cycle = 1;
    
    uint64_t xfer_phys;
    dev->endpoints[0].xfer_ring_virt = dma_alloc(XHCI_XFER_RING_SIZE * 16, &xfer_phys);
    dev->endpoints[0].xfer_ring_phys = xfer_phys;
    
    in_ctx->ep[0].tr_dequeue_ptr = xfer_phys | 1; /* DCS=1 */

    /* 6. ADDRESS_DEVICE Command */
    memset(&cmd_trb, 0, sizeof(xhci_trb_t));
    cmd_trb.parameter = dev->input_ctx_phys;
    cmd_trb.control = TRB_TYPE_FIELD(TRB_TYPE_ADDRESS_DEVICE) | TRB_SET_SLOT(slot_id);
    
    cmd_phys = hc->cmd_ring_phys + (hc->cmd_enqueue * sizeof(xhci_trb_t));
    xhci_command_ring_push(hc, &cmd_trb);
    if (xhci_wait_command(hc, cmd_phys, NULL, &cc) < 0 || cc != TRB_CC_SUCCESS) {
        debug_log(DEBUG_LEVEL_ERROR, "USB", "ADDRESS_DEVICE failed (CC=%u)", cc);
        return -1;
    }
    
    dev->state = USB_DEV_STATE_ADDRESSED;
    debug_log(DEBUG_LEVEL_INFO, "USB", "USB: ADDRESS_DEVICE → CC=1 (Success)");

    /* 7. GET_DESCRIPTOR (Device) */
    uint64_t desc_phys;
    void *desc_buf = dma_alloc(4096, &desc_phys);
    
    usb_setup_t setup = {
        .bmRequestType = USB_REQTYPE_DEV_TO_HOST | USB_REQTYPE_STANDARD | USB_REQTYPE_DEVICE,
        .bRequest = USB_REQ_GET_DESCRIPTOR,
        .wValue = (USB_DESC_TYPE_DEVICE << 8) | 0,
        .wIndex = 0,
        .wLength = sizeof(usb_dev_desc_t)
    };

    debug_log(DEBUG_LEVEL_INFO, "USB", "USB: GET_DESCRIPTOR Device (%lu bytes)", sizeof(usb_dev_desc_t));
    if (usb_control_transfer(hc, dev, &setup, desc_phys, sizeof(usb_dev_desc_t), 1) == 0) {
        memcpy(&dev->dev_desc, desc_buf, sizeof(usb_dev_desc_t));
        dev->class_code = dev->dev_desc.bDeviceClass;
        dev->subclass_code = dev->dev_desc.bDeviceSubClass;
        dev->protocol_code = dev->dev_desc.bDeviceProtocol;
        debug_log(DEBUG_LEVEL_INFO, "USB", "USB: Device %04X:%04X (class=0x%02X)", 
                  dev->dev_desc.idVendor, dev->dev_desc.idProduct, dev->class_code);
    }

    /* 8. SET_CONFIGURATION (1) */
    setup.bmRequestType = USB_REQTYPE_HOST_TO_DEV | USB_REQTYPE_STANDARD | USB_REQTYPE_DEVICE;
    setup.bRequest = USB_REQ_SET_CONFIGURATION;
    setup.wValue = 1;
    setup.wIndex = 0;
    setup.wLength = 0;
    
    if (usb_control_transfer(hc, dev, &setup, 0, 0, 0) == 0) {
        dev->state = USB_DEV_STATE_CONFIGURED;
        debug_log(DEBUG_LEVEL_INFO, "USB", "USB: SET_CONFIGURATION(1) OK");
    }

    /* 9. Read Configuration & Interfaces to see if it's HID */
    setup.bmRequestType = USB_REQTYPE_DEV_TO_HOST | USB_REQTYPE_STANDARD | USB_REQTYPE_DEVICE;
    setup.bRequest = USB_REQ_GET_DESCRIPTOR;
    setup.wValue = (USB_DESC_TYPE_CONFIG << 8) | 0;
    setup.wIndex = 0;
    setup.wLength = 256; /* Arbitrary buffer size to capture config+interfaces+endpoints */
    
    if (usb_control_transfer(hc, dev, &setup, desc_phys, 256, 1) == 0) {
        usb_cfg_desc_t *cfg = (usb_cfg_desc_t *)desc_buf;
        uint8_t *ptr = (uint8_t *)desc_buf + cfg->bLength;
        uint8_t *end = (uint8_t *)desc_buf + cfg->wTotalLength;
        
        while (ptr < end) {
            uint8_t len = ptr[0];
            uint8_t type = ptr[1];
            
            if (type == USB_DESC_TYPE_INTERFACE) {
                usb_iface_desc_t *iface = (usb_iface_desc_t *)ptr;
                debug_log(DEBUG_LEVEL_INFO, "USB", "USB: Interface %u: class=%02X, subclass=%02X, protocol=%02X",
                          iface->bInterfaceNumber, iface->bInterfaceClass, 
                          iface->bInterfaceSubClass, iface->bInterfaceProtocol);
                          
                if (iface->bInterfaceClass == USB_CLASS_HID) {
                    /* It's a HID device, probe it */
                    usb_hid_probe(hc, dev);
                }
            }
            ptr += len;
        }
    }

    /* Free the temp descriptor buffer */
    pmm_free_pages(desc_phys, 1);
    
    return 0;
}
