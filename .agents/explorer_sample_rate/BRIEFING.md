# BRIEFING — 2026-08-31T02:44:00Z

## Mission
Investigate the entire audio pipeline regarding Sample Rate Conversion & Host Sample Rate handling in the REAPER extension and core audio engine.

## 🔒 My Identity
- Archetype: Teamwork explorer
- Roles: Audio Sample Rate & Pipeline Investigator
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_sample_rate
- Original parent: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Milestone: Audio Sample Rate & Pipeline Investigation

## 🔒 Key Constraints
- Read-only investigation — do NOT implement
- ALWAYS use GitNexus MCP tools for code exploration and symbol understanding
- Report exact file paths, line numbers, root causes, and recommended fixes

## Current Parent
- Conversation ID: 849229b8-a9a2-4bb6-b1f1-6bdc5c14257a
- Updated: 2026-08-31T02:44:00Z

## Investigation State
- **Explored paths**:
  - `core/include/reals/audio/Engine.h`
  - `core/src/audio/Engine.cpp`
  - `core/include/reals/audio/SoundTouchProcessor.h`
  - `core/src/audio/SoundTouchProcessor.cpp`
  - `core/include/reals/audio/DragExporter.h`
  - `core/src/audio/DragExporter.cpp`
  - `bridge/include/reals/bridge/Bridge.h`
  - `bridge/src/Bridge.cpp`
  - `extension/src/reaper_plugin.cpp`
  - `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp`
- **Key findings**:
  1. Uninitialized startup host sample rate (`targetSampleRate = 0`) causes fallback to native file sample rate, leading to 8.84% speedup and +1.47 semitone pitch shift on 48kHz ASIO devices if preview starts before audio hook.
  2. Frame unit mismatch between `track.totalFrames` (native file rate) and `dspSource.totalFrames` (host rate) causes `refFrames` in `playFile` to reject `nominalLoopFrames`, startFrame phase error, and waveform cursor desynchronization in `positionFraction()`.
  3. Dynamic BPM sync loop boundary in `Bridge.cpp:1114` computes `nominalLoopFrames` using `trk.sampleRate` instead of `targetSampleRate`, causing the loop to truncate 0.65s early every cycle at 48kHz.
  4. Standalone mode does not capture miniaudio device sample rate into `targetSampleRate`.
  5. REAPER extension does not query `GetAudioDeviceInfo("SRATE")` on plugin startup.
- **Unexplored areas**: None (Full audio pipeline traced from decoder to SoundTouch to hardware mixing hook).

## Key Decisions Made
- All findings documented with verbatim code references, line numbers, and exact mathematical impact.

## Artifact Index
- handoff.md — Authoritative 5-component report
- progress.md — Heartbeat and progress log
- DISPATCH.md — Initial mission dispatch
