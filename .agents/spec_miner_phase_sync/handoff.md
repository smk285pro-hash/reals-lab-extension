# SPECIFICATION MINING & AUDIT REPORT: REAPER 8-Point Playhead Phase Sync

**Investigator:** Spec Miner (`spec_miner_phase_sync`)  
**Parent Orchestrator:** `849229b8-a9a2-4bb6-b1f1-6bdc5c14257a`  
**Date:** 2026-08-30T19:45:00Z  
**Target Codebase:** `extension/src/reaper_plugin.cpp`, `core/src/audio/Engine.cpp`, `bridge/src/Bridge.cpp`, `core/src/audio/SoundTouchProcessor.cpp`, `core/src/audio/DragExporter.cpp`

---

## Executive Summary

An exhaustive audit of the audio pipeline against the **8-Point Playhead Phase Sync Master Specification** was conducted.

### Core Discoveries:
1. **Root Cause of Tempo Speedup & Phase Lag (Points 1 & 4):**
   - **Sample Rate / Frame Scaling Mismatch in `Engine.cpp`:** When previewing a 44.1kHz sample on a 48kHz (or 96kHz) REAPER ASIO host, miniaudio correctly decodes and resamples the audio into `dspSource.pcmData` (96,000 frames for 2.0s). `nominalLoopFrames` is computed at 48,000Hz (96,000 frames). However, in `Engine::playFile` (lines 533–536) and `Engine::positionFraction()` (lines 955–959), `nominalLoopFrames` is compared against `m_impl->track.totalFrames` (88,200 frames from the un-resampled native file probe). Because `96000 <= 88200` evaluates to **FALSE**, `refFrames` and `denom` erroneously fall back to 88,200! This causes an immediate phase distortion of up to **162.5ms** and causes `positionFraction()` to overshoot 1.0 (reaching 1.088).
   - **Uninitialized / Fallback Target Sample Rate:** When `Bridge::init()` starts before `ReaperOnAudioBuffer` has executed, `Engine::targetSampleRate` is `0`. If an audio preview is triggered before the first audio buffer callback sets `srate`, the decoder decodes at 44.1kHz. When `ReaperOnAudioBuffer` subsequently pulls 48,000 frames/sec, the 44.1kHz buffer is consumed at **1.0884x speed (+8.84% faster, with audible pitch shift)**.
2. **Discontinuity Handling & Active Phase Lock Defect (Points 1, 5, 7):**
   - `ReaperOnAudioBuffer` detects timeline jumps (`discontinuityCounter++`), but `Engine` never listens to this counter or re-aligns its cursor. When REAPER loops or seeks during playback, the preview desynchronizes permanently.
   - The Proportional-Integral (PI) phase controller specified in Point 7 does not exist in the codebase.
3. **Audio Thread Safety Violations (Points 2 & 8):**
   - `dsp_on_read` takes `std::recursive_mutex dspMutex` on every audio callback.
   - `ReaperOnAudioBuffer`, `renderFrames`, and `dsp_on_read` contain `thread_local std::vector::resize` and dynamic allocations.
   - `setTargetSampleRate` writes to a non-atomic `int` from the audio thread.

---

## 1. 8-Point Compliance Matrix

