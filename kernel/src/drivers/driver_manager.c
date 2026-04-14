/* ============================================================================
 * driver_manager.c — NexusOS Device Driver Manager
 * Purpose: Registry of loaded drivers and PnP probe dispatching
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "drivers/driver.h"
#include "lib/printf.h"
#include "lib/spinlock.h"

#define MAX_DRIVERS 64

static driver_t *registered_drivers[MAX_DRIVERS];
static int num_drivers = 0;
static spinlock_t driver_lock = SPINLOCK_INIT;

void driver_register(driver_t *drv) {
    if (!drv) return;
    
    spinlock_acquire(&driver_lock);

    if (num_drivers >= MAX_DRIVERS) {
        spinlock_release(&driver_lock);
        kprintf_set_color(0x00FF4444, FB_DEFAULT_BG);
        kprintf("[FAIL] Driver manager: Maximum drivers reached, cannot register '%s'\n", drv->name);
        return;
    }
    
    registered_drivers[num_drivers++] = drv;
    
    spinlock_release(&driver_lock);
    /* Debug note temporarily disabled because it gets noisy but good for tracking */
    /* kprintf("Driver Registered: %s\n", drv->name); */
}

void driver_unregister(driver_t *drv) {
    if (!drv) return;
    
    spinlock_acquire(&driver_lock);

    for (int i = 0; i < num_drivers; i++) {
        if (registered_drivers[i] == drv) {
            /* Shift array down */
            for (int j = i; j < num_drivers - 1; j++) {
                registered_drivers[j] = registered_drivers[j+1];
            }
            num_drivers--;
            spinlock_release(&driver_lock);
            return;
        }
    }

    spinlock_release(&driver_lock);
}

driver_t *driver_probe_device(void *device_info) {
    if (!device_info) return (void *)0;

    spinlock_acquire(&driver_lock);

    for (int i = 0; i < num_drivers; i++) {
        driver_t *drv = registered_drivers[i];
        if (drv->probe && drv->probe(device_info)) {
            /* Hand over to init */
            if (drv->init && drv->init(device_info) == 0) {
                spinlock_release(&driver_lock);
                return drv; /* Successfully claimed and initialized */
            }
        }
    }
    
    spinlock_release(&driver_lock);
    return (void *)0; /* No driver claimed this device */
}
