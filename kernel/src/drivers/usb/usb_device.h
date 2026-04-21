/* ============================================================================
 * usb_device.h — NexusOS USB Device Descriptors and Enumeration API
 * Purpose: Standard USB descriptor structs and device management
 * Reference: USB 2.0 Specification §9
 * Author: NexusOS Driver Agent (Phase 4, Task 4.8)
 * ============================================================================ */

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdint.h>
#include "drivers/usb/xhci.h"

/* ── Standard USB request codes ─────────────────────────────────────────── */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

/* ── bmRequestType values ────────────────────────────────────────────────── */
#define USB_REQTYPE_HOST_TO_DEV     0x00   /* Direction: host→device */
#define USB_REQTYPE_DEV_TO_HOST     0x80   /* Direction: device→host */
#define USB_REQTYPE_STANDARD        0x00
#define USB_REQTYPE_CLASS           0x20
#define USB_REQTYPE_VENDOR          0x40
#define USB_REQTYPE_DEVICE          0x00
#define USB_REQTYPE_INTERFACE       0x01
#define USB_REQTYPE_ENDPOINT        0x02

/* ── Descriptor Types (wValue high byte for GET_DESCRIPTOR) ─────────────── */
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIG        0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_REPORT        0x22

/* ── USB Class Codes ─────────────────────────────────────────────────────── */
#define USB_CLASS_HID               0x03
#define USB_CLASS_MASS_STORAGE      0x08
#define USB_CLASS_HUB               0x09

/* ── HID Subclass / Protocol ─────────────────────────────────────────────── */
#define USB_HID_SUBCLASS_BOOT       0x01
#define USB_HID_PROTOCOL_KEYBOARD   0x01
#define USB_HID_PROTOCOL_MOUSE      0x02

/* ── Standard USB Setup Packet (8 bytes) ─────────────────────────────────── */
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_setup_t;

/* ── USB Device Descriptor (18 bytes) ───────────────────────────────────── */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} __attribute__((packed)) usb_dev_desc_t;

/* ── USB Configuration Descriptor (9 bytes) ─────────────────────────────── */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} __attribute__((packed)) usb_cfg_desc_t;

/* ── USB Interface Descriptor (9 bytes) ─────────────────────────────────── */
typedef struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_iface_desc_t;

/* ── USB Endpoint Descriptor (7 bytes) ──────────────────────────────────── */
typedef struct {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;  /* bit7: 1=IN, 0=OUT; bits[3:0]: EP number */
    uint8_t  bmAttributes;      /* bits[1:0]: 0=ctrl, 1=isoch, 2=bulk, 3=intr */
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;         /* polling interval in frames/ms */
} __attribute__((packed)) usb_ep_desc_t;

/* Endpoint address direction bit */
#define USB_EP_DIR_IN   (1u << 7)

/* Endpoint transfer type */
#define USB_EP_TYPE_CTRL    0
#define USB_EP_TYPE_ISOCH   1
#define USB_EP_TYPE_BULK    2
#define USB_EP_TYPE_INTR    3

/* ── USB Device State ────────────────────────────────────────────────────── */

#define USB_DEV_STATE_DEFAULT       0
#define USB_DEV_STATE_ADDRESSED     1
#define USB_DEV_STATE_CONFIGURED    2

/* ── USB Endpoint (per-device, one per active endpoint) ─────────────────── */

typedef struct {
    uint8_t  ep_addr;           /* bEndpointAddress */
    uint8_t  ep_type;           /* USB_EP_TYPE_* */
    uint16_t max_packet;        /* wMaxPacketSize */
    uint8_t  interval;          /* polling interval */
    /* DMA transfer ring for this endpoint */
    uint64_t xfer_ring_phys;    /* Physical address */
    void    *xfer_ring_virt;    /* Virtual address */
    uint32_t xfer_enqueue;      /* Producer index */
    uint8_t  xfer_cycle;        /* Producer cycle bit */
    uint8_t  xhci_ep_id;        /* xHCI endpoint ID (2*ep_num + dir) */
} usb_endpoint_t;

/* ── USB Device Descriptor ───────────────────────────────────────────────── */

#define USB_MAX_ENDPOINTS   4   /* We only need EP0 + 1 interrupt for HID */

typedef struct usb_device {
    uint8_t  slot_id;           /* xHCI slot ID (1-based) */
    uint8_t  port;              /* Root hub port index (0-based) */
    uint8_t  speed;             /* XHCI_SPEED_FS/LS/HS/SS */
    uint8_t  address;           /* USB device address (after SET_ADDRESS) */
    uint8_t  state;             /* USB_DEV_STATE_* */

    /* Descriptor info populated during enumeration */
    usb_dev_desc_t  dev_desc;
    uint8_t  class_code;
    uint8_t  subclass_code;
    uint8_t  protocol_code;

    /* Per-device DMA input context */
    void    *input_ctx_virt;    /* Virtual */
    uint64_t input_ctx_phys;    /* Physical */

    /* Per-device DMA output (device) context, pointed to from DCBAA */
    void    *dev_ctx_virt;      /* Virtual */
    uint64_t dev_ctx_phys;      /* Physical */

    /* Endpoint table */
    usb_endpoint_t endpoints[USB_MAX_ENDPOINTS];
    uint8_t  num_endpoints;

    /* HID driver private data (NULL if not HID) */
    void    *hid_priv;
} usb_device_t;

/* ── Enumeration API ─────────────────────────────────────────────────────── */

/* Enumerate a newly connected device on the given port.
 * Returns 0 on success, negative on failure. */
int usb_enumerate_device(xhci_t *hc, uint8_t port, uint8_t speed);

/* Issue a control transfer (Setup + optional Data + Status).
 * setup: 8-byte setup packet (physical DMA buffer address on entry)
 * data_phys: physical address of data buffer (0 if no data stage)
 * data_len: length in bytes (0 if no data stage)
 * dir_in: 1 if data flows device→host, 0 if host→device
 * Returns 0 on success, negative on error. */
int usb_control_transfer(xhci_t *hc, usb_device_t *dev,
                         usb_setup_t *setup,
                         uint64_t data_phys, uint16_t data_len, int dir_in);

#endif /* USB_DEVICE_H */