| # | Master Spec Point | Status | Observed Gap / Violation | Action Required |
|---|---|---|---|---|
| **1** | **Goal: Beat Phase Sync** | ⚠️ PARTIALLY COMPLIANT | `Engine.cpp` compares resampled `nominalLoopFrames` with native `track.totalFrames`. Cursor drifts on seek/loop. | Use `dspSource.totalFrames` instead of `track.totalFrames`; link discontinuity to cursor re-sync. |
| **2** | **Two-Tier Access (Main vs Audio Hook)** | ❌ NON-COMPLIANT | `ReaperOnAudioBuffer` calls `setTargetSampleRate` (allocating if null, non-atomic int); `renderFrames` locks mutex. | Make `targetSampleRate` atomic; eliminate mutex and allocations on audio hook. |
| **3** | **Registration & Hardware Buffers** | ✅ FULLY COMPLIANT | Correct `Audio_RegHardwareHook(true, &hook)` / `Audio_RegHardwareHook(false, &hook)`. Double ReaSample mixing in `isPost`. | Minor: Handle single-channel / mono master outputs gracefully. |
| **4** | **Position & Tempo APIs** | ✅ FULLY COMPLIANT | Calls `GetPlayPosition2Ex(proj)`, `TimeMap_GetDividedBpmAtTime`, `TimeMap2_timeToBeats`. | None (API signatures and usage are correct). |
| **5** | **Phase Calculation & Discontinuity Loop** | ⚠️ PARTIALLY COMPLIANT | `discontinuityCounter` is incremented on `fabs(delta - expectedDelta) > 0.01`, but never consumed or acted upon. | Notify `Engine` on discontinuity to trigger instant hard re-anchor. |
| **6** | **Thread Communication (Atomics)** | ⚠️ MOSTLY COMPLIANT | `LiveAudioTransportState` uses relaxed atomics. `targetSampleRate` is non-atomic; `dspMutex` is used across threads. | Convert all cross-thread variables to atomics; remove `dspMutex` from realtime read path. |
| **7** | **Phase Compensation (PI / Hard Seek)** | ❌ NON-COMPLIANT | PI controller completely missing; hard seek only occurs once at initial `playFile`. | Implement PI micro-tempo adjustment ($\pm 0.5\%$) and hard-seek on seek/loop wrap. |
| **8** | **Audio Safety (Zero Alloc / IO / Mutex)** | ❌ NON-COMPLIANT | File I/O is compliant (zero disk reads in audio hook). Mutex and `std::vector::resize` violate realtime safety. | Pre-allocate fixed buffers (8192 floats); remove `dspMutex` in `dsp_on_read`. |

---

## 2. Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|---|---|---|---|---|---|---|
| 1 | Sync | Late Phase Anchor | Deferred transport query immediately before audio seek/start to eliminate decode-time phase lag | `startFraction` (double) | `clampedFraction` (double) | Returns 0.0 if DAW stopped | `core/src/audio/Engine.cpp:514` |
| 2 | Sync | Bar Quantizer | Snaps arbitrary file duration to musical bars (0.25 to 64 bars) with tail allowance | `durationSeconds`, `sampleBpm` | `loopBeats` (double) | Fallback to nearest integer beat | `bridge/src/Bridge.cpp:871-903` |
| 3 | Sync | Nominal Loop Boundary | Enforces seamless loop wrap at exact musical beat boundary instead of file end | `nominalLoopFrames` (uint64) | `effectiveLoopFrames` | Defaults to `totalFrames` if 0 | `core/src/audio/Engine.cpp:140-142` |
| 4 | Sync | Live Re-Phase | Dynamically enables/disables BPM sync on running audio track | `audio.setSyncBpm` (JSON) | `syncState` event | Graceful fallback if no BPM detected | `bridge/src/Bridge.cpp:1035-1138` |
| 5 | DSP | Low-Latency SoundTouch | SoundTouch profile with 20ms sequence, 8ms seek, 4ms overlap for <30ms latency | `lowLatency = true` | Latency ~28ms (44.1k) | Falls back to high-fidelity (40ms) if false | `core/src/audio/SoundTouchProcessor.cpp:17-36` |
| 6 | DSP | Pre-roll Latency Priming | Primes SoundTouch with initial latency frames on startup to eliminate initial silence | `latencyFrames()` | Primed PCM frames | Skips if bypass mode | `core/src/audio/Engine.cpp:546-564` |
| 7 | REAPER | Hardware Output Mix | Post-fader addition into REAPER 64-bit `ReaSample` master hardware buffer | `reg->GetBuffer(true, 0/1)` | Mixed audio on master | No-op if buffers null | `extension/src/reaper_plugin.cpp:402-421` |
| 8 | REAPER | Live Transport Atom | Lock-free audio hook transport publisher for UI & Bridge | `GetPlayPosition2Ex`, `TimeMap2` | `g_liveTransport` atomics | Fallback to `GetCursorPosition` if stopped | `extension/src/reaper_plugin.cpp:348-397` |
| 9 | Drag | Native Take Pitch/Playrate Sync (Mechanism A) | Injects `D_PLAYRATE`, `B_PPITCH`, `D_PITCH`, `D_LENGTH` onto imported REAPER take | `queuePendingPlayrate` | Synchronized REAPER take | Erased after 60s timeout | `extension/src/reaper_plugin.cpp:205-221` |
| 10 | Drag | Ghost Item Swap (Mechanism C) | Drops dummy WAV then hot-swaps `P_SOURCE` with original media file | `PCM_Source_CreateFromFileEx` | Lossless native take | Deletes old source | `extension/src/reaper_plugin.cpp:168-189` |

