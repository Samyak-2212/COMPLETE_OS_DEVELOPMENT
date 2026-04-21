; crt0.asm — C Runtime Entry Point
; On process entry (via kernel exec):
;   RSP = user stack pointer
;   Stack layout: [argc] [argv[0]..argv[n] NULL] [envp[0]..envp[n] NULL]
;   (Note: some kernels pass argc in rdi, but we will pick it up from stack to be safe)

[bits 64]
section .text
global _start
extern main
extern _exit
extern __libc_start_main

_start:
    ; At entry: rsp = user stack top
    ; Stack: [argc] [argv pointers] [NULL] [envp pointers] [NULL]
    xor rbp, rbp            ; ABI: zero frame pointer at entry

    pop rdi                  ; argc
    mov rsi, rsp             ; argv (pointer to array of char*)
    lea rdx, [rsp + rdi*8 + 8] ; envp (after argv + NULL)

    ; Initialize libc (sets up malloc, errno, etc.)
    call __libc_start_main   ; (argc, argv, envp) → calls main()

    ; Should not reach here
    mov rdi, 0
    call _exit
    ud2
