; syscall_entry.asm — System Call Entry/Exit
;
; Upon syscall instruction:
;   CPU saves RIP -> RCX, RFLAGS -> R11
;   CPU loads CS/SS from IA32_STAR
;   CPU jumps to IA32_LSTAR (here)
;   Interrupts masked by IA32_FMASK
;
; Our job:
;   Save user registers on kernel stack
;   Call C handler: syscall_dispatch(num, a1, a2, a3, a4, a5, a6)
;   Restore user registers
;   Execute sysretq
;
; Stack RSP management:
;   At entry RSP = user RSP.
;   We swap to kernel_rsp_scratch (updated by scheduler on every context switch).
;   Single-CPU Phase 4: one global scratch var is sufficient.
;
; WARNING: All .data symbol accesses MUST use [rel <sym>] to generate
; 64-bit RIP-relative addressing and avoid section-crossing relocation warnings.

[bits 64]
default rel          ; All memory references are RIP-relative by default

section .text

extern syscall_dispatch

global syscall_entry
global kernel_rsp_scratch

syscall_entry:
    ; Save user RSP, load kernel RSP from scheduler-managed scratch
    mov     [user_rsp_scratch], rsp
    mov     rsp, [kernel_rsp_scratch]

    ; Push user context frame on kernel stack
    push    rcx                         ; user RIP  (saved by CPU in RCX on syscall)
    push    r11                         ; user RFLAGS (saved by CPU in R11)
    push    rbp
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15
    push    qword [user_rsp_scratch]    ; user RSP

    ; Safely on kernel stack — re-enable interrupts
    sti

    ; Linux syscall ABI -> SysV C calling convention mapping:
    ;   RAX = syscall num -> RDI (1st C arg)
    ;   RDI = a1          -> RSI (2nd C arg)
    ;   RSI = a2          -> RDX (3rd C arg)
    ;   RDX = a3          -> RCX (4th C arg)
    ;   R10 = a4          -> R8  (5th C arg)
    ;   R8  = a5          -> R9  (6th C arg)
    ;   R9  = a6          -> [rsp] (7th C arg, pushed to align stack)
    ;
    ; After 9 pushes above, RSP is misaligned by 8 bytes.
    ; Pushing r9 (a6) brings us to 16-byte alignment for the C call.
    push    r9               ; a6 -> 7th argument (on stack for SysV)

    mov     r9,  r8          ; a5 -> R9
    mov     r8,  r10         ; a4 -> R8
    mov     rcx, rdx         ; a3 -> RCX
    mov     rdx, rsi         ; a2 -> RDX
    mov     rsi, rdi         ; a1 -> RSI
    mov     rdi, rax         ; num -> RDI

    call    syscall_dispatch  ; rax = return value

    add     rsp, 8           ; clean up pushed a6

    ; Disable interrupts before returning to user space
    cli

    ; Restore saved registers (reverse push order)
    pop     qword [user_rsp_scratch]   ; restore user RSP to scratch
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    pop     rbp
    pop     r11              ; user RFLAGS -> R11 (consumed by sysretq)
    pop     rcx              ; user RIP    -> RCX (consumed by sysretq)

    ; Switch back to user stack
    mov     rsp, [user_rsp_scratch]

    o64 sysret               ; return to user: CS/SS from STAR, RIP=RCX, RFLAGS=R11

section .data
align 8
user_rsp_scratch:   dq 0    ; per-CPU scratch: stores user RSP during syscall
kernel_rsp_scratch: dq 0    ; per-CPU KC RSP: updated by scheduler on every switch
