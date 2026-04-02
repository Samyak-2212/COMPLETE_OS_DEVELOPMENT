# NexusOS GUI — Phase 6 (RESERVED)

> **Status**: Not yet implemented.

This directory will contain the graphical user interface server for NexusOS, including:

- **gui_server.c** — Window compositor, mouse cursor, event dispatch
- **wm.c** — Window manager (stacking, drag, resize, close)
- **widgets/** — UI widget toolkit (buttons, text fields, panels)
- **gui_terminal.c** — Terminal emulator widget inside a GUI window

## How It Works

The display manager (`display_manager.c`) checks for the GUI via `__attribute__((weak))` symbols:

- If `gui_server_init` resolves to a valid function → GUI takes over
- If `gui_server_init` is NULL → terminal console remains active
- Terminal remains accessible via Ctrl+Alt+F1 virtual console switching

## Implementation Notes

When implementing Phase 6:
1. Create `gui_server.c` that implements `gui_server_init()`
2. The display manager will automatically detect and transition to GUI
3. Terminal becomes a virtual console accessible via hotkey
