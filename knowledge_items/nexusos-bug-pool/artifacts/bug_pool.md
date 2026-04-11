# NexusOS Universal Bug Pool

> **Version**: 1.1 | **Last Updated**: 2026-04-11
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
