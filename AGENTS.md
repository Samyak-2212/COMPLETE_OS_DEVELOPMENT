# NexusOS — Agent Onboarding

Before doing anything else, read the knowledge items in `knowledge_items/`.
They are the authoritative source of truth for this project.

## Required Reading (in order)

0. **Mandatory Skill**: [.agents/skills/nexusos-sync/SKILL.md](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/.agents/skills/nexusos-sync/SKILL.md) — Documentation synchronization workflow (Atomic DLC Protocol)
1. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md` — roles, boundaries, coding standards, behavior rules
2. `knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md` — stable kernel interfaces to code against
3. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md` — what is built and what is pending
4. `knowledge_items/nexusos-task-tracker/artifacts/task.md` — phase-by-phase task checklist
5. `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md` — open bugs that may affect your work
6. `knowledge_items/nexusos-implementation-plan/artifacts/implementation_plan.md` — full architecture spec (read only the sections relevant to your phase)
7. `knowledge_items/nexusos-shell-api/artifacts/SHELL_API.md` — instructions for shell command creation and automating `--help` documentation

## Optional Reading

- `knowledge_items/nexusos-debugger/artifacts/debugger.md` — serial debugger API (COM1)
- `knowledge_items/nexusos-history/artifacts/history.md` — git changelog and sync log
- `knowledge_items/nexusos-gitignore/artifacts/.gitignore` — what to exclude from version control

## Key Rules (summary)

- Do not modify files outside your agent role without Coordinator + user approval.
- Protected files (`kernel.c`, `hal/`, `mm/`, linker scripts) require Coordinator approval before any edit.
- Keep all conversational bug explanations to **15 words maximum**.
- No pleasantries. No padding. Be direct and precise.
- Every kernel struct that maps to hardware must use `__attribute__((packed))`.
- All MMIO access must use `volatile` pointers.
- Use `kmalloc`/`kfree` only — no standard library.
- Build must pass with **0 errors, 0 C warnings** before any commit.

## Filing Bugs

If you encounter an issue outside your scope or cannot resolve it in your session, log it in `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md`. Ask the user for permission before modifying that file.

---

## Phase 4 Subsystems (COMPLETE — for new agents joining at Phase 5)

The following subsystems are fully built and stable. **Do not modify them** without Coordinator + user approval.

### Scheduler & Multitasking (`kernel/src/sched/`)
- `process.h/c` — PCB: PID, credentials (uid/gid/euid/egid), VMA list, lazy FD table, COW address space
- `thread.h/c` — TCB: TID, kernel stack (1 page), priority, sleep timer
- `scheduler.h/c` — 8-level round-robin, IRQ0-driven (1ms quantum), idle thread, `schedule()` / `scheduler_block()` / `scheduler_wake()`
- `context.asm` — `switch_context(old_rsp, new_rsp)`: saves/restores callee-saved regs on kernel stack

### System Calls (`kernel/src/syscall/`)
- `syscall.c` — MSR init (EFER/STAR/LSTAR/FMASK), Linux x86_64 ABI
- `syscall_entry.asm` — SYSCALL/SYSRET entry: saves user state, calls C dispatcher, uses `default rel`
- `syscall_handlers.c` — 40+ handlers; stubs return `-ENOSYS`. Use `UNUSED_ARGS6` macro for stub params.
- `syscall_table.c` — dispatch table indexed by Linux syscall number
- `elf_loader.c` — ELF64 loader with lazy physical allocation (BSS demand-paged via #PF handler)
- `init_process.c` — creates PID 1 from Limine module, IRETQ → Ring 3

### Virtual Memory (`kernel/src/mm/vmm.c`)
- COW fork: `vmm_cow_clone()` marks parent pages read-only, `#PF` handler copies on write
- Demand paging: `#PF` handles stack growth, heap region, VMA-tracked BSS
- `vmm_destroy_address_space()` — COW-safe teardown (uses `pmm_page_unref`)

### USB (`kernel/src/drivers/usb/`)
- `xhci.c` — xHCI host controller: BIOS handoff, command/event rings, port enumeration
- `usb_device.c` — ENABLE_SLOT, ADDRESS_DEVICE, GET_DESCRIPTOR, SET_CONFIGURATION
- `usb_hid.c` — keyboard boot protocol, report → `input_event_t`
- `usb_hub.c` — stub (Phase 5 expansion)
- All DMA uses `pmm_alloc_page()` + HHDM (never heap pointers). Physical addresses verified < 4 GB.

### Userspace (`userspace/`)
- `libc/` — crt0.asm, syscall.asm wrappers, stdio (buffered), stdlib (brk-malloc), string, unistd, errno
- `init/` — PID 1: reads `/etc/nexus/init.conf`, spawns `nsh`
- `shell/` — userspace interactive shell with builtins

### Integration Point (`kernel/src/kernel.c` — Phase 4 init block)
```c
ata_init_subsystem();
ahci_init_subsystem();
xhci_init_subsystem();   /* must be BEFORE pci_init() */
pci_init();
/* ... */
scheduler_init();
syscall_init();
init_process_start();
scheduler_start();       /* never returns */
```

### Phase 4 Documentation
- `docs/phase4/scheduler.md` — scheduler internals, context switch, priority
- `docs/phase4/syscall.md` — syscall ABI, STAR MSR, handler table, ELF loader
- `docs/phase4/usb.md` — xHCI init, TRB rings, USB enumeration, HID
- `docs/phase4/userspace.md` — libc, init, shell architecture

### Phase 5 Starting Point
Next phase: ACPI (shutdown/reboot), APIC (replace PIC), physical hardware testing.
Read `knowledge_items/nexusos-task-tracker/artifacts/task.md` Phase 5 section.
Key deferred work from Phase 4:
- `execve` context replacement (overwrite saved RIP in syscall frame)
- `fork` child thread kernel stack frame setup
- Full `lseek`, `dup`, `pipe`, `stat`, signal delivery
