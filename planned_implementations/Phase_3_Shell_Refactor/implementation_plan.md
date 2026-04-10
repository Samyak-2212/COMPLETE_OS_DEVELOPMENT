# Implementation Plan — Shell Command Architecture Refactor

## Goal

Refactor the monolithic `terminal_shell.c` (415 lines, 26 commands in one file) into a **modular, self-registering command system** where each command lives in its own `.c` file within categorized folders. Adding a new command requires creating one file — no other code touches needed.

## User Review Required

> [!IMPORTANT]
> This plan modifies two **protected files**:
> 1. `kernel/linker-scripts/x86_64.lds` — adds a `.shell_commands` section
> 2. `kernel/src/kernel.c` — changes one `#include` path
>
> Coordinator (me) approves both edits as architecturally necessary.

> [!WARNING]
> The old `kernel/src/display/terminal_shell.c` and `terminal_shell.h` will be **deleted** and replaced by the new structure. All existing command functionality is preserved 1:1 in the new files.

---

## Architecture — Linker Section Auto-Registration

The key design: each command file declares a `shell_command_t` struct using a `REGISTER_SHELL_COMMAND()` macro. That macro places the struct into a dedicated ELF section (`.shell_commands`). At runtime, the shell core iterates this section to discover all registered commands — **no central table, no init-call list, no manual registration**.

```
┌─────────────────────────────────────────────────────────┐
│               COMPILE TIME (per .c file)                │
│                                                         │
│  cmd_ls.c:                                              │
│    REGISTER_SHELL_COMMAND(ls, "List directory", ...)    │
│       ↓                                                 │
│    Places shell_command_t into ELF section               │
│    ".shell_commands"                                     │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│               LINK TIME (x86_64.lds)                    │
│                                                         │
│  .shell_commands : {                                     │
│      __shell_commands_start = .;                         │
│      KEEP(*(.shell_commands))                            │
│      __shell_commands_end = .;                           │
│  }                                                       │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│               RUNTIME (shell_core.c)                    │
│                                                         │
│  for (cmd = __shell_commands_start;                     │
│       cmd < __shell_commands_end; cmd++)                │
│      if (strcmp(input, cmd->name) == 0)                  │
│          cmd->handler(argc, argv);                       │
└─────────────────────────────────────────────────────────┘
```

**Adding a new command = create one `.c` file. Done.**

The Makefile already auto-discovers all `.c` files under `src/`. The linker `KEEP()` prevents `--gc-sections` from dropping the section. `__attribute__((used))` prevents the compiler from optimizing it away.

---

## New Directory Structure

```
kernel/src/shell/                          ← NEW top-level module
├── shell_command.h                        ← Command struct + REGISTER macro
├── shell_core.h                           ← Public API (shell_run, shell_execute)
├── shell_core.c                           ← Input loop, dispatch, path utils, CWD state
└── cmds/                                  ← All commands, organized by category
    ├── system/                            ← OS-level utilities
    │   ├── cmd_help.c                     ← Lists all registered commands
    │   ├── cmd_clear.c                    ← Clear terminal
    │   ├── cmd_uname.c                    ← System info
    │   ├── cmd_reboot.c                   ← PS/2 reset
    │   ├── cmd_shutdown.c                 ← ACPI halt
    │   └── cmd_dmesg.c                    ← Dump klog buffer
    │
    ├── fs/                                ← Filesystem commands
    │   ├── cmd_ls.c                       ← List directory contents
    │   ├── cmd_cd.c                       ← Change working directory
    │   ├── cmd_pwd.c                      ← Print working directory
    │   ├── cmd_cat.c                      ← Print file contents
    │   ├── cmd_echo.c                     ← Print text (and > redirect)
    │   ├── cmd_mkdir.c                    ← Create directory
    │   ├── cmd_rmdir.c                    ← Remove empty directory
    │   ├── cmd_rm.c                       ← Remove file
    │   ├── cmd_cp.c                       ← Copy file
    │   ├── cmd_mv.c                       ← Move/rename file
    │   ├── cmd_touch.c                    ← Create empty file
    │   ├── cmd_mount.c                    ← Mount filesystem
    │   ├── cmd_umount.c                   ← Unmount filesystem
    │   └── cmd_df.c                       ← Disk free space
    │
    ├── diag/                              ← Diagnostics and monitoring
    │   ├── cmd_free.c                     ← Memory usage
    │   ├── cmd_uptime.c                   ← System uptime
    │   ├── cmd_lspci.c                    ← PCI device listing
    │   └── cmd_colortest.c               ← Terminal color test
    │
    └── process/                           ← Process management (Phase 4 stubs for now)
        ├── cmd_ps.c                       ← Process list
        └── cmd_kill.c                     ← Kill process
```

