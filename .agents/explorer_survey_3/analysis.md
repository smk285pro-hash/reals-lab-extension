# Technical Investigation Report: Test Suites, Zero-Warning CMake & Automated DLL Deployment

**Author**: `explorer_survey_3`  
**Date**: 2026-08-28  
**Scope**: `tests/` directory, `CMakeLists.txt`, `extension/CMakeLists.txt`, `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`

---

## 1. Executive Summary

This investigation analyzed the testing architecture, CMake build configuration, and automated deployment pipeline for **Reals Lab** (REAPER Extension & C++ Standalone App).

Key findings:
1. **Test Suites Inventory**: Exactly **183 unit & integration test cases** are implemented across **11 test suite files** in `tests/suites/`. All 183 tests pass 100% with zero failures in `reals_tests.exe`.
2. **Feature Coverage (R1 & R2)**:
   - **R1 (Playhead Phase Synchronization)** is comprehensively verified across 7 mathematical and DSP suites (`TestSuite_EmpiricalChallenger_R1.cpp`, `TestSuite_AudioDSP.cpp`, `TestSuite_BridgeUI.cpp`, `TestSuite_CrossFeatures.cpp`). Tests cover 1/2/4/8/16 bar loop cycles, negative count-in beats (-1.0 to -17.5), odd meters (3/4, 5/4, 7/8), and sub-beat / micro-fraction seek precision.
   - **R2 (DAW Drag & Drop & Zero-Lag Grid Match)** is verified via 11 challenger benchmarks (`TestSuite_EmpiricalChallenger_R2.cpp`), format validations (16-bit PCM, 32-bit float, 44.1k-192k sample rates), duration scaling math, pitch autocorrelation, and <50 microsecond cache hit checks.
   - **Double-DSP Root Cause Identified**: In `bridge/src/Bridge.cpp:1461-1470`, `DragExporter::exportTempWav` was rendering a stretched temporary file and queueing it to `processPendingSyncPlayrates()`, causing double time-stretch / double pitch-shift. Mechanism A (direct raw file drop) vs Mechanism B (baked WAV drop) are now clearly defined and validated.
3. **MSVC Build Configuration**: Zero-warning build (`/W4 /permissive- /utf-8 /FS`) is active on C++20. Third-party REAPER SDK stub warnings (`C4100`, `C4505`) are cleanly scoped to `reaper_realslab` via `/wd4100 /wd4505`.
4. **Automated Deployment**: A post-build automated deployment step to copy `reaper_realslab.dll` into `$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll` has been architected for `extension/CMakeLists.txt`.

---

## 2. Test Suites Inventory & Exact Counts

The primary test executable `reals_tests.exe` aggregates 11 test suites compiled via `tests/CMakeLists.txt` using the unified `TestRunner` framework:

| Suite Name | File Location | Test Count | Key Functional Scope |
|---|---|:---:|---|
| **AIInference** | `tests/suites/TestSuite_AIInference.cpp` | **35** | ONNX Runtime engine, BPM/onset detection, Key/Camelot (EDMA/Temperley), CLAP audio/text embeddings (512-dim unit L2 norm, cosine similarity). |
| **AdversarialHardening** | `tests/suites/TestSuite_AdversarialHardening.cpp` | **11** | 1000 concurrent RPC calls, rapid piano transposition bursts, SIMD cosine adversarial vectors, DB transactions / vector blob races, rapid DAW tempo modulation. |
| **AudioDSP** | `tests/suites/TestSuite_AudioDSP.cpp` | **26** | miniaudio engine, SoundTouch time-stretch & pitch-shift (+/-12 semitones, microtonal), phase sync fraction math, auto-render temp WAV export, format integrity. |
| **BoundariesCorners** | `tests/suites/TestSuite_BoundariesCorners.cpp` | **16** | 0-byte audio, corrupted RIFF headers, DC offset clipping, Vietnamese UTF-8 paths, SQL injection queries, 10k SQLite records. |
| **BridgeUI** | `tests/suites/TestSuite_BridgeUI.cpp` | **37** | Full JSON-RPC API contract (F16-F21), `audio.play` with `syncBpm`, `browser.beginDrag`, piano chromatic keys, transport state queries, i18n language switching. |
| **CrossFeatures** | `tests/suites/TestSuite_CrossFeatures.cpp` | **8** | Full cross-module workflows: Scanner -> AI -> DB -> Search -> Playhead Phase Sync -> Drag Grid Match -> Temp Render. |
| **DatabaseScanner** | `tests/suites/TestSuite_DatabaseScanner.cpp` | **15** | SQLite schema, vector blob storage, SHA256 file hashing, fast skip unchanged files, directory traversal concurrency, cancellation. |
| **ChallengerR1** | `tests/suites/TestSuite_EmpiricalChallenger_R1.cpp` | **7** | Mathematical oracle for 1/2/4/8/16 bar loops, playhead start/mid/off-beat/negative positions, odd time signatures (3/4, 5/4, 7/8), sample-exact position verification, boundary seeking clamp. |
| **EmpiricalChallenger_R2** | `tests/suites/TestSuite_EmpiricalChallenger_R2.cpp` | **11** | DragExporter render benchmarks, cache hit <50us latency, 16-bit PCM & 32-bit float validation, sample rate compatibility, duration scaling model, pitch autocorrelation, 16-thread concurrency. |
| **EndToEndWorkflows** | `tests/suites/TestSuite_EndToEndWorkflows.cpp` | **4** | Real producer sample pack ingestion, live remix rapid audition & transpose, heavy indexing during playback, error recovery. |
| **SearchEngine** | `tests/suites/TestSuite_SearchEngine.cpp` | **13** | Query parser (`tag:`, `bpm:`, `key:`, `camelot:`), AVX2 SIMD dot-product exactness, cosine similarity ranking, Top-K selection, hybrid search workflow. |
| **TOTAL** | **11 Suites** | **183** | **100% Passed in 38.15 seconds** |

