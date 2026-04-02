; ============================================================================
; isr_stubs.asm — NexusOS ISR / IRQ Assembly Stubs
; Purpose: Push error code (or dummy), push ISR number, save all GPRs,
;          call C handler (isr_handler), restore GPRs, iretq
; Author: NexusOS Kernel Team
;
; CRITICAL x64 NOTES:
;   - Uses IRETQ (not IRET) for 64-bit mode
;   - 16-byte stack alignment maintained before CALL
;   - All 15 GPRs saved/restored (excluding RSP which is handled by iretq)
; ============================================================================

section .text

; External C handler function
extern isr_handler

; ── Macro: ISR stub WITHOUT error code (CPU doesn't push one) ────────────
; Push a dummy 0 error code, then the interrupt number.
%macro ISR_NOERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push 0                  ; Dummy error code
    push %1                 ; Interrupt number
    jmp isr_common_stub
%endmacro

; ── Macro: ISR stub WITH error code (CPU pushes it automatically) ────────
; Only push the interrupt number; error code is already on stack.
%macro ISR_ERRCODE 1
global isr_stub_%1
isr_stub_%1:
    push %1                 ; Interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; ── CPU Exceptions (ISR 0-31) ────────────────────────────────────────────
; Exceptions that push an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
ISR_NOERRCODE 0         ; #DE  Divide Error
ISR_NOERRCODE 1         ; #DB  Debug
ISR_NOERRCODE 2         ; #NMI Non-Maskable Interrupt
ISR_NOERRCODE 3         ; #BP  Breakpoint
ISR_NOERRCODE 4         ; #OF  Overflow
ISR_NOERRCODE 5         ; #BR  Bound Range Exceeded
ISR_NOERRCODE 6         ; #UD  Invalid Opcode
ISR_NOERRCODE 7         ; #NM  Device Not Available
ISR_ERRCODE   8         ; #DF  Double Fault
ISR_NOERRCODE 9         ;      Coprocessor Segment Overrun
ISR_ERRCODE   10        ; #TS  Invalid TSS
ISR_ERRCODE   11        ; #NP  Segment Not Present
ISR_ERRCODE   12        ; #SS  Stack-Segment Fault
ISR_ERRCODE   13        ; #GP  General Protection Fault
ISR_ERRCODE   14        ; #PF  Page Fault
ISR_NOERRCODE 15        ;      Reserved
ISR_NOERRCODE 16        ; #MF  x87 Floating-Point Exception
ISR_ERRCODE   17        ; #AC  Alignment Check
ISR_NOERRCODE 18        ; #MC  Machine Check
ISR_NOERRCODE 19        ; #XM  SIMD Floating-Point Exception
ISR_NOERRCODE 20        ; #VE  Virtualization Exception
ISR_ERRCODE   21        ; #CP  Control Protection Exception
ISR_NOERRCODE 22        ;      Reserved
ISR_NOERRCODE 23        ;      Reserved
ISR_NOERRCODE 24        ;      Reserved
ISR_NOERRCODE 25        ;      Reserved
ISR_NOERRCODE 26        ;      Reserved
ISR_NOERRCODE 27        ;      Reserved
ISR_NOERRCODE 28        ;      Reserved
ISR_ERRCODE   29        ; #HV  Hypervisor Injection Exception
ISR_ERRCODE   30        ; #VC  VMM Communication Exception
ISR_NOERRCODE 31        ;      Reserved

; ── IRQ stubs (mapped to INT 32-47 after PIC remap) ─────────────────────
ISR_NOERRCODE 32        ; IRQ0  - PIT Timer
ISR_NOERRCODE 33        ; IRQ1  - PS/2 Keyboard
ISR_NOERRCODE 34        ; IRQ2  - Cascade
ISR_NOERRCODE 35        ; IRQ3  - COM2
ISR_NOERRCODE 36        ; IRQ4  - COM1
ISR_NOERRCODE 37        ; IRQ5  - LPT2
ISR_NOERRCODE 38        ; IRQ6  - Floppy
ISR_NOERRCODE 39        ; IRQ7  - LPT1 / Spurious
ISR_NOERRCODE 40        ; IRQ8  - RTC
ISR_NOERRCODE 41        ; IRQ9  - Free
ISR_NOERRCODE 42        ; IRQ10 - Free
ISR_NOERRCODE 43        ; IRQ11 - Free
ISR_NOERRCODE 44        ; IRQ12 - PS/2 Mouse
ISR_NOERRCODE 45        ; IRQ13 - FPU
ISR_NOERRCODE 46        ; IRQ14 - Primary ATA
ISR_NOERRCODE 47        ; IRQ15 - Secondary ATA

; ── Common ISR stub ─────────────────────────────────────────────────────
; At this point the stack looks like:
;   [SS] [RSP] [RFLAGS] [CS] [RIP] [error_code] [int_no]  ← top
;
; We need to save all general-purpose registers so the C handler gets
; a complete registers_t* struct.
isr_common_stub:
    ; Save all general-purpose registers (order must match registers_t)
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to registers_t as first argument (RDI in SysV ABI)
    mov rdi, rsp

    ; Ensure 16-byte alignment before call.
    ; We pushed 15 GPRs (120 bytes) + int_no (8) + err_code (8) = 136 bytes
    ; CPU pushed 5 values (40 bytes). Total = 176 bytes = 16-byte aligned.
    ; So we're already aligned. Call the C handler.
    call isr_handler

    ; Restore all GPRs
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove int_no and err_code from stack
    add rsp, 16

    ; Return from interrupt (64-bit)
    iretq
