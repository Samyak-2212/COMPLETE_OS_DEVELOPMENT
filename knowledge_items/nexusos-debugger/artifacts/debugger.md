# NexusOS Serial Debugger Subsystem

The **Serial Debugger Subsystem** is a centralized kernel service providing structured diagnostic information over **COM1 (0x3F8)**. It is designed for multi-agent use cases where display access is unavailable or secondary.

---

## 🛠 Features

### 1. 16550 UART Driver
A robust driver for the universal asynchronous receiver-transmitter (UART) chip.
- **Port**: `0x3F8` (COM1)
- **Baud Rate**: `115200`
- **Path**: [serial.h](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/lib/serial.h), [serial.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/lib/serial.c)

### 2. High-Level Debugging API
High-level routines for system introspection.
- **Path**: [debug.h](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/lib/debug.h), [debug.c](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/kernel/src/lib/debug.c)
- **Default Mode**: **Structured JSON** for machine parsing.
- **Alternative Mode**: **Human-Readable (HR)** (use `-hr` or `debug_set_mode(DEBUG_MODE_HR)`).
- **Core Functions**:
  - `debug_log(level, component, message)`
  - `debug_dump_mem(address, length)`
  - `debug_backtrace()` (requires `-fno-omit-frame-pointer`)

### 3. Machine-Parsable JSON Schema
The debugger outputs single-line JSON objects to the serial port by default.

#### Log Entry
```json
{"type":"log","level":"INFO|WARN|ERROR|PANIC","comp":"SUBSYSTEM","msg":"Message text"}
```

#### Memory Dump
```json
{"type":"memdump","addr":"0xffffffff80100000","len":64,"data":"abcdef01..."}
```

#### Stack Trace
```json
{"type":"trace","frames":["0xffffffff80201000", "0xffffffff80201200", "0xffffffff80201300"]}
```

---

## 🚀 Usage for Agents
Agents should listen to the serial terminal (QEMU `-serial stdio`) and parse the JSON stream for automated status reporting and debugging.

### Enabling Human-Readable Output
Call `debug_set_mode(DEBUG_MODE_HR);` in `kmain()` to switch output to a formatted text style.

---

## 🔒 Multi-Agent Protocol
- **No Manual Deletion**: Do not delete logs or debugger source files without user permission.
- **Report Panics**: Any `kpanic()` will trigger an automated JSON summary on the serial port.
- **Structured Interaction**: Agents can add specific JSON-producing debuggers for new subsystems (VFS, Scheduler, etc.).