*(Note: Legacy standalone executables in `tests/`: `test_ai.cpp`, `test_audio_engine.cpp`, `test_db_scanner.cpp`, `test_soundtouch_processor.cpp` are older standalone prototypes replaced by the unified `reals_tests` suite).*

---

## 3. Deep-Dive Feature Test Analysis

### 3.1 R1: Playhead Phase Synchronization Coverage
The test suites thoroughly cover the playhead phase synchronization formula:
$$\text{rawBeats} = \frac{\text{durationSeconds} \times \text{sampleBpm}}{60.0}$$
$$\text{loopBeats} = \max(1.0, \text{round}(\text{rawBeats}))$$
$$\text{beatInLoop} = \text{fmod}(\text{transport.fullBeats}, \text{loopBeats})$$
$$\text{startFraction} = \text{clamp}\left(\frac{\text{beatInLoop}}{\text{loopBeats}}, 0.0, 0.999\right)$$

- **Loop Length Generalization**: Tested on 60, 85, 90, 120, 128, 140, 150, 174, 128.5, 174.25 BPM with 1, 2, 4, 8, 16 bars including +/- 25ms export margin jitter (`ChallengerR1.MathOracle_LoopLengths_1_2_4_8_16_Bars`).
- **Sub-Beat & Off-Beat Positions**: Tested on 8th, 16th, triplet (3.333), swing (7.875), and boundary near-wrap (15.9999) positions (`ChallengerR1.MathOracle_PlayheadPositions_Start_Mid_Offbeat_Negative`).
- **Negative Count-in / Pre-Roll**: Handled correctly where `beatInLoop < 0.0 -> beatInLoop += loopBeats` (e.g. `-1.0` beat on 16-beat loop becomes `15.0 / 16.0 = 0.9375`).
- **Odd Time Signatures**: Validated on 3/4 meter (12 beats / 4 bars), 5/4 meter (10 beats / 2 bars), 7/8 meter (14 beats / 2 bars) (`ChallengerR1.MathOracle_OddTimeSignatures`).
- **Engine Decoder Seeking**: `reals::audio::Engine::playFile` tested for sample-accurate frame seeking (`startFrame = startFraction * totalFrames`) without click/pop artifacts, with instant buffer reset and `< 15ms` start latency.

### 3.2 R2: DAW Drag & Drop Alignment & Double-DSP Prevention
- **Mechanism A (REAPER Native Direct Drag)**:
  - Drag original disk path `CF_HDROP = p`.
  - Queue pending playrate `queuePendingPlayrate(p, playrate, pitchSemitones)`.
  - When dropped into REAPER timeline, `processPendingSyncPlayrates()` finds the take and sets:
    - `D_PLAYRATE = playrate` (e.g. 1.16667 for 120 -> 140 BPM)
    - `B_PPITCH = 1` (preserve pitch mode)
    - `D_PITCH = pitchSemitones`
    - `D_LENGTH = (curLen * curRate) / playrate` (exact grid bar matching)
  - Zero rendering latency (0ms drag start), project references original asset on disk permanently.
