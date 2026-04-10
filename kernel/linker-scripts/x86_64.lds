/* ============================================================================
 * NexusOS — x86_64 Kernel Linker Script
 * Purpose: Link the kernel ELF at higher-half (0xFFFFFFFF80000000)
 * ============================================================================ */

/* Output format: x86_64 ELF64 */
OUTPUT_FORMAT(elf64-x86-64)

/* Kernel entry point */
ENTRY(kmain)

/* Program headers for Limine MMU permission control */
PHDRS
{
    limine_requests PT_LOAD;
    text    PT_LOAD;
    rodata  PT_LOAD;
    data    PT_LOAD;
}

SECTIONS
{
    /* Higher-half kernel: top 2 GiB of virtual address space */
    . = 0xffffffff80000000;

    /* Limine request structures — must be contiguous and discoverable */
    .limine_requests : {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    /* Executable code */
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .text : {
        *(.text .text.*)
    } :text

    /* Read-only data */
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .rodata : {
        *(.rodata .rodata.*)

        /* Shell command auto-registration table */
        . = ALIGN(8);
        __shell_commands_start = .;
        KEEP(*(.shell_commands))
        __shell_commands_end = .;
    } :rodata

    /* Build ID (optional, harmless to include) */
    .note.gnu.build-id : {
        *(.note.gnu.build-id)
    } :rodata

    /* Writable initialized data */
    . = ALIGN(CONSTANT(MAXPAGESIZE));
    .data : {
        *(.data .data.*)
    } :data

    /* Uninitialized data (BSS) — must be last in :data */
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    /* Export kernel end address for heap placement */
    _kernel_end = .;

    /* Discard unwanted sections */
    /DISCARD/ : {
        *(.eh_frame*)
        *(.note .note.*)
    }
}
