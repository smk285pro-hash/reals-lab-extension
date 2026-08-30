# BRIEFING — 2026-08-31T02:44:00Z

## Mission
Investigate BPM sync & time-stretching calculations in Bridge.cpp, reaper_plugin.cpp, Engine.cpp, SoundTouch usage, and identify why preview audio plays faster than DAW tempo.

## 🔒 My Identity
- Archetype: explorer
- Roles: [investigator, synthesizer]
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_tempo_bpm
- Original parent: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Milestone: BPM & Time-Stretching Math Investigation

## 🔒 Key Constraints
- Read-only investigation — do NOT implement directly in source files.
- MUST use GitNexus MCP tools for code exploration.
- Strictly adhere to AGENTS.md, SPEC.md, PLAN.md, and the 8-point Playhead Phase Sync specification.

## Current Parent
- Conversation ID: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Updated: 2026-08-31T02:44:00Z

## Investigation State
- **Explored paths**: `bridge/src/Bridge.cpp`, `bridge/include/reals/bridge/Bridge.h`, `core/src/audio/Engine.cpp`, `core/include/reals/audio/Engine.h`, `core/src/audio/SoundTouchProcessor.cpp`, `core/include/reals/audio/SoundTouchProcessor.h`, `extension/src/reaper_plugin.cpp`, `core/src/audio/DragExporter.cpp`, `ui-web/app.js`, `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp`, `tests/suites/TestSuite_AudioDSP.cpp`, `tests/suites/TestSuite_SoundTouchCore.cpp`.
- **Key findings**:
  1. SoundTouch `setTimeRatio` uses `st.setTempo(clamped)` which correctly scales playback speed without changing pitch (pitch preserved via WSOLA). Formula `ratio = projectBpm / sampleBpm` is mathematically exact.
  2. Four distinct root causes of tempo acceleration / mismatch were discovered: (a) Sample rate mismatch (e.g. 44.1k file output directly to 48k/96k buffer if `targetSampleRate` is uninitialized or mismatched); (b) `ExtHostActions::projectTempo()` calling static `Master_GetTempo()` instead of dynamic `TimeMap_GetDividedBpmAtTime(playPos)` when tempo markers are active; (c) Half-time / double-time detection errors in `TempoDetector`; (d) Drag-and-drop double-stretching when pre-rendering and applying `D_PLAYRATE`.
- **Unexplored areas**: None.

## Key Decisions Made
- Provided complete mathematical proofs and precise before/after code diff proposals for `reaper_plugin.cpp`, `Bridge.cpp`, and `Engine.cpp`.

## Artifact Index
- .agents/explorer_tempo_bpm/DISPATCH.md — Initial dispatch log
- .agents/explorer_tempo_bpm/progress.md — Liveness heartbeat and task checklist
- .agents/explorer_tempo_bpm/handoff.md — Comprehensive 5-component report
