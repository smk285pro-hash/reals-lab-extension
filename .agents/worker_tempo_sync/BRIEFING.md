# BRIEFING — 2026-08-30T19:46:14Z

## Mission
Implement the Audio Tempo Mismatch and 8-Point Playhead Phase Sync Fix across core, bridge, extension, and test suites, ensuring realtime audio thread safety and 100% test pass with zero compiler warnings.

## 🔒 My Identity
- Archetype: worker
- Roles: implementer, qa, specialist
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\worker_tempo_sync
- Original parent: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Milestone: audio-tempo-sync-fix

## 🔒 Key Constraints
- ALWAYS use GitNexus MCP tools (impact, context, query, detect_changes) before modifying any symbols.
- Follow all guidelines in AGENTS.md, SPEC.md, PLAN.md, and DESIGN.md.
- Adhere to C++20, zero compiler warnings on MSVC (`cmake --build --preset windows`), and all tests passing (`ctest --preset windows`).
- Zero allocations (new/malloc/vector::resize), zero disk I/O, and zero mutex locking inside `ReaperOnAudioBuffer` / `renderFrames` / `dsp_on_read` audio callback path.
- Real implementations only, no dummy/facade implementations.

## Current Parent
- Conversation ID: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Updated: 2026-08-30T19:46:14Z

## Task Summary
- **What to build**: Audio Tempo Mismatch and 8-Point Playhead Phase Sync Fix.
- **Success criteria**:
  1. Host sample rate cold-start & atomic target sample rate.
  2. Frame metric & loop boundary alignment.
  3. Dynamic project BPM detection via live transport/TimeMap.
  4. Realtime-safe audio callback path (zero mutex locks, zero memory allocations).
  5. Mono master support & discontinuity handling.
  6. Multi-rate tests and phase sync diagnostic tests passing with zero warnings.
- **Interface contracts**: `core/include/reals/audio/Engine.h`, `bridge/include/reals/bridge/Bridge.h`, `extension/src/reaper_plugin.cpp`.
- **Code layout**: `core/`, `bridge/`, `extension/`, `tests/`.

## Key Decisions Made
- [TBD]

## Artifact Index
- `.agents/worker_tempo_sync/DISPATCH.md` — Assignment requirements
- `.agents/worker_tempo_sync/progress.md` — Progress tracker
- `.agents/worker_tempo_sync/BRIEFING.md` — Agent briefing & memory
- `.agents/worker_tempo_sync/handoff.md` — Final handoff report

## Change Tracker
- **Files modified**: None yet
- **Build status**: Untested
- **Pending issues**: None

## Quality Status
- **Build/test result**: Pending
- **Lint status**: 0
- **Tests added/modified**: Pending

## Loaded Skills
- None
