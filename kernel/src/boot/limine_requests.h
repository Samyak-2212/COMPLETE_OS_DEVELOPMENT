/* ============================================================================
 * limine_requests.h — NexusOS Limine Request Accessor Declarations
 * Purpose: Provide access to Limine boot protocol responses
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#ifndef NEXUS_BOOT_LIMINE_REQUESTS_H
#define NEXUS_BOOT_LIMINE_REQUESTS_H

#include <stdbool.h>
#include <limine.h>

/* Check if the bootloader supports our base revision */
bool limine_base_revision_supported(void);

/* Get framebuffer response (address, dimensions, pitch, BPP) */
struct limine_framebuffer_response *limine_get_framebuffer(void);

/* Get physical memory map response */
struct limine_memmap_response *limine_get_memmap(void);

/* Get Higher Half Direct Map offset response */
struct limine_hhdm_response *limine_get_hhdm(void);

/* Get ACPI RSDP table pointer response */
struct limine_rsdp_response *limine_get_rsdp(void);

/* Get kernel executable load addresses response */
struct limine_executable_address_response *limine_get_exec_addr(void);

/* Get firmware type (BIOS vs UEFI) response */
struct limine_firmware_type_response *limine_get_firmware_type(void);


#endif /* NEXUS_BOOT_LIMINE_REQUESTS_H */
