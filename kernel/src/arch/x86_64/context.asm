; ============================================================================
; context.asm — NexusOS CPU Context Switch
; Purpose: switch_context(old_rsp_ptr, new_rsp) — saves callee-saved registers
;          to the old thread's kernel stack, restores from new thread's stack.
;          thread_entry_stub — entry point for brand-new threads.
; Author: NexusOS Kernel Team
; ============================================================================

section .text

; ── switch_context ────────────────────────────────────────────────────────────
; void switch_context(uint64_t *old_rsp_out, uint64_t new_rsp)
;   rdi = pointer to old thread's kstack_rsp field  (OUT: save current RSP here)
;   rsi = new thread's saved RSP                    (IN: load into RSP)
;
; Callee-saved registers (System V AMD64 ABI): rbx, rbp, r12-r15
; The return address is already on the stack from the CALL instruction.
global switch_context
switch_context:
    ; Push callee-saved registers onto the CURRENT (old) kernel stack
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    ; Save current RSP into old_thread->kstack_rsp
    mov     [rdi], rsp

    ; Load new thread's kernel stack pointer
    mov     rsp, rsi

    ; Restore callee-saved registers from the NEW kernel stack
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx

    ; Return to the new thread's continuation point.
    ; For an existing thread: this returns to wherever switch_context was called.
    ; For a brand-new thread: this jumps to thread_entry_stub (pre-pushed as "rip").
    ret

; ── thread_entry_stub ─────────────────────────────────────────────────────────
; Called (via ret) when a brand-new thread is first scheduled.
; Stack layout at entry (pushed by thread_create in thread.c):
;   [rsp+0]  = entry_fn  pointer   ← popped into rdi
;   [rsp+8]  = arg1                 ← popped into rsi
;   [rsp+16] = arg2                 ← popped into rdx
;
; After popping, calls entry_fn(arg1, arg2).
; If entry_fn returns, calls process_exit_current(0) to clean up.
global thread_entry_stub
extern process_exit_current
extern sched_lock

thread_entry_stub:
    ; A new thread just started. Release the scheduler lock so it can be preempted.
    mov     dword [rel sched_lock], 0

    ; Enable interrupts so the thread can be preempted and won't lock up on 'hlt'
    sti

    ; Retrieve the three values that thread_create pushed above the saved regs
    pop     rdi     ; entry_fn pointer
    pop     rsi     ; arg1
    pop     rdx     ; arg2

    ; Call the thread's entry function: entry_fn(arg1, arg2)
    ; rdi already holds the fn ptr; shift args: rdi=arg1, rsi=arg2
    mov     rax, rdi    ; save entry_fn
    mov     rdi, rsi    ; arg1 -> rdi
    mov     rsi, rdx    ; arg2 -> rsi
    call    rax

    ; Thread returned — exit gracefully with code 0
    xor     rdi, rdi
    call    process_exit_current

    ; Should never reach here
    ud2
