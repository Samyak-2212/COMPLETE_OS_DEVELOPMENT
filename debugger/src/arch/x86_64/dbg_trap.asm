; ============================================================================
; dbg_trap.asm — NexusOS Debugger Trap Handlers
; Purpose: Exception handlers for INT1 (Debug) and INT3 (Breakpoint) that
;          save the full CPU context and enter the interactive debugger.
;
; The register save order matches registers_t from isr.h exactly:
;   r15 r14 r13 r12 r11 r10 r9 r8  rbp rdi rsi rdx rcx rbx rax
;   int_no  err_code
;   rip  cs  rflags  rsp  ss     (pushed by CPU)
;
; Author: NexusOS Debugger Team
; ============================================================================

section .text

; External C function — blocks until 'continue' command
extern dbg_console_enter

; ── INT1 (Debug Exception) ──────────────────────────────────────────────────
; Triggered by: single-step (TF=1), hardware breakpoints (DR0-DR3)
; No error code pushed by CPU.

global dbg_trap_int1
dbg_trap_int1:
    push 0                      ; Dummy error code
    push 1                      ; Interrupt number

    ; Save all general-purpose registers (order = registers_t)
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

    ; Clear the Trap Flag (TF) in saved RFLAGS so we don't re-trap on IRETQ.
    ; RFLAGS is at [rsp + 15*8 + 2*8 + 2*8] = [rsp + 136] from top of GPRs.
    ; Layout: 15 GPRs (120) + int_no(8) + err_code(8) + rip(8) + cs(8) + RFLAGS
    ; = rsp + 120 + 8 + 8 + 8 + 8 = rsp + 152
    mov rax, [rsp + 152]
    and rax, ~(1 << 8)          ; Clear TF (bit 8)
    mov [rsp + 152], rax

    ; Enter interactive debugger (blocking)
    call dbg_console_enter

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

    ; Remove int_no + err_code
    add rsp, 16

    iretq

; ── INT3 (Breakpoint) ───────────────────────────────────────────────────────
; Triggered by: INT3 instruction (opcode 0xCC), or hardware breakpoint.
; No error code pushed by CPU.

global dbg_trap_int3
dbg_trap_int3:
    push 0                      ; Dummy error code
    push 3                      ; Interrupt number

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

    ; Enter interactive debugger (blocking)
    call dbg_console_enter

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

    add rsp, 16

    iretq
