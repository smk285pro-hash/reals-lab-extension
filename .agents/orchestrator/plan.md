# Plan — Audio Tempo Mismatch & Playhead Phase Sync Fix

## Objective
Investigate and fix the audio tempo mismatch (preview playing faster than DAW) and verify the REAPER Extension implementation against the official 8-point Playhead Phase Sync master specification.

## Plan Steps
1. **Phase 0: Survey & Investigation (Parallel Explorers / Spec Miners)**
   - Explorer 1 (`teamwork_preview_explorer`): Investigate audio decoding, sample rate conversion (miniaudio/SoundTouch/Engine), host sample rate detection in REAPER extension vs Engine.
   - Explorer 2 (`teamwork_preview_explorer`): Investigate BPM calculation, timeRatio / SoundTouch tempo math in Bridge.cpp, Engine.cpp, and Sync BPM mode vs normal mode.
   - Spec Miner (`teamwork_preview_spec_miner`): Probe 8-point Playhead Phase Sync specification adherence in `extension/src/reaper_plugin.cpp`, `core/src/audio/Engine.cpp`, and `bridge/src/Bridge.cpp` (hardware hook, thread safety, atomic transport, zero-alloc in audio thread).
2. **Phase 1: Synthesize Findings & Create PROJECT.md / SCOPE.md**
   - Synthesize findings into concrete bugs and architecture requirements.
   - Record Feature Inventory and Milestones.
3. **Phase 2: Implementation (Worker)**
   - Dispatch `teamwork_preview_worker` to apply fixes using GitNexus impact analysis, adhering to C++20, zero warnings, zero allocations/locks in audio thread.
4. **Phase 3: Verification & Review (Reviewers, Challengers, Auditor)**
   - 2 Reviewers (`teamwork_preview_reviewer`)
   - 2 Challengers (`teamwork_preview_challenger`)
   - 1 Forensic Auditor (`teamwork_preview_auditor`)
5. **Phase 4: Gate Evaluation & Final Delivery**
   - Verify zero errors/warnings, passing tests, clean audit, and report to Parent.
