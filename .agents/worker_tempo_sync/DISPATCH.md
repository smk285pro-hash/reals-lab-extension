## 2026-08-30T19:46:14Z
You are the Worker Agent responsible for implementing the Audio Tempo Mismatch and 8-Point Playhead Phase Sync Fix.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\worker_tempo_sync (create this directory if needed, maintain progress.md and write handoff.md here).

You MUST read the authoritative request at:
c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md

Also read the scope document at:
c:\Users\smk28\Desktop\reals lab extension\PROJECT.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

CRITICAL USER RULES:
1. ALWAYS use GitNexus MCP tools (impact, context, query, detect_changes) before modifying any symbols.
2. Follow all guidelines in AGENTS.md, SPEC.md, PLAN.md, and DESIGN.md.
3. Adhere to C++20, zero compiler warnings on MSVC (`cmake --build --preset windows`), and all tests passing (`ctest --preset windows`).
4. Ensure zero allocations (new/malloc/vector::resize), zero disk I/O, and zero mutex locking inside `ReaperOnAudioBuffer` / `renderFrames` / `dsp_on_read` audio callback path.

DETAILED SYNTHESIS OF ROOT CAUSES & REQUIRED IMPLEMENTATION:

1. **Host Sample Rate Cold-Start & Atomic Target SR**:
   - In `core/include/reals/audio/Engine.h` & `core/src/audio/Engine.cpp`:
     - Make `targetSampleRate` an `std::atomic<int>`.
     - In `Engine::init(bool useDevice)`: If `useDevice && m_impl->engineInited`, initialize `targetSampleRate` with `ma_engine_get_sample_rate(&m_impl->engine)`.
   - In `extension/src/reaper_plugin.cpp`:
     - In `REAPER_PLUGIN_ENTRYPOINT` and `ExtHostActions` constructor/init, query `GetAudioDeviceInfo("SRATE", buf, sizeof(buf))` immediately on plugin load to seed `Engine::instance().setTargetSampleRate(devSr)`.
     - In `ReaperOnAudioBuffer`, ensure `setTargetSampleRate` call is lock-free and thread-safe.

2. **Frame Metric & Loop Boundary Alignment**:
   - In `core/src/audio/Engine.cpp`:
     - In `playFile`: When resampled to `targetSr`, update `m_impl->track.sampleRate = targetSr;` and `m_impl->track.totalFrames = static_cast<double>(m_impl->dspSource.totalFrames.load());`.
     - In `playFile`: In calculating `startFrame`, compare `nominalLoopFrames` against `m_impl->dspSource.totalFrames.load()` rather than un-resampled probe frames.
     - In `positionFraction()`, `seekFraction()`, and `level()`: Use `m_impl->dspSource.totalFrames.load()` as the reference total frames.
   - In `bridge/src/Bridge.cpp`:
     - In `audio.setSyncBpm` (lines 1111-1117): Compute `nominalLoopFrames` using effective sample rate: `const int effectiveSr = (eng.targetSampleRate() > 0) ? eng.targetSampleRate() : trk.sampleRate;` -> `eng.setLoopBoundaryFrames(static_cast<uint64_t>(nominalLoopSec * effectiveSr));`.

3. **Dynamic Project BPM in ExtHostActions**:
   - In `extension/src/reaper_plugin.cpp`:
     - Update `ExtHostActions::projectTempo()`: Check `g_liveTransport.bpm` first. If <= 30.0, query `GetPlayPosition2Ex` / `GetCursorPosition` and pass to `TimeMap_GetDividedBpmAtTime(playPos)`. Fallback to `Master_GetTempo()` only if time map returns <= 30.0.
   - In `bridge/src/Bridge.cpp`:
     - In `audio.play`: Check `transport.bpm` or `m_actions->projectTempo()` to ensure live tempo marker changes are respected.

4. **Audio Thread Realtime Safety (Point 8 Compliance)**:
   - In `core/src/audio/Engine.cpp`:
     - In `dsp_on_read`: Eliminate `std::recursive_mutex dspMutex` on the realtime callback path. Use atomic variables for cursor, active state, loop boundaries, and volume.
     - In `renderFrames`: Avoid per-block `std::vector::resize`. Use fixed-size stack/static arrays or pre-allocated buffers.
   - In `extension/src/reaper_plugin.cpp`:
     - In `ReaperOnAudioBuffer`: Eliminate per-block heap allocations / dynamic vector resizing (use pre-allocated fixed arrays up to max block size, e.g. 8192 frames).
     - Support mono master hardware outputs (`outR == nullptr`) gracefully by mixing `outL` alone.

5. **Discontinuity Handling & Tests**:
   - In `extension/src/reaper_plugin.cpp` & `core/src/audio/Engine.cpp`:
     - Handle timeline seek/loop discontinuity.
   - Update `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp` and other test suites to comprehensively verify:
     - Multi-rate decoding (44.1kHz audio on 48kHz and 96kHz host sample rate).
     - Accurate 1.0x pitch-neutral playback and tempo ratios.
     - Position fraction bounds and loop boundary accuracy.
     - Build with zero warnings and run `ctest --preset windows` to verify all tests pass 100%.
