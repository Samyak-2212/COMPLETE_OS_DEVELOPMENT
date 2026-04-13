# NexusOS — Multi-Agent Coordination Protocol

> **Version**: 1.0 | **Last Updated**: 2026-03-29
> **Authority**: Senior Systems Architect (Coordinator Agent)

---

## 1. Agent Roles

| Role | Scope | Can Modify |
|---|---|---|
| **Coordinator** | Architecture, planning, review, error flagging | Knowledge items (with user permission), `docs/`, `implementation_plan.md` |
| **Kernel Agent** | Kernel subsystems (Phases 1-5) | `kernel/src/`, `kernel/GNUmakefile`, linker scripts |
| **GUI Agent** | Desktop Environment (Phase 6) | `userspace/desktop_env/`, entirely independent of the kernel |
| **Driver Agent** | New hardware drivers | `kernel/src/drivers/<new_subsystem>/` |
| **App Agent** | Userspace applications | `userspace/apps/`, `userspace/libc/` |
| **Overhaul Agent** | Major refactors across all subsystems | Any file (requires user approval per change area) |

---

## 2. Extension Points

These are the **stable interfaces** that agents must code against. Breaking these requires Coordinator review + user approval.

### 2.1 Adding a New Driver

1. Create `kernel/src/drivers/<category>/<driver_name>.h` and `.c`
2. Implement `driver_t` interface (see `kernel/src/drivers/driver.h`):
   ```c
   driver_t my_driver = {
       .name = "my_device",
       .type = DRIVER_TYPE_xxx,
       .init = my_init,
       .deinit = my_deinit,
       .probe = my_probe,
       .irq_handler = my_irq
   };
   ```
3. Register in `driver_manager.c`: add `driver_register(&my_driver)` call
4. The GNUmakefile auto-discovers `*.c` and `*.asm` files — no Makefile edit needed
5. Add the IRQ handler registration in `idt.c` if needed

### 2.2 Adding a New Filesystem

1. Create `kernel/src/fs/<fsname>.h` and `.c`
2. Implement VFS operations (see `kernel/src/fs/vfs.h`):
   ```c
   static vfs_ops_t myfs_ops = {
       .open = myfs_open,
       .read = myfs_read,
       .write = myfs_write,
       .close = myfs_close,
       .readdir = myfs_readdir,
       .mkdir = myfs_mkdir,
       .unlink = myfs_unlink,
       .stat = myfs_stat,
   };
   ```
3. Register: `vfs_register_fs("myfs", &myfs_ops)`
4. Add partition type detection in `partition.c` if on-disk

### 2.3 Adding a Shell Command
1. Create `kernel/src/shell/cmds/<category>/cmd_<name>.c`
2. Implement your handler and register via the `REGISTER_SHELL_COMMAND` macro:
   ```c
   #include "shell/shell_command.h"

   static int cmd_mycmd(int argc, char **argv) {
       kprintf("Hello!\n");
       return 0;
   }

   REGISTER_SHELL_COMMAND(mycmd, "[args]", "[opts]", "Description", "Full help", "system", cmd_mycmd);
   ```
3. The linker automatically collects and registers commands in the `.shell_commands` section.

### 2.4 Creating a Desktop Environment / GUI (Phase 6)

The architectural mandate is absolute: **The GUI must be fully decoupled from the Kernel.**

Any GUI Agent designing a window manager or desktop environment must:
1. Run their system exclusively in **Userspace (Ring 3)**.
2. Interface with hardware exclusively via VFS and System Calls:
   - Use `open("/dev/fb0")` and `SYS_MMAP` to request direct access to the display buffer memory.
   - Use `open("/dev/input/event0")` and `SYS_READ` to receive uninterpreted keyboard and mouse events.
3. Multiple Desktop Environments (made by different agents) must be able to run sequentially or concurrently on the exact same Kernel simply by swapping the executed userspace binaries.
4. Notify the kernel to suspend the internal VT100 fallback terminal using `SYS_IOCTL`.

### 2.5 Adding a Userspace Program

