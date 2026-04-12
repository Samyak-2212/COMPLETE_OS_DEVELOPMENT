# NexusOS Shell API Documentation

This knowledge artifact governs the absolute method for integrating new shell commands into NexusOS. Agents tasked with extending the shell *must* follow these documentation standards.

## Command Creation & Registration

NexusOS operates via a monolithic static linker array for its shell (`.shell_commands`). You do **not** need to manually edit `shell_core.c` or build function pointer tables when making a new command. 

### The `REGISTER_SHELL_COMMAND` Macro

Whenever you write a new command handler (e.g. `cmd_foo`), you must instantiate it using the universally provided macro at the bottom of the C file:

```c
#include "shell/shell_command.h"

static int cmd_foo(int argc, char **argv) {
    kprintf("Executing Foo...\n");
    return 0;
}

REGISTER_SHELL_COMMAND(foo, "Short one liner", "Full usage block", "system", cmd_foo);
```

### Parameter Breakdown

To ensure your commands dynamically integrate accurately into the automated `help` command engine, parameters must be provided reliably:

1. `name`: The raw command string without quotes (e.g., `foo`).
2. `short_desc`: A highly concise, single-line explanation. This maps exactly into the overarching `help` categorized menu.
3. `usage_help`: A larger string block. This string is displayed automatically when the user types `foo --help`, `foo -h`, or `help foo`. 
4. `category`: Must neatly conform to existing domains (`"system"`, `"fs"`, `"diag"`, `"process"`).
5. `handler`: The C function pointer serving the command.

### Collision Exclusions (`-h` support)

The monolithic dispatcher natively intercepts both `--help` and `-h` commands to output the `.usage_help` documentation passively. 
If your specific command utilizes `-h` as a native functional parameter (e.g., `ls -h` for human-readable byte calculations), you must exclusively utilize the extended macro and pass `true` to block the `-h` interceptor:

```c
REGISTER_SHELL_COMMAND_EXT(ls, "List dir", "Display files", "fs", cmd_ls, true);
```
Passing `true` mathematically disables `-h` from printing documentation, guaranteeing that the `--help` flag acts as your sole documentation trigger.
