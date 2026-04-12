# NexusOS — Agent Onboarding

Before doing anything else, read the knowledge items in `knowledge_items/`.
They are the authoritative source of truth for this project.

## Required Reading (in order)

1. `knowledge_items/nexusos-agent-protocol/artifacts/AGENT_PROTOCOL.md` — roles, boundaries, coding standards, behavior rules
2. `knowledge_items/nexusos-kernel-api/artifacts/KERNEL_API.md` — stable kernel interfaces to code against
3. `knowledge_items/nexusos-progress-report/artifacts/progress_report.md` — what is built and what is pending
4. `knowledge_items/nexusos-task-tracker/artifacts/task.md` — phase-by-phase task checklist
5. `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md` — open bugs that may affect your work
6. `knowledge_items/nexusos-implementation-plan/artifacts/implementation_plan.md` — full architecture spec (read only the sections relevant to your phase)
7. `knowledge_items/nexusos-shell-api/artifacts/SHELL_API.md` — instructions for shell command creation and automating `--help` documentation

## Optional Reading

- `knowledge_items/nexusos-debugger/artifacts/debugger.md` — serial debugger API (COM1)
- `knowledge_items/nexusos-history/artifacts/history.md` — git changelog and sync log
- `knowledge_items/nexusos-gitignore/artifacts/.gitignore` — what to exclude from version control

## Key Rules (summary)

- Do not modify files outside your agent role without Coordinator + user approval.
- Protected files (`kernel.c`, `hal/`, `mm/`, linker scripts) require Coordinator approval before any edit.
- Keep all conversational bug explanations to **15 words maximum**.
- No pleasantries. No padding. Be direct and precise.
- Every kernel struct that maps to hardware must use `__attribute__((packed))`.
- All MMIO access must use `volatile` pointers.
- Use `kmalloc`/`kfree` only — no standard library.
- Build must pass with **0 errors, 0 C warnings** before any commit.

## Filing Bugs

If you encounter an issue outside your scope or cannot resolve it in your session, log it in `knowledge_items/nexusos-bug-pool/artifacts/bug_pool.md`. Ask the user for permission before modifying that file.