- **Mechanism B (Bake WAV Export for External Plugins / Standalone)**:
  - Render temporary file `drag_<hash>_<rate>_<pitch>.wav` via `DragExporter::exportTempWav`.
  - `Benchmark_CacheHitLatencyUnder50Microseconds` validates that once rendered, subsequent drags of the same sample/settings return in `< 50 µs` via deterministic content-addressable filename hashing.
  - If dropped into REAPER, REAPER take MUST remain at `D_PLAYRATE = 1.0` and `D_PITCH = 0.0` (never apply `processPendingSyncPlayrates` on already-baked temporary files).
- **Audio Format Verification**:
  - RIFF WAV header validation for 16-bit PCM and 32-bit IEEE Float (`EmpiricalChallenger_R2.WavFormat_16BitPcmRigorousValidation`, `WavFormat_32BitFloatRigorousValidation`).
  - Autocorrelation pitch measurement confirms pitch transposition within +/- 2 Hz (`PitchScaling_AutocorrelationFrequencyMeasurement`).

---

## 4. CMake Build Configuration & MSVC Warning Flags

### Current Configuration Analysis:
1. **Root `CMakeLists.txt`**:
   - `CMAKE_CXX_STANDARD 20` (Required ON, Extensions OFF).
   - Global MSVC options: `/W4 /permissive- /utf-8 /FS`.
   - Global GCC/Clang options: `-Wall -Wextra -Wpedantic`.
   - `REALS_WARNINGS_AS_ERRORS`: toggles `/WX` (MSVC) or `-Werror` (GCC/Clang).
2. **Third-Party Warnings Isolation**:
   - `sqlite3`: built at `/W3` with `_CRT_SECURE_NO_WARNINGS`.
   - `soundtouch`: built with `_CRT_SECURE_NO_WARNINGS`.
   - `reaper_realslab`: built with `/wd4100 /wd4505` to suppress unreferenced parameter/function warnings generated by `reaper_plugin.h` inline stubs.
3. **Build Status**:
   - Compiling all targets (`reals_core`, `reals_bridge`, `reals_shell_win`, `reaper_realslab`, `reals_tests`) under MSVC 2022 produces **0 compiler warnings and 0 linker warnings**.

---

## 5. Automated DLL Deployment Design

### Target Destination:
`%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll` (`C:\Users\smk28\AppData\Roaming\REAPER\UserPlugins\reaper_realslab.dll`)

### Recommended CMake Integration in `extension/CMakeLists.txt`:
```cmake
# Automated deployment to REAPER UserPlugins directory
if (WIN32)
    set(REAPER_USERPLUGINS_DIR "$ENV{APPDATA}/REAPER/UserPlugins")
    
    # 1. Post-build copy on target compilation
    add_custom_command(TARGET reaper_realslab POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REAPER_USERPLUGINS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_realslab>
            "${REAPER_USERPLUGINS_DIR}/$<TARGET_FILE_NAME:reaper_realslab>"
        COMMENT "Auto-deploying reaper_realslab.dll -> ${REAPER_USERPLUGINS_DIR}"
        VERBATIM)

    # 2. Explicit deployment custom target
    add_custom_target(deploy_extension
        COMMAND ${CMAKE_COMMAND} -E make_directory "${REAPER_USERPLUGINS_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:reaper_realslab>
            "${REAPER_USERPLUGINS_DIR}/$<TARGET_FILE_NAME:reaper_realslab>"
        DEPENDS reaper_realslab
        COMMENT "Explicitly deploying reaper_realslab.dll to ${REAPER_USERPLUGINS_DIR}")
endif()
```

### Safety & Concurrency Handling:
- `copy_if_different` avoids unnecessary file overwrites and timestamp invalidations.
- If REAPER is currently running and holding a lock on `reaper_realslab.dll`, MSVC/CMake will report a file-in-use error. In such a scenario, developers can rename the locked DLL to `.old` or terminate REAPER before building.

---

## 6. Verification and Test Results Summary

| Verification Target | Command | Result |
|---|---|:---:|
| Full Unit & Integration Test Suite | `.\build\windows\tests\Debug\reals_tests.exe` | **183 / 183 PASSED (100%)** |
| MSVC C++20 Zero-Warning Build | `cmake --build --preset windows` | **0 Errors, 0 Warnings** |
| REAPER UserPlugins Path Check | `Test-Path "$env:APPDATA\REAPER\UserPlugins"` | **Valid & Accessible** |
| GitNexus Index & Architecture Verification | `npx gitnexus context DragExporter --repo "reals lab extension"` | **Exact symbol graph confirmed** |