1. Create `userspace/<program>.c`
2. Link against `userspace/libc/` (minimal C library)
3. Entry point: `int main(int argc, char **argv)`
4. Add to Makefile: gets compiled as flat binary or ELF loaded by `exec()` syscall

### 2.6 Adding System Calls

1. Add `SYS_xxx` constant to `kernel/src/syscall/syscall.h`
2. Implement handler in `kernel/src/syscall/syscall_table.c`
3. Add userspace wrapper in `userspace/libc/syscall.h` + `syscall.asm`

---

## 3. File Ownership & Boundaries

> [!CAUTION]
> Agents MUST NOT modify files outside their scope without Coordinator + user approval.

### 3.1 Protected Files (require Coordinator approval)

| File | Reason |
|---|---|
| `kernel/src/kernel.c` | Init sequence order affects all subsystems |
| `kernel/src/hal/*` | GDT/IDT/PIC are foundational — changes can triple-fault |
| `kernel/src/mm/*` | Memory manager changes affect all allocations |
| `kernel/GNUmakefile` | Build flag changes affect everything |
| `kernel/linker-scripts/x86_64.lds` | Section layout changes can break boot |
| `kernel/src/boot/limine_requests.*` | Boot protocol is fixed by Limine spec |

### 3.2 Freely Modifiable (within agent's role)

| Agent | Can Freely Create/Modify |
|---|---|
| Driver Agent | `kernel/src/drivers/<new>/` |
| GUI Agent | `userspace/desktop_env/*` |
| App Agent | `userspace/apps/*` |

### 3.3 Shared Files (coordinate with Coordinator)

| File | Who Modifies | Coordination Required |
|---|---|---|
| `kernel/src/drivers/driver_manager.c` | Driver Agent | Register new driver — needs Coordinator review |
| `kernel/src/fs/vfs.c` | FS/Kernel Agent | Register new FS — needs Coordinator review |
| `kernel/src/display/terminal_shell.c` | Any agent adding commands | Append to command table |
| `kernel/src/hal/idt.c` | Driver Agent | Register new IRQ — needs Coordinator review |
| `kernel/src/syscall/syscall_table.c` | Kernel/App Agent | New syscalls — needs Coordinator review |

---

## 4. Knowledge Item Management

### 4.1 Global Knowledge Items (in `~/.gemini/antigravity/knowledge/`)

| KI | Purpose | Owner |
|---|---|---|
| `nexusos-implementation-plan` | Full architecture spec | Coordinator |
| `nexusos-progress-report` | What's built, what's pending | Coordinator |
| `nexusos-task-tracker` | Phase-by-phase task checklist | Coordinator |
| `nexusos-agent-protocol` | This document — multi-agent rules | Coordinator |
| `nexusos-kernel-api` | Stable API reference for all agents | Coordinator |
| `nexusos-bug-pool` | Universal bug tracker for cross-agent issues | All Agents |

### 4.2 Rules for Knowledge Updates

1. **Read**: Any agent may read any knowledge item at any time
2. **Local copies**: Each agent may maintain local working copies in their conversation artifacts
3. **Global updates**: To update a global knowledge item, an agent must:
   - Propose the change to the user
   - Receive explicit user permission
   - Make the update
   - Note the change in the progress report
4. **Conflict resolution**: If two agents' changes conflict:
   - The Coordinator reviews both
   - User makes final decision
   - Losing agent adapts their code

### 4.3 What Each Agent Should Check on Startup

