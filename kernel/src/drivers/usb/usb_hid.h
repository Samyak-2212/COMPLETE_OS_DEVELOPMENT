/* ============================================================================
 * usb_hid.h — NexusOS USB HID Class Driver API
 * Purpose: HID report parsing and keyboard/mouse to input_event_t translation
 * Author: NexusOS Driver Agent (Phase 4, Task 4.9)
 * ============================================================================ */

#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>
#include "drivers/usb/usb_device.h"

/* Check if the device is a supported HID device, and if so, initialize it.
 * Returns 0 on success, negative if unsupported or error. */
int usb_hid_probe(xhci_t *hc, usb_device_t *dev);

/* Process a received HID report on the interrupt IN endpoint */
void usb_hid_process_report(usb_device_t *dev, uint8_t *report, uint32_t len);

#endif /* USB_HID_H */
