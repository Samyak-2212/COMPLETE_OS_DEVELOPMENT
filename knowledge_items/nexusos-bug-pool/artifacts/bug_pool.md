# NexusOS Universal Bug Pool

> **Version**: 1.3 | **Last Updated**: 2026-04-21
> **Rule**: Any agent encountering an issue outside their scope, or an issue they cannot solve in their current session, MUST log it here. To modify this file, the agent must ask the user for explicit permission first.

---

## 🚨 Critical (System Halts, Triple Faults, Security)
*Issues that prevent booting, cause instant panics, or corrupt hardware/data.*

| ID | Component | Description | Status | Discovered By | Notes |
|---|---|---|---|---|---|
| BUG-001 | *Example* | *Triple fault on PS/2 init in VirtualBox* | `OPEN` | *Coordinator* | *Requires investigating IST configuration.* |
| BUG-002 | Build/QEMU UEFI | **"Host has not initialized the display yet"** — OVMF missing NVRAM vars pflash (unit=1). | `RESOLVED` | Kernel Agent | Fixed in GNUmakefile: added `ovmf-vars-x86_64.fd` as writable pflash unit=1 to all UEFI targets. |

---

## ⚠️ High (Feature Breakage, Severe Memory Leaks)
*Issues that break major functionality but don't halt the entire OS.*

| ID | Component | Description | Status | Discovered By | Notes |
|---|---|---|---|---|---|
| | | | | | |

---

## 🟡 Medium (Minor Glitches, Degradation)
*Issues that are annoying but bypassable, or minor specification deviations.*

| ID | Component | Description | Status | Discovered By | Notes |
|---|---|---|---|---|---|
| | | | | | |

---

## 🔵 Low (Cosmetics, Formatting, Tech Debt)
*Low priority cleanup tasks.*

| ID | Component | Description | Status | Discovered By | Notes |
|---|---|---|---|---|---|
| | | | | | |

---

## ✅ Resolved Bugs Archive
*Bugs that have been fixed and verified. Keep a record here.*

| ID | Component | Description | Resolved By | Resolution Notes |
|---|---|---|---|---|
| BUG-003 | AHCI Driver/DMA | AHCI DMA read produced zeros due to invalid virtual address translation for PRDT entries. | Driver Agent | Resolved by implementing a persistent-started AHCI port state and using static bounce buffers with known physical addresses. Enforced `volatile` MMIO to prevent register caching. |
| BUG-004 | Partitions/AHCI | Invalid memory mappings used for partition table scratch pages. | Driver Agent | Fixed by ensuring all AHCI DMA buffers are allocated via PMM and accessed via safe HHDM mappings. Corrected virtual address overlaps during MBR parsing. |
| BUG-006 | `kernel/src/lib/printf.c` — `vsnprintf` | `vsnprintf` handled only `%s/%d/%c/%%`. All hex/unsigned/long specifiers hit `default:` branch — emitted literal chars, consumed no va_args → stack corruption / UB. Fix: full rewrite with flag/width/length-mod parser. | Bug-Fix Agent | 2026-04-17 |
| BUG-007 | `init_process.c` | Missing `#include "mm/heap.h"` → `kcalloc` implicit function declaration (returns int not ptr on 64-bit → silent ptr truncation). | Coordinator | 2026-04-21: Added missing include. |
| BUG-008 | `usb_device.c:usb_control_transfer` | `(uint64_t *)setup` cast on `usb_setup_t __attribute__((packed))` → unaligned load UB (`setup` is align=1, uint64_t* requires align=8). Could corrupt setup bytes on some CPUs. | Coordinator | 2026-04-21: Replaced with `__builtin_memcpy(&setup_raw, setup, 8)`. |
| BUG-009 | `xhci.c:xhci_process_events` | `slot_id` and `ep_id` extracted from TRB but not used in success path → compiler `-Wunused-variable`. | Coordinator | 2026-04-21: Restructured condition so vars are always referenced in the debug log path. |
| BUG-010 | `process_destroy` | Only freed PML4 physical page, not user-space pages or page tables → massive memory leak on exit. COW pages never unref'd → physical pages never freed. PCB VMA linked list leaked. | Coordinator | 2026-04-21: Implemented `vmm_destroy_address_space()` (walks PML4[0-255], calls `pmm_page_unref` per user page, frees PT/PD/PDPT). `process_destroy` now frees VMAs + calls it. |
| BUG-011 | `vmm.h` / `vmm.c` | `vmm_destroy_address_space(uint64_t)` declared in `vmm.h` but never defined → silent linker bomb (unused in Phase 4 so not caught). | Coordinator | 2026-04-21: Fully implemented. |
| BUG-012 | `syscall_entry.asm` | `.data` section accessed from `.text` via 32-bit absolute refs → `warning: reloc refers to section` in 64-bit ELF; 32-bit ref to 64-bit symbols is invalid above 4 GB. | Coordinator | 2026-04-21: Added `default rel` directive — all memory refs now RIP-relative 64-bit. |