**Total: 4 infrastructure files + 25 command files = 29 files**

---

## Proposed Changes

### 1. Shell Infrastructure

#### [NEW] kernel/src/shell/shell_command.h

The shared header defining the command struct and the registration macro.

```c
/* shell_command.h — Shell Command Registration Interface */
#ifndef NEXUS_SHELL_COMMAND_H
#define NEXUS_SHELL_COMMAND_H

#include <stdint.h>

/* Maximum constants */
#define SHELL_MAX_LINE_LEN  256
#define SHELL_MAX_ARGS      16

/* Command handler signature */
typedef int (*shell_cmd_handler_t)(int argc, char **argv);

/* Command descriptor — placed into .shell_commands ELF section */
typedef struct {
    const char          *name;
    const char          *description;
    const char          *category;       /* "system", "fs", "diag", "process" */
    shell_cmd_handler_t  handler;
} shell_command_t;

/*
 * REGISTER_SHELL_COMMAND — Zero-touch command registration macro.
 *
 * Usage (in any .c file):
 *   static int cmd_foo(int argc, char **argv) { ... }
 *   REGISTER_SHELL_COMMAND(foo, "Does foo things", "system", cmd_foo);
 *
 * The macro places the shell_command_t struct into a dedicated ELF
 * section ".shell_commands". The linker script collects all such
 * structs into a contiguous array bounded by sentinel symbols.
 * The shell core iterates them at runtime — no manual table edits.
 */
#define REGISTER_SHELL_COMMAND(cmd_name, desc, cat, func)           \
    static const shell_command_t __shell_cmd_##cmd_name             \
        __attribute__((used, section(".shell_commands"), aligned(8))) = { \
            .name        = #cmd_name,                               \
            .description = desc,                                    \
            .category    = cat,                                     \
            .handler     = func                                     \
        }

/* Linker-provided section boundaries (defined in shell_core.c) */
extern const shell_command_t __shell_commands_start[];
extern const shell_command_t __shell_commands_end[];

/* Shared shell state accessors (defined in shell_core.c) */
const char *shell_get_cwd(void);
void shell_normalize_path(const char *input, char *output);

#endif /* NEXUS_SHELL_COMMAND_H */
```

#### [NEW] kernel/src/shell/shell_core.h

Public API for `kernel.c` to call.

```c
/* shell_core.h — Kernel Shell Public API */
#ifndef NEXUS_SHELL_CORE_H
#define NEXUS_SHELL_CORE_H

/* Launch the interactive kernel shell (blocks forever) */
void shell_run(void);

/* Execute a single command line */
int shell_execute(const char *line);

#endif
```

#### [NEW] kernel/src/shell/shell_core.c

Contains:
- CWD state (`cwd`, `cwd_node`) and accessors
- `shell_normalize_path()` — moved from old `terminal_shell.c`
- `shell_execute()` — tokenizer + command dispatch via section iteration
- `shell_run()` — input loop (reads from `input_poll_event`)
- Linker symbol externs: `__shell_commands_start`, `__shell_commands_end`

This is the **only** file that knows about the command dispatch mechanism. Individual commands are completely decoupled.

---

### 2. Linker Script Modification

#### [MODIFY] [x86_64.lds](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/linker-scripts/x86_64.lds)

Add a `.shell_commands` section inside the `.rodata` segment (commands are `const`):

```diff
     /* Read-only data */
     . = ALIGN(CONSTANT(MAXPAGESIZE));
     .rodata : {
         *(.rodata .rodata.*)
+
+        /* Shell command auto-registration table */
+        . = ALIGN(8);
+        __shell_commands_start = .;
+        KEEP(*(.shell_commands))
+        __shell_commands_end = .;
     } :rodata
```

`KEEP()` prevents `--gc-sections` from discarding command entries. The `ALIGN(8)` ensures struct alignment matches the `aligned(8)` attribute in the macro.

---

### 3. Kernel Entry Update

#### [MODIFY] [kernel.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/kernel.c)

Change:
```diff
-#include "display/terminal_shell.h"
+#include "shell/shell_core.h"
```

And at the bottom:
```diff
-    terminal_shell_run();
+    shell_run();
```

---

### 4. Individual Command Files

Each command file follows this exact template:

```c
/* ============================================================================
 * cmd_<name>.c — NexusOS Shell Command: <name>
 * Purpose: <one-line description>
 * Author: NexusOS Kernel Team
 * ============================================================================ */

#include "shell/shell_command.h"
/* ...additional includes as needed... */

static int cmd_<name>(int argc, char **argv) {
    /* implementation */
    return 0;
}

REGISTER_SHELL_COMMAND(<name>, "<description>", "<category>", cmd_<name>);
```

