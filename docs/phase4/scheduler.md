# NexusOS Phase 4: Scheduler Implementation

## Components
- **nexus_config.h**: Kernel parameters (SCHED_QUANTUM_MS=10, max processes, stacks)
- **context.asm**: `switch_context` via stack-pushed callee-saved registers (rbx, rbp, r12-r15) + `thread_entry_stub`.
- **process.h/c**: Address space cloning (copies upper-half PML4), recursive PCB freeing, lazy FD table pointer, PID allocator.
- **thread.h/c**: TCB management, 4KB kernel stack pre-population according to `switch_context` ASM layout (`arg2`, `arg1`, `entry_rip`, `thread_entry_stub`, then 6 zeroed regs).
- **scheduler.h/c**: Preemptive multi-level round-robin scheduler. Tracks 8 priority queues. Replaces PIT IRQ0 to track quantum and call `schedule()`. Provides `scheduler_init` and `scheduler_start`. Test threads included.

## Status
- **Build**: 0 errors, 0 warnings.
- **Dependencies resolved**: `pit_tick_increment` exposed; `struct _xhci` properly patched.
- **Testing**: Test threads A and B implemented.
