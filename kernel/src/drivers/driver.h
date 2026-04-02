/* ============================================================================
 * driver.h — NexusOS Device Driver Model
 * Purpose: Unified interface for all hardware device drivers
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_DRIVERS_DRIVER_H
#define NEXUS_DRIVERS_DRIVER_H

#include <stdint.h>

/* Forward declaration in case drivers need internal linking */
struct driver;

/* 
 * Generic driver structure that all hardware drivers must implement.
 * The PnP manager uses this to initialize and track hardware.
 */
typedef struct driver {
    const char *name;            /* Human-readable driver name */
    
    /* 
     * Probe: Check if this driver supports the given hardware device.
     * device_info depends on the bus (e.g., pci_device_t* for PCI).
     * Return 1 if supported, 0 if not.
     */
    int (*probe)(void *device_info);
    
    /* 
     * Init: Initialize the hardware device and register it with subsystems.
     * Return 0 on success, <0 on error.
     */
    int (*init)(void *device_info);
    
} driver_t;

/* ── Driver Manager API ─────────────────────────────────────────────────── */

/* 
 * Register a driver with the system.
 * Typically called during kernel initialization.
 */
void driver_register(driver_t *drv);

/* 
 * Unregister a driver.
 */
void driver_unregister(driver_t *drv);

/* 
 * Ask all registered drivers to probe a newly discovered device.
 * Returns the driver that accepted the device, or NULL if unsupported.
 */
driver_t *driver_probe_device(void *device_info);

#endif /* NEXUS_DRIVERS_DRIVER_H */
