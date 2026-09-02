# Project: Reals Lab — Audio Preview, Transposition & Verification Master Plan

## Architecture
- **Audio Core (`core/src/audio/`)**:
  - `Engine.cpp`: Miniaudio decoder (`ma_format_f32`, uniform stereo, `lpfOrder = 4` Butterworth filter), lock-free atomic parameter updates, dual-channel RAM buffering.
  - `SoundTouchProcessor.cpp`: SoundTouch DSP wrapper (`SETTING_USE_AA_FILTER = 1`, `SETTING_USE_QUICKSEEK = 0`, low-latency 20/8/6ms 32-tap preview, Studio Master 82/28/12ms 64-tap Sinc filter).
  - `DragExporter.cpp`: Offline WAV export generator with pitch/tempo stretch in Studio Master profile (`lowLatency = false`).
- **Extension Shell (`extension/src/reaper_plugin.cpp`)**:
  - Direct 64-bit ASIO master output mixing via `Audio_RegHardwareHook`, bypassing Windows WASAPI endpoint.
  - OLE native drag-and-drop take property injection (`D_PLAYRATE`, `B_PPITCH = 1`, `D_PITCH`, grid length matching).
- **Bridge Dispatcher (`bridge/src/Bridge.cpp`)**:
  - RPC handler between WebView2/CEF and C++ backend.
  - Immediate `pitchSemitones` payload dispatch in `audio.play`.
  - Batch metadata hydration in `fs.list` via `Database::getSamplesByPaths()`.
- **UI State Machine (`ui-web/app.js`)**:
  - `state.isUserTargetKeyLocked` invariant protecting `state.userTargetNote`.
  - Circular semitone distance calculation (`calculateSemitoneDistance`).
- **Test Infrastructure (`tests/`)**:
  - Custom C++20 header-only test runner (`TestRunner.h`) with 334 test cases across 23 suites.

---

## Feature Inventory
| # | Feature | Description | Milestone | Status |
|---|---------|-------------|-----------|--------|
| 1 | `ma_decoder` Butterworth LPF Resampling | 4th-order Butterworth anti-aliasing filter (`lpfOrder = 4`), stereo float32 buffering across all mono/stereo files | M1 | DONE |
| 2 | SoundTouch DSP Anti-Aliasing & Precision | `SETTING_USE_AA_FILTER = 1`, `SETTING_USE_QUICKSEEK = 0`, 64-tap Sinc filter & low-latency profile | M1 | DONE |
| 3 | REAPER `Audio_RegHardwareHook` Direct 64-bit ASIO Mixing | Direct master hook mixing (`Engine::instance().init(false)`), eliminating WASAPI degradation | M1 | DONE |
| 4 | `state.isUserTargetKeyLocked` Invariant | Strict preservation of `state.userTargetNote` across sample selection, async events, and metadata hydration | M2 | DONE |
| 5 | Semitone Distance & Glitch-Free Audio/Drag | Exact semitone shift calculation, immediate payload passing in `audio.play` and `browser.beginDrag` | M2 | DONE |
| 6 | SQLite Metadata Hydration in `fs.list` | 400-path chunked batch hydration in `Bridge.cpp` via `Database::getSamplesByPaths()` | M2 | DONE |
| 7 | Automated Test Suite Pass Rate | 100% pass across all 334 test cases in `reals_tests.exe` (and Debug timing threshold alignment) | M3 | DONE |
| 8 | MSVC Zero-Warning Build | Strict `/W4` MSVC build with 0 warnings across all targets | M3 | DONE |
| 9 | Critical Invariants Documentation | Inline `CRIT-*` comments and synchronization with `PLAN.md` & `SPEC.md` | M3 | DONE |

---

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | M1: Audio DSP Quality & Hardware Hook Audit | Verify & enforce 4th-order Butterworth LPF, SoundTouch 64-tap Sinc filter, and REAPER 64-bit ASIO hook mixing | none | DONE |
| 2 | M2: Key Transposer & State Sync Invariants | Verify & enforce key locking invariant, semitone calculation, zero-glitch playback/drag, and SQLite batch hydration | M1 | DONE |
| 3 | M3: Automated Test Suite, Build Quality & Audit Gate | Ensure zero-warning build, resolve Debug timing threshold in `TestSuite_EmpiricalChallenger_R2.cpp`, verify full test suite, verify CRIT-* comments, and run final Forensic Audit | M2 | DONE |

---

## Interface Contracts
### `core::audio::Engine` ↔ `extension::reaper_plugin`
- `Engine::init(bool useDevice)`: `useDevice = false` inside REAPER to bypass WASAPI device init.
- `Engine::renderFrames(float* outL, float* outR, int numFrames)`: Renders 32-bit float audio buffer for direct addition to REAPER 64-bit `ReaSample` master output.
- `Engine::setTargetSampleRate(int sr)`: Dynamically syncs internal resampler with REAPER hardware sample rate.

### `bridge::Bridge` ↔ `ui-web/app.js`
- `audio.play({ path, loop, syncBpm, sampleBpm, pitchSemitones })`: SoundTouch receives initial pitch shift immediately on play start.
- `audio.state` / `audio.syncState`: Emitted periodically; UI ignores `semitones`/`pitchSemitones` if `state.isUserTargetKeyLocked == true`.
- `browser.beginDrag({ path, syncBpm, sampleBpm, pitchSemitones })`: Native REAPER drag queuing with pitch and tempo stretch parameters.
- `fs.list({ path })`: Returns `FileEntry` array with hydrated `bpm`, `key`, `camelot`, `durationSec` from SQLite DB.

---

## Code Layout
- `core/include/reals/audio/`, `core/src/audio/`: Audio engine, DSP processors, WAV exporter.
- `core/include/reals/db/`, `core/src/db/`: SQLite database schema, FTS5 search, batch hydration.
- `core/include/reals/ai/`, `core/src/ai/`: Tempo detection, key detection, CLAP embedding.
- `bridge/include/reals/bridge/`, `bridge/src/`: JSON RPC interface, action dispatcher.
- `extension/src/`: REAPER C++ plugin, hardware audio hook, OLE drag hook.
- `ui-web/`: Web application frontend (HTML, CSS, JS).
- `tests/`: Test framework (`TestRunner.h`, `AudioTestFixtures.h`, `DbTestFixtures.h`, `MockHostActions.h`), test suites (`tests/suites/`), unit tests (`tests/unit/`), benchmarks (`tests/benchmarks/`).