---

## 3. Edge Cases

| # | Feature | Input | Observed Behavior |
|---|---|---|---|
| 1 | Sample Rate Conversion | 44.1kHz audio played on 48kHz ASIO device | Decodes to 48kHz, but `nominalLoopFrames <= totalF` fails due to comparing 96000 against 88200. `startFrame` miscalculates and `positionFraction()` overshoots 1.0. |
| 2 | Host Sample Rate Detection | `playFile` called before `ReaperOnAudioBuffer` fires | `targetSampleRate` is 0; file decoded at 44.1kHz. When 48kHz ASIO starts, audio plays 1.0884x faster (8.84% speedup). |
| 3 | REAPER Seek / Loop Wrap | User clicks timeline or loop region wraps | `discontinuityCounter` increments, but preview continues sequential cursor playback; falls out of sync. |
| 4 | Mono Master Hardware | REAPER hardware output with 1 channel (`outR == nullptr`) | `if (outL && outR)` condition fails; preview audio is completely silent. |
| 5 | Non-Power-of-Two Loop | 12-bar blues loop (48 beats) or 3-bar loop (12 beats) | Bar Quantizer correctly matches 12-bar (48 beats) via `kStandardBars`. |
| 6 | Reverb Tail Loop | 4-bar loop (16 beats = 8.0s) with 0.8s tail (8.8s total) | Bar Quantizer matches 16.0 beats via tail allowance; `loopBoundaryFrames` set to 352,800 frames. |
| 7 | SoundTouch Latency Step | Rapid tempo ratio change (e.g. 1.0x to 1.5x) | Applied lock-free at block boundary; no audible discontinuity or audio thread stall. |
| 8 | Audio Thread Allocation | First audio buffer callback with `len = 1024` | `tempL.resize(1024)` performs dynamic allocation on audio thread. |

---

## 4. Five-Component Handoff Report

### 1. Observation
- **`core/src/audio/Engine.cpp:533-536`**:
  ```cpp
  const ma_uint64 totalF = static_cast<ma_uint64>(m_impl->track.totalFrames);
  const ma_uint64 refFrames = (nominalLoopFrames > 0 && nominalLoopFrames <= totalF)
      ? nominalLoopFrames
      : totalF;
  ```
  `m_impl->track` is populated by `probeFile(path)` which reads native file properties (e.g. 88,200 frames @ 44.1kHz). Meanwhile, `nominalLoopFrames` is computed at `targetSr` (e.g. 96,000 frames @ 48kHz). When `targetSr > nativeSr`, `nominalLoopFrames <= totalF` fails, corrupting `refFrames` to 88,200.
