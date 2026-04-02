; ============================================================================
; gdt_flush.asm — NexusOS GDT Flush
; Purpose: Load GDTR, reload segment registers, load TSS
; Author: NexusOS Kernel Team
; ============================================================================

section .text
global gdt_flush

; void gdt_flush(uint64_t gdtr_ptr)
; RDI = pointer to gdt_pointer_t struct
gdt_flush:
    ; Load the GDT register
    lgdt [rdi]

    ; Reload CS via a far return
    ; Push the new code segment selector and the return address
    push 0x08               ; GDT_KERNEL_CODE
    lea rax, [rel .reload_cs]
    push rax
    retfq                   ; Far return: pops RIP and CS

.reload_cs:
    ; Reload data segment registers with kernel data selector
    mov ax, 0x10            ; GDT_KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load the TSS segment selector
    mov ax, 0x28            ; GDT_TSS
    ltr ax

    ret
