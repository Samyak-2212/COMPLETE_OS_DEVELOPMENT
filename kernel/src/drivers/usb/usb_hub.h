/* ============================================================================
 * usb_hub.h — NexusOS USB Hub Driver
 * Purpose: External USB Hub port state management
 * Author: NexusOS Driver Agent (Phase 4, Task 4.8)
 * ============================================================================ */

#ifndef USB_HUB_H
#define USB_HUB_H

#include "drivers/usb/usb_device.h"

int usb_hub_probe(xhci_t *hc, usb_device_t *dev);

#endif /* USB_HUB_H */
