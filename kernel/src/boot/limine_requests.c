/* ============================================================================
 * limine_requests.c — NexusOS Limine Boot Protocol Request Structures
 * Purpose: Define all Limine request structs in the .requests section
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

/* ── Section markers ─────────────────────────────────────────────────────── */

/* Start marker for Limine requests section */
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

/* End marker for Limine requests section */
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

/* ── Base revision ───────────────────────────────────────────────────────── */

/* Protocol version negotiation (revision 6 is latest) */
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

/* ── Framebuffer request ─────────────────────────────────────────────────── */

/* Get framebuffer address, dimensions, pitch, BPP */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

/* ── Memory map request ──────────────────────────────────────────────────── */

/* Physical memory map (usable, reserved, ACPI, bootloader) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

/* ── HHDM request ────────────────────────────────────────────────────────── */

/* Higher Half Direct Map offset (e.g., 0xFFFF800000000000) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

/* ── RSDP request ────────────────────────────────────────────────────────── */

/* ACPI RSDP table pointer */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

/* ── Executable address request ──────────────────────────────────────────── */

/* Get kernel load addresses (physical + virtual) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_address_request exec_addr_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0
};

/* ── Executable file request ─────────────────────────────────────────────── */

/* Get kernel binary file in memory (for symbols) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_executable_file_request exec_file_request = {
    .id = LIMINE_EXECUTABLE_FILE_REQUEST_ID,
    .revision = 0
};

/* ── Stack size request ──────────────────────────────────────────────────── */

/* Request larger initial stack (128 KiB) */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .stack_size = 131072  /* 128 KiB */
};

/* ── Firmware type request ───────────────────────────────────────────────── */

/* Detect BIOS vs UEFI boot */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_firmware_type_request firmware_type_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID,
    .revision = 0
};
/* ── Module request ────────────────────────────────────────────────────────── */

/* Get modules loaded by bootloader */
__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST_ID,
    .revision = 1
};

/* ── Accessor functions ─────────────────────────────────────────────────── */
/* These allow other translation units to access the responses. */

bool limine_base_revision_supported(void) {
    return LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision);
}

struct limine_framebuffer_response *limine_get_framebuffer(void) {
    return framebuffer_request.response;
}

struct limine_memmap_response *limine_get_memmap(void) {
    return memmap_request.response;
}

struct limine_hhdm_response *limine_get_hhdm(void) {
    return hhdm_request.response;
}

struct limine_rsdp_response *limine_get_rsdp(void) {
    return rsdp_request.response;
}

struct limine_executable_address_response *limine_get_exec_addr(void) {
    return exec_addr_request.response;
}

struct limine_executable_file_response *limine_get_exec_file(void) {
    return exec_file_request.response;
}

struct limine_firmware_type_response *limine_get_firmware_type(void) {
    return firmware_type_request.response;
}
struct limine_module_response *limine_get_modules(void) {
    return module_request.response;
}
