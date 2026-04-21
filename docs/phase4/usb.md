# NexusOS Phase 4: USB Subsystem

## Overview
The NexusOS USB stack implements the xHCI (eXtensible Host Controller Interface) standard, providing support for SuperSpeed, High-Speed, Full-Speed, and Low-Speed devices. The implementation focuses strictly on the Phase 4 requirements: host controller initialization, device enumeration, and HID class support for keyboard/mouse.

## Architecture

### 1. Host Controller (`xhci.c`)
- **Initialization**: Validates BIOS handoff, resets the controller, and configures DCBAA (Device Context Base Address Array).
- **Rings**: Uses a simple, flat structure for the Command Ring, Event Ring, and Event Ring Segment Table (ERST) packed efficiently into DMA-safe, 4KB physically contiguous memory.
- **MMIO**: Strict adherence to `volatile` and `PAGE_NOCACHE` access rules.
- **Interrupts**: An IRQ handler (`xhci_irq_handler`) clears event status bits and delegates Event Ring processing.

### 2. Device Enumeration (`usb_device.c`)
The enumeration process uses a synchronous polling approach on EP0 for reliability during early boot.
- **Port Reset**: Issues Port Reset command to discover link speed.
- **ENABLE_SLOT**: Allocates an xHCI device slot.
- **ADDRESS_DEVICE**: Assigns the USB address and activates EP0 context.
- **GET_DESCRIPTOR (Device)**: Fetches the Standard Device Descriptor to reveal Class/Subclass/Protocol (or delays inference until interface read).
- **SET_CONFIGURATION (1)**: Activates the first valid configuration.
- **GET_DESCRIPTOR (Configuration)**: Reads the configuration, interface, and endpoint descriptors to dispatch driver probes (e.g., `usb_hid_probe`).

### 3. HID Driver (`usb_hid.c`)
- **Probe Strategy**: Examines interface subclasses. Subclass 1 / Protocol 1 triggers keyboard support.
- **Input System Translation**: Converts standard 8-byte HID keyboard reports to hardware-independent `input_event_t` structures (make/break + modifier tracking). Maps keycodes to ASCII via read-only table without occupying `.bss` space. 
- **Mouse Support**: Parses 3-4 byte relative movement/button reports and forwards them as `INPUT_EVENT_MOUSE_MOVE` or `INPUT_EVENT_MOUSE_BUTTON`.

## Constraints Addressed
- **BUG-003 Resolution**: All DMA buffers are explicitly allocated from the physical memory manager `pmm_alloc_pages()` checking for <4GB addresses. `vmm_get_phys()` on heap memory is strictly avoided.
- **Memory Efficiency**: Heavy structs like `usb_device_t` are instantiated via array pointers on an as-needed (lazy) basis rather than reserving static arrays.

## Testing Setup
```bash
make all lodbug
make run lodbug QEMU_FLAGS="-device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0"
```