#### Command Implementation Details

| File | Category | Status | Notes |
|---|---|---|---|
| `cmd_help.c` | system | **Port** from old code | Iterates `__shell_commands_start` to `__shell_commands_end`, groups by category |
| `cmd_clear.c` | system | **Port** | ANSI `\033[2J\033[H` |
| `cmd_uname.c` | system | **Port** | Prints version string |
| `cmd_reboot.c` | system | **Port** | PS/2 controller reset (0x64 → 0xFE) |
| `cmd_shutdown.c` | system | **Port** | `cli; hlt` (ACPI proper in Phase 5) |
| `cmd_dmesg.c` | system | **Port** | Calls `klog_dump()` |
| `cmd_ls.c` | fs | **Port** | Uses `vfs_resolve_path` + `vfs_readdir` |
| `cmd_cd.c` | fs | **Port** | Updates CWD via `shell_set_cwd()` |
| `cmd_pwd.c` | fs | **Port** | Prints CWD |
| `cmd_cat.c` | fs | **Port** | `vfs_read` + `kprintf` |
| `cmd_echo.c` | fs | **Enhance** | Add `> file` and `>> file` redirect support via `vfs_write` |
| `cmd_mkdir.c` | fs | **Implement** | Calls `vfs_mkdir(normalized_path)` |
| `cmd_rmdir.c` | fs | **Stub** | Prints "Not yet implemented" (needs `vfs_rmdir`) |
| `cmd_rm.c` | fs | **Stub** | Prints "Not yet implemented" (needs `vfs_unlink`) |
| `cmd_cp.c` | fs | **Stub** | Prints "Not yet implemented" (needs read+write) |
| `cmd_mv.c` | fs | **Stub** | Prints "Not yet implemented" (needs rename) |
| `cmd_touch.c` | fs | **Implement** | Creates empty file via ramfs (needs `ramfs_create_file`) |
| `cmd_mount.c` | fs | **Stub** | Manual mount (Phase 3 auto-mounts, manual can wait) |
| `cmd_umount.c` | fs | **Stub** | Prints "Not yet implemented" |
| `cmd_df.c` | fs | **Stub** | Prints "Not yet implemented" (needs FS stat) |
| `cmd_free.c` | diag | **Port** | PMM page count stats |
| `cmd_uptime.c` | diag | **Port** | PIT tick counter |
| `cmd_lspci.c` | diag | **Port** | PCI device listing |
| `cmd_colortest.c` | diag | **Port** | ANSI 16-color test |
| `cmd_ps.c` | process | **Stub** | Prints "Not yet implemented" (Phase 4) |
| `cmd_kill.c` | process | **Stub** | Prints "Not yet implemented" (Phase 4) |

**Port** = existing functionality moved verbatim from `terminal_shell.c`
**Implement** = new working implementation
**Enhance** = existing functionality + new features
**Stub** = placeholder until prerequisite subsystems exist

---

### 5. File Deletions

#### [DELETE] [terminal_shell.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/terminal_shell.c)
#### [DELETE] [terminal_shell.h](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/terminal_shell.h)

All functionality moved to `kernel/src/shell/`.

---

## Agent Prompt

The implementing agent should be given the following scope:

**Target Directory**: `kernel/src/shell/` (new), plus minor edits to `kernel.c` and `x86_64.lds`

**Constraints**:
- Must not break the build (0 errors, 0 warnings)
- Must preserve all existing command behavior exactly
- The `help` command should group commands by category
- All string operations use `kernel/src/lib/string.h` (no libc)
- All memory allocation via `kmalloc`/`kfree`
- `__attribute__((packed))` not needed here (no hardware structs)
- After implementation, delete old `display/terminal_shell.c` and `display/terminal_shell.h`

---

## Verification Plan

### Automated Tests
```bash
make clean && make all    # Must be 0 errors, 0 C warnings
make run                  # Boot in QEMU
```

### Manual Verification
1. Shell prompt appears (`nexus:/$`)
2. `help` lists all 25 commands grouped by category
3. `ls /` shows `dev`, `tmp`, `mnt`
4. `cd /tmp && pwd` prints `/tmp`
5. `echo hello > /tmp/test.txt` creates file (if redirect implemented)
6. `cat /tmp/test.txt` prints `hello`
7. `mkdir /tmp/foo && ls /tmp` shows `foo`
8. `free` shows memory stats
9. `lspci` shows PCI devices
10. `uname` prints version

### Architecture Verification
- Create a trivial new command file `cmd_test.c` with `REGISTER_SHELL_COMMAND(test, ...)` → rebuild → `test` appears in `help` without touching any other file.
