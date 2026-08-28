# Forensic Audit Report — Reals Lab

**Work Product**: `bridge/src/Bridge.cpp`, `extension/src/reaper_plugin.cpp`, `core/src/ai/FeatureExtractor.cpp`, `core/src/audio/Engine.cpp`, `extension/CMakeLists.txt`, `tests/suites/`, `tests/framework/`  
**Profile**: General Project (Forensic Integrity)  
**Integrity Mode**: Development (Mode inferred from `ORIGINAL_REQUEST.md`)  
**Verdict**: `CLEAN`

---

## 1. Executive Summary

A comprehensive forensic audit was conducted on the Reals Lab codebase to verify the authenticity, algorithmic integrity, and performance of:
1. **Playhead Phase Synchronization** (DAW preview sync based on real REAPER SDK transport and SoundTouch seek fraction).
2. **DAW Drag & Drop Alignment (Mechanism A & Mechanism B)** (Native CF_HDROP routing, zero-lag drag start, Take playrate/pitch/length grid alignment, and safeguard against double-DSP processing).
3. **Core DSP and AI Analysis** (Authentic Cooley-Tukey Radix-2 FFT, Mel Filterbank, Chromagram peak picking, and miniaudio/SoundTouch engine).
4. **Build & Test Suite Execution** (Zero-warning MSVC C++20 build and 100% CTest pass rate).
5. **Automated DLL Deployment** (`%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`).

**Audit Finding**: Zero facade implementations, zero hardcoded test mocks, and zero integrity violations detected. The algorithms and architecture strictly adhere to the requirements specified in `ORIGINAL_REQUEST.md` and `PROJECT.md`.

---

## 2. Phase 1: Source Code & Integrity Inspection

| # | Check Item | Status | Forensic Observation |
|---|------------|--------|----------------------|
| 1 | **Hardcoded / Cheating Outputs** | **PASS** | No fake or pre-canned PASS/FAIL strings or mock DSP math. Phase sync formula `startFraction = std::clamp(std::fmod(fullBeats, loopBeats) / loopBeats, 0.0, 0.999)` and `loopBeats = std::max(1.0, std::round((duration * sampleBpm) / 60.0))` are computed dynamically from real audio file probes and REAPER SDK `TimeMap2_timeToBeats`. |
| 2 | **Facade / Dummy Implementations** | **PASS** | `Bridge.cpp`, `reaper_plugin.cpp`, `Engine.cpp`, and `FeatureExtractor.cpp` implement full genuine logic with thread-safe synchronization (`std::lock_guard`, atomics, lock-free queues), full error handling, and robust memory management. |
| 3 | **Pre-populated Result Artifacts** | **PASS** | No pre-generated logs or falsified test outputs were present. All logs and databases are generated on-the-fly during test runs in isolated temporary folders. |
| 4 | **Mechanism A (Native CF_HDROP Drag)** | **PASS** | `browser.beginDrag` in `Bridge.cpp` directly passes the original sample path `p` to `m_actions->beginDrag(p)` and queues `m_actions->queueSyncPlayrate(p, playrate, pitchShift)`. Synchronous offline rendering on drag initiation is completely removed, achieving 0ms drag latency. |
| 5 | **Mechanism B Safeguard** | **PASS** | In `reaper_plugin.cpp` (`processPendingSyncPlayrates`), any item containing `drag_` or `drag_export` is identified as pre-baked and explicitly reset to `D_PLAYRATE = 1.0` and `D_PITCH = 0.0`, eliminating double time-stretch and double pitch-shift. |
| 6 | **Take Alignment Grid Math** | **PASS** | `reaper_plugin.cpp` adjusts media item boundary on the timeline via `SetMediaItemInfo_Value(item, "D_LENGTH", (curLen * curRate) / it->playrate)`, ensuring perfect grid alignment with the project tempo. |
| 7 | **Chromagram Local Peak Picking** | **PASS** | `FeatureExtractor.cpp` (`computeChromagram`) implements local peak picking (`if (mag < stft[t][k - 1] || mag < stft[t][k + 1]) continue;`) to prevent Hann window spectral leakage across adjacent pitch classes. |

---

## 3. Phase 2: Behavioral & Build Verification

### 3.1 MSVC C++20 Compilation
- **Command**: `cmake --build --preset windows`
- **Result**: **0 Warnings, 0 Errors**
- **Targets Built**:
  - `reals_core.lib`
  - `reals_bridge.lib`
  - `reals_shell_win.lib`
  - `reaper_realslab.dll`
  - `reals_tests.exe`
  - Auxiliary test executables (`test_ai.exe`, `test_audio_engine.exe`, `test_db_scanner.exe`, `test_soundtouch_processor.exe`)

### 3.2 Automated Test Execution
- **CTest Preset**: `ctest --preset windows -C Debug --output-on-failure`
  - `test_soundtouch_processor`: **PASSED** (1.19s)
  - `test_audio_engine`: **PASSED** (0.17s)
  - `test_ai`: **PASSED** (3.73s)
  - `test_db_scanner`: **PASSED** (0.24s)
  - `reals_e2e_tests`: **PASSED** (57.15s)
  - **Overall CTest Pass Rate**: **100% (5/5 suites)**
- **Test Coverage**: 183 automated test cases covering RPC contracts, DSP phase sync, OLE drag alignment, database transactions, background scanning, ONNX AI embeddings, SIMD similarity matching, and end-to-end user workflows.

### 3.3 DLL Deployment Verification
- **Target Path**: `C:\Users\smk28\AppData\Roaming\REAPER\UserPlugins\reaper_realslab.dll`
- **File Size**: `2,503,680 bytes`
- **Deployment Mechanism**: CMake `POST_BUILD` command with atomic PowerShell file move/copy.
- **Status**: **VERIFIED ACTIVE & UPDATED**.

---

## 4. Final Verdict

**VERDICT: `CLEAN`**

The implementation is authentic, robust, zero-warning compliant, and fully verified against all empirical tests and user constraints.
