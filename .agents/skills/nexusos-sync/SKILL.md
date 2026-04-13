---
name: nexusos-sync
description: >
  Protocol and tools for maintaining permanent synchronization between the
  NexusOS codebase and its Knowledge Items (task.md, progress_report.md).
  Ensures Phase completion and source counts are always accurate.
---

Perform this "Audit-Sync" loop for EVERY development task. Failure results in documentation-codebase desync.

## The DLC (Documentation Lifecycle) Protocol

### Phase 1: Pre-Audit (START of task)
1. Read `task.md` and `progress_report.md`.
2. Run `scripts/audit.py` to detect any existing desyncs.
3. If desync found, the FIRST part of your plan MUST be "Synchronize Documentation".

### Phase 2: Atomic Planning (DURING task)
1. Your implementation plan MUST have a "Documentation Updates" section.
2. Identify which tasks will mark checkboxes in `task.md`.
3. Identify how the file count or Phase status in `progress_report.md` will change.

### Phase 3: Synchronous Execution (DURING task)
1. Update Knowledge Items (KIs) IN THE SAME TURN as code changes where possible.
2. DO NOT wait until the final turn to sync everything.

### Phase 4: Verification (END of task)
1. Run `scripts/audit.py` again.
2. Verify all 0 desyncs.
3. The final `walkthrough.md` must link to the updated KIs.

## Key Targets
- **[task.md](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/knowledge_items/nexusos-task-tracker/artifacts/task.md)**
- **[progress_report.md](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/knowledge_items/nexusos-progress-report/artifacts/progress_report.md)**
- **[AGENT_PROTOCOL.md](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md)** (Version tracking section)
- **[KERNEL_API.md](file:///c:/Users/naska/OneDrive/Documents/GitHub/COMPLETE_OS_DEVELOPMENT/knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md)** (Stable interface marker)

## Command Reference
`python .agents/skills/nexusos-sync/scripts/audit.py`
