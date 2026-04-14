[bits 64]

section .text

extern debugger_main

; Global entry points for IDT
global debugger_trap_entry_int3
global debugger_trap_entry_int1
global debugger_trap_entry_int8

; Macro to save registers into the stack in the order of debugger_context_t
%macro SAVE_CONTEXT 0
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
%endmacro

; Macro to restore registers in reverse order
%macro RESTORE_CONTEXT 0
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
%endmacro

debugger_trap_entry_int3:
    push qword 0      ; Fake error code
    push qword 3      ; Interrupt number
    SAVE_CONTEXT
    
    mov rdi, rsp      ; First argument: pointer to context
    call debugger_main
    
    RESTORE_CONTEXT
    add rsp, 16       ; Clean up int_no and err_code
    iretq

debugger_trap_entry_int1:
    push qword 0      ; Fake error code
    push qword 1      ; Interrupt number
    SAVE_CONTEXT
    
    mov rdi, rsp      ; First argument: pointer to context
    call debugger_main
    
    RESTORE_CONTEXT
    add rsp, 16       ; Clean up int_no and err_code
    iretq

debugger_trap_entry_int8:
    ; #DF always pushes an error code (0)
    push qword 8      ; Interrupt number
    SAVE_CONTEXT
    
    mov rdi, rsp      ; First argument: pointer to context
    call debugger_main
    
    RESTORE_CONTEXT
    add rsp, 16       ; Clean up int_no and err_code
    iretq