- **`core/src/audio/Engine.cpp:955-959`**:
  ```cpp
  const ma_uint64 bound = m_impl->dspSource.loopBoundaryFrames.load();
  const double denom = (bound > 0 && bound <= static_cast<ma_uint64>(m_impl->track.totalFrames))
      ? static_cast<double>(bound)
      : m_impl->track.totalFrames;
  ```
  `bound` (96,000) > `track.totalFrames` (88,200), so `denom` is set to 88,200. `cursorFrames / denom` overshoots to 1.088.
- **`extension/src/reaper_plugin.cpp:343` & `core/src/audio/Engine.cpp:364-367`**:
  ```cpp
  void Engine::setTargetSampleRate(const int sampleRate) {
      if (!m_impl) m_impl = std::make_unique<Impl>();
      m_impl->targetSampleRate = sampleRate;
  }
  ```
  Called from `ReaperOnAudioBuffer` on the audio thread. `m_impl->targetSampleRate` is a plain `int`, unprotected by atomic or lock-free synchronization.
- **`extension/src/reaper_plugin.cpp:383-390`**:
  `g_liveTransport.discontinuityCounter.fetch_add(1)` is executed when `fabs(delta - expectedDelta) > 0.01`, but `Engine` has no listener or mechanism to receive this event during continuous playback.
- **`core/src/audio/Engine.cpp:106`**:
  `dsp_on_read` executes `std::lock_guard lock(ds->dspMutex)` on the audio callback thread.
- **`extension/src/reaper_plugin.cpp:405-408`** & **`core/src/audio/Engine.cpp:971-973`**:
  `thread_local std::vector<float>` calls `.resize(len)` on the audio thread.

### 2. Logic Chain
1. A sample file encoded at 44.1kHz is played inside REAPER operating with a 48kHz ASIO device.
2. `Engine::playFile` initializes miniaudio decoder with `targetSr = 48000`. The decoded buffer in RAM has 96,000 frames for 2.0s.
3. `Bridge` calculates `nominalLoopFrames = 2.0 * 48000 = 96000`.
4. `probeFile` returned 88,200 frames. In `playFile:533`, `96000 <= 88200` evaluates to `false`. `refFrames` becomes 88,200.
5. `startFrame` is computed as `startFraction * 88200` instead of `startFraction * 96000`. The start offset has an error of `7800 * startFraction` frames (up to 162.5ms offset).
6. If `playFile` was called when REAPER audio engine had not yet delivered its first callback (`targetSampleRate == 0`), the file is decoded at 44.1kHz. `ReaperOnAudioBuffer` pulls 48,000 frames/sec from this 44.1kHz buffer, producing an **8.84% speedup** and high pitch.
7. If REAPER seeks or loops during playback, `discontinuityCounter` increments, but `dspSource.cursorFrames` advances without re-anchoring, causing immediate and permanent phase desynchronization.
8. Mutex locking (`dspMutex`) and memory resizing (`std::vector::resize`) in `dsp_on_read` / `renderFrames` violate realtime safety (Point 8).

### 3. Caveats
- REAPER hardware hook tests require a host or the comprehensive mock host harness (`MockHostActions` in `TestSuite_PhaseSyncDiagnostics.cpp`).
- SoundTouch low-latency mode introduces ~28ms algorithmic pipeline latency (at 44.1kHz) or ~25ms (at 48kHz), which is primed on playback start.

### 4. Conclusion
The tempo speedup and playhead phase sync issues stem from:
1. Frame count reference discrepancies between native probe metadata (`track.totalFrames`) and resampled PCM buffers (`dspSource.totalFrames`).
2. Race condition / fallback to 0 in `targetSampleRate` before first audio hook callback.
3. Unhandled discontinuity signals during active playback (missing seek re-anchor and PI controller).
4. Audio thread mutex locking and heap vector resizing.

### 5. Verification Method
1. Build check: `cmake --build --preset windows` (MSVC, zero errors, zero warnings).
2. Test suite check: `ctest --preset windows --output-on-failure`.
3. Verify test cases in `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp` and `tests/suites/TestSuite_SoundTouchCore.cpp`.
