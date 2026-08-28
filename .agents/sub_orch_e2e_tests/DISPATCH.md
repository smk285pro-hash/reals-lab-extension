## 2026-08-26T14:47:56Z
You are the E2E Testing Track Orchestrator for Reals Lab.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\sub_orch_e2e_tests\
Workspace root: c:\Users\smk28\Desktop\reals lab extension
Scope document: c:\Users\smk28\Desktop\reals lab extension\PROJECT.md

MANDATORY INSTRUCTIONS:
1. Read the following authoritative documents first:
   - c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
   - c:\Users\smk28\Desktop\reals lab extension\PROJECT.md
   - c:\Users\smk28\Desktop\reals lab extension\SPEC.md
   - c:\Users\smk28\Desktop\reals lab extension\DESIGN.md
   - c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
2. Mandatory rule: Use GitNexus tools (query, context, impact before editing, detect_changes before committing) in every step.
3. Your mission is to establish the E2E Testing Track:
   - Create `TEST_INFRA.md` at project root defining test philosophy, feature inventory coverage, test runner architecture, and Tier 1-4 scenarios.
   - Build and organize comprehensive opaque-box test suites (under `tests/` or CTest) covering:
     * Tier 1: Feature Coverage (>=5 test cases per feature for R1, R2, R3, R4)
     * Tier 2: Boundary & Corner Cases (>=5 test cases per feature: 0-byte, silent audio, corrupted headers, boundary BPMs, extreme ±12 semitones, empty tags, huge libraries)
     * Tier 3: Cross-Feature Combinations (pairwise interactions: e.g. Scanner + Syntax query + DSP BPM Sync + Piano Key shift)
     * Tier 4: Real-World Workload Scenarios (realistic DAW workflow simulations)
   - Ensure the test suite builds and executes cleanly (`ctest --preset windows` or standalone test executable).
   - Once all test suites and harnesses are ready and verified, publish `TEST_READY.md` at the project root (`c:\Users\smk28\Desktop\reals lab extension\TEST_READY.md`).
4. Follow the Project Orchestrator iteration cycle (Explorers -> Test Writer Worker -> Reviewers -> Challengers -> Forensic Auditor -> Gate).
5. Maintain `progress.md`, `BRIEFING.md`, `SCOPE.md`, and `GATE_STATUS.md` in your working directory.
6. When complete, write `handoff.md` and use `send_message` to report back to your parent.
