## 2026-08-29T02:08:06+07:00
You are the Project Orchestrator for a comprehensive multi-agent audit and code inspection of the entire codebase at c:/Users/smk28/Desktop/reals lab extension.

Your working directory is: c:/Users/smk28/Desktop/reals lab extension/.agents/orchestrator_2
User request is recorded at: c:/Users/smk28/Desktop/reals lab extension/.agents/ORIGINAL_REQUEST.md
Project workspace root: c:/Users/smk28/Desktop/reals lab extension

Key Instructions:
1. Read c:/Users/smk28/Desktop/reals lab extension/.agents/ORIGINAL_REQUEST.md and AGENTS.md, SPEC.md, PLAN.md, DESIGN.md.
2. IMPORTANT CONSTRAINT: Inspect the codebase directly using file and search tools without using GitNexus tools.
3. If spawning subagents, use Model: "flash" or "flash_lite" to conserve quota. Or you may perform direct inspection and synthesis into CODEBASE_AUDIT_REPORT.md.
4. Requirements to cover:
   - R1: Architecture & Layer Boundary Audit (core/ inclusions of ImGui/GLFW/reaper_plugin; ui/ inclusions of GLFW/reaper_plugin; app/ and extension/ thin shells; UI localization tr("key") backed by assets/i18n/; file size >400 lines).
   - R2: Code Quality, Memory & Concurrency (C++20 compliance, PascalCase/camelCase/m_/k conventions, smart pointers vs raw owning pointers, audio thread real-time safety - malloc/free/mutex/blocking in audio callbacks, null checks, boundary conditions).
   - R3: Build & Test Diagnostics (CMake config, test suite coverage and edge cases).
   - R4: Synthesis & Structured Markdown Audit Report CODEBASE_AUDIT_REPORT.md categorized by Severity (Critical, Major, Minor, Style/Lint), file & line reference, rule/contract violated, and concrete remediation recommendation.
5. Continuously maintain progress.md and BRIEFING.md in your working directory (.agents/orchestrator_2/).
6. Synthesize all findings into CODEBASE_AUDIT_REPORT.md at project root.
7. When finished, send a completion message with summary and artifacts to your parent agent (the Sentinel).