Every agent working on NexusOS should:
1. Read `nexusos-agent-protocol` (this document)
2. Read `nexusos-kernel-api` (stable interfaces)
3. Read `nexusos-progress-report` (what exists, what's pending)
4. Read `nexusos-task-tracker` (what's assigned, what's done)
5. Read `nexusos-bug-pool` (any open bugs affecting their work)
6. Read `nexusos-implementation-plan` ONLY for their relevant phases

---

## 5. Agent Behavior & Communication Constraints

To ensure maximum efficiency and preserve context limits, ALL agents (including the Coordinator) must abide by these strict personality and behavior rules:

1. **Extreme Brevity**: Do NOT provide long explanations of bugs, concepts, or code unless explicitly requested by the user. Do not waste context space.
2. **Conversational Bug Limit**: Any verbal explanation of a bug sent directly to the user MUST be strictly limited to a maximum of **15 words**.
3. **Bug Pool Tracking Limit**: When logging an issue into the `nexusos-bug-pool`, descriptions and info can be up to **3000 words**. However, entries must be as concise and informative as possible to preserve context space.
4. **No Pleasantries**: Do not be overly friendly. Omit greetings, conversational padding, apologies, and subjective commentary.
5. **Perfection & Honesty**: You are expected to be perfect in your job. Do not make unverified assumptions or mistakes. Be completely honest and direct about failures or impossibilities.
6. **Concise Thinking**: Agent internal thoughts/planning must be brief, strict, and devoid of casual or conversational filler. Only critical reasoning items permitted.
7. **Atomic DLC Protocol**: Any agent making a change MUST ensure documentation stays in sync by utilizing the `.agents/skills/nexusos-sync` skill. Code commits and KI updates are atomic.

---

## 6. Coding Standards (All Agents)

| Rule | Detail |
|---|---|
| **Language** | C (gnu11), NASM for assembly |
| **Indentation** | 4 spaces (no tabs) |
| **Naming** | `snake_case` functions/variables, `UPPER_CASE` macros, `_t` suffix for typedefs |
| **File headers** | Every file starts with block comment: filename, purpose, author |
| **Function docs** | Brief comment above every non-static function |
| **Includes** | Use `""` for project headers, `<>` for freestanding/limine headers |
| **No libc** | Kernel code uses `kernel/src/lib/` only. No `<stdio.h>`, `<stdlib.h>`, etc. |
| **Packed structs** | All hardware structs: `__attribute__((packed))` |
| **Volatile** | All MMIO: use `volatile` pointers |
| **Error returns** | `0` = success, negative = error, from all init functions |
| **Memory** | Use `kmalloc`/`kfree` from `mm/heap.h`. Never assume memory is zeroed. |
| **Interrupts** | Use `spinlock_acquire`/`release` for shared data. Never hold locks long. |

---

## 6. Build & Test Protocol

### 6.1 Before Committing

```bash
# Must pass with 0 errors, 0 C warnings
make clean && make all

# Must boot without triple-fault
make run-bios    # BIOS test
make run          # UEFI test
```

### 6.2 Testing New Drivers

```bash
# Add QEMU flags for device emulation:
make run QEMU_FLAGS="-device qemu-xhci -device usb-kbd"   # USB
make run QEMU_FLAGS="-hda test.img"                        # Disk
```

### 6.3 Serial Debug Output

For debugging, agents can use `-serial stdio` QEMU flag. Kernel serial output can be added via COM1 (I/O 0x3F8). This is encouraged for driver development.

---

## 7. Major Overhaul Protocol

For changes that affect multiple subsystems (e.g., switching from PIC to APIC, adding SMP, redesigning VFS):

1. **Propose**: Agent writes a proposal document in their conversation
2. **Review**: Coordinator reviews for architectural consistency
3. **Approve**: User approves the plan
4. **Branch**: Work on a separate branch if possible
5. **Test**: Full test suite must pass
6. **Merge**: Coordinator verifies, user approves merge
7. **Update Knowledge**: All affected knowledge items updated

---

## 8. Version Tracking

---

## 9. Documentation Synchronization (DLC Protocol)

Every agent session MUST begin and end with a documentation audit to prevent desync.

1. **Audit Startup**: Run `.agents/skills/nexusos-sync/scripts/audit.py` to verify current KI state against filesystem.
2. **Plan Sync**: Implementation plans MUST contain a "Knowledge Updates" section.
3. **Milestone Sync**: Update `task.md` and `progress_report.md` as milestones are reached.
4. **Final Sync**: Verify 0 desyncs via `audit.py` before final submission/turn.

| Field | Current Value |
|---|---|
| **Kernel Version** | 0.1.0 "Genesis" |
| **Phase Completed** | Phase 3 (Hardware/FS/Shell) |
| **Source Files** | 101 |
| **Build Status** | 0 errors, 0 C warnings |
| **Next Phase** | Phase 4 (Multitasking, USB, Userspace) |
