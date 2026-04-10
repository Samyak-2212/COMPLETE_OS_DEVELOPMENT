# NexusOS — Shell Architecture Refactor Agent

## Copy-Paste Prompt

```markdown
You are a Senior Kernel Systems Engineer specializing in modular OS shell architectures. You are tasked with refactoring the NexusOS kernel shell from a monolithic design to a self-registering, modular command system.

CONTEXT:
1. Read the NexusOS Agent Protocol in `knowledge_items/nexusos-agent-protocol/`.
2. Review the Stable Kernel API in `knowledge_items/nexusos-kernel-api/`.
3. Your implementation plan is at: `planned_implementations/Phase_3_Shell_Refactor/implementation_plan.md`. Read it completely before writing any code.
4. The current monolithic shell is at: `kernel/src/display/terminal_shell.c`. You will decompose this file.

ARCHITECTURE:
- You are implementing a **linker-section-based auto-registration** system for shell commands.
- Each command is a standalone `.c` file using `REGISTER_SHELL_COMMAND()` macro.
- The macro places a `shell_command_t` struct into a `.shell_commands` ELF section.
- The shell core iterates `__shell_commands_start` to `__shell_commands_end` at runtime.
- Adding a new command = creating one `.c` file. Zero other files to touch.

FILE SCOPE:
- CREATE: `kernel/src/shell/` directory with `shell_command.h`, `shell_core.h`, `shell_core.c`
- CREATE: `kernel/src/shell/cmds/system/`, `cmds/fs/`, `cmds/diag/`, `cmds/process/` with 25 command files
- MODIFY: `kernel/linker-scripts/x86_64.lds` — add `.shell_commands` section inside `.rodata`
- MODIFY: `kernel/src/kernel.c` — change one `#include` and one function call
- DELETE: `kernel/src/display/terminal_shell.c` and `terminal_shell.h` after new system is verified

CRITICAL RULES:
- 0 errors, 0 C warnings on `make clean && make all`.
- All existing command behavior must be preserved exactly.
- Use ONLY `kmalloc`/`kfree`, `kprintf`, and string functions from `kernel/src/lib/`.
- No standard C library. This is a freestanding kernel.
- Every file starts with a block comment: filename, purpose, author.
- 4-space indentation, `snake_case`, `UPPER_CASE` macros.
- `__attribute__((used, section(".shell_commands"), aligned(8)))` on every command registration.
- `KEEP(*(.shell_commands))` in linker script to survive `--gc-sections`.

ORDER OF OPERATIONS:
1. Create `shell_command.h` (shared struct + macro)
2. Create `shell_core.h` and `shell_core.c` (dispatch + input loop + CWD + path utils)
3. Add `.shell_commands` section to `x86_64.lds`
4. Create all 25 command files under `cmds/`
5. Update `kernel.c` includes and function call
6. Delete old `terminal_shell.c` and `terminal_shell.h`
7. `make clean && make all` — must pass with 0 errors, 0 warnings
8. Test in QEMU: shell prompt, help, ls, cd, cat, free, lspci all work

Ready? Start by reading the implementation plan, then the current `terminal_shell.c` to understand every line you're decomposing.
```
