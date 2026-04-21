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

/* Get kernel load addresses (physical + virtual) */
struct limine_executable_address_response *limine_get_exec_addr(void);

/* Get kernel executable file response (ELF structure) */
struct limine_executable_file_response *limine_get_exec_file(void);

/* Get firmware type (BIOS vs UEFI) response */
struct limine_firmware_type_response *limine_get_firmware_type(void);

/* Get loaded modules (e.g. initrd, standalone files) */
struct limine_module_response *limine_get_modules(void);

#endif /* NEXUS_BOOT_LIMINE_REQUESTS_H */
