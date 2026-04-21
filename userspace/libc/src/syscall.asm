; syscall.asm — Low-level system call wrappers
; Convention: arguments in rdi, rsi, rdx, rcx, r8, r9
; syscall ABI: args in rdi, rsi, rdx, r10, r8, r9 (r10 not rcx!)
; Return: rax (>=0=success, <0=-errno)

[bits 64]
section .text

extern errno

%macro DEFSYSCALL 2
global %1
%1:
    mov rax, %2       ; syscall number
    mov r10, rcx      ; 4th arg: ABI uses rcx, syscall uses r10
    syscall
    ; If rax < 0, set errno = -rax, return -1 (except if rax bounds check)
    ; Usually kernel returns values between -1 and -4095 for errors
    cmp rax, -4095
    ja .error_%1
    ret
.error_%1:
    neg rax
    ; Store in errno
    lea rcx, [rel errno]
    mov [rcx], eax
    mov rax, -1
    ret
%endmacro

; Low level raw syscall dispatchers used by C code
global __syscall0
__syscall0:
    mov rax, rdi      ; syscall number
    syscall
    ret

global __syscall1
__syscall1:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    syscall
    ret

global __syscall2
__syscall2:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    mov rsi, rdx
    syscall
    ret

global __syscall3
__syscall3:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    syscall
    ret

global __syscall4
__syscall4:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    syscall
    ret

global __syscall5
__syscall5:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8,  r9
    syscall
    ret

global __syscall6
__syscall6:
    mov rax, rdi      ; syscall number
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, rcx
    mov r10, r8
    mov r8,  r9
    mov r9,  [rsp+8]  ; 7th arg is on stack
    syscall
    ret
