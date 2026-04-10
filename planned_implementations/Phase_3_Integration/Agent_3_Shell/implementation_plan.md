# Implementation Plan — Agent 3: Console & Shell Engineer

## Role & Expertise
You are a **Senior Terminal & UI Engineer**. Your expertise lies in high-performance framebuffer rendering, VT100/ANSI escape code parsing, and interactive shell command design. You write snappy, responsive interfaces that users love to interact with.

## Objective
Finalize the Kernel Console and implement the interactive Kernel Shell.

## Proposed Changes

### 1. Terminal Subsystem
#### [MODIFY] [terminal.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/terminal.c)
- Complete the ANSI Escape Code parser (Colours, Cursor movement).
- Implement logical line wrapping and robust scrolling.
- Integrate with the Input Manager to handle raw keyboard events.

### 2. Kernel Shell
#### [NEW] [terminal_shell.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/terminal_shell.c)
- Implement Command Parser (Space-delimited arguments).
- Implement core commands:
    - **FS**: `ls`, `cd`, `pwd`, `cat`, `mkdir`, `touch`, `rm`, `cp`.
    - **System**: `clear`, `uname`, `free`, `uptime`, `lspci`.
    - **Display**: `colortest`.
- Implement a command history buffer (Up/Down arrow support).

### 3. Display Management
#### [MODIFY] [display_manager.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/display/display_manager.c)
- Ensure the Display Manager correctly routes `kprintf` output to the active terminal.

## Verification Plan
1. **Interactive Shell**: Use the PS/2 keyboard to type commands and verify output.
2. **FS Integrity**: Create a file in `/tmp`, `ls` it, and `cat` its content.
3. **ANSI Colors**: Run a color test command to verify 16-color FG/BG support.
