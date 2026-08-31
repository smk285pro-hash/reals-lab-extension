## 2026-09-01T01:18:46+07:00
You are the Project Orchestrator for Reals Lab REAPER Extension.
Your working directory is `c:\Users\smk28\Desktop\reals lab extension\.agents\orchestrator_1\`.
Read the authoritative user request at `c:\Users\smk28\Desktop\reals lab extension\ORIGINAL_REQUEST.md`.

You must implement and verify the following:
1. Global Favorites View (`★`): Display all favorited audio and MIDI files across entire library (all roots & subfolders).
2. Global Search across all root folders recursively with <50ms response times, supporting text, `/tag`, `/bpm:range`, `/key:note` filters, and restore view/scroll on clear.
3. Clean initial default roots (0 default OS folders like Music/Desktop/Downloads on fresh install).
4. File browsing performance optimization & zero-lag benchmarking for 5,000+ files (<30ms listing/search, 60fps scrolling, thread safety, zero memory leaks).
5. Ensure `cmake --build --preset windows` compiles with 0 warnings / 0 errors and `ctest --preset windows` passes 100%.

Strictly follow AGENTS.md, PLAN.md, DESIGN.md, SPEC.md and GitNexus workflows.
Maintain `plan.md`, `progress.md`, and `BRIEFING.md` in your working directory.
When fully finished and verified with builds/tests, report completion with full details to the caller.
