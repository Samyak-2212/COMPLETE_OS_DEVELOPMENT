# NexusOS libc (libcnexus)

Minimal static C library for NexusOS userspace.

## Headers and Status
- `syscall.h`: NexusOS/Linux ABI syscall numbers.
- `unistd.h`: Basic `read`, `write`, `open`, `fork`, `execve`. (Implemented over __syscallN).
- `stdio.h`: Base `printf` family and basic `fopen`, `fread` implementations.
- `stdlib.h`: `malloc` (brk-based), `free`, `exit`, `atoi`, etc.
- `string.h`: Primary string and memory operations via simple iterative copy loops.
- `errno.h`: Global `errno` and POSIX values.
- `dirent.h`: Support for `getdents64` and `readdir`.

## Compatibility
`libc` headers attempt to follow `glibc` structures where relevant, although feature coverage is strictly minimal and geared toward `init` and `nsh`.

## Adding functions
Simply implement the function natively inside `src/`. Do not assume floating-point safety without confirming SSE/FPU state preservation in scheduler if needed.
