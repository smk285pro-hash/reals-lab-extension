# Project: Reals Lab — Playhead Phase Sync & DAW Drag & Drop Alignment

## Architecture
- `core/audio`: SoundTouchProcessor, DragExporter, Engine (DSP, Decoder, Audio Playback)
- `bridge`: Bridge RPC handler (`audio.play`, `browser.beginDrag`), IHostActions abstract interface
- `extension`: reaper_plugin (ExtHostActions, REAPER C API transport, take synchronization & OLE message posting)
- `shell/win`: OleDrag (Win32 IDropSource, IDataObject, CF_HDROP, CF_UNICODETEXT)
- `tests`: TestRunner, MockHostActions, AudioTestFixtures, 11 Test Suites (183+ test cases)

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | R1.1 REAPER Host Transport API Extension | Import GetPlayPosition and TimeMap2_timeToBeats in extension, expose HostTransport struct via IHostActions | M1 | ORIGINAL_REQUEST §R1 |
| 2 | R1.2 Audio Engine Seek Fraction on Play | Add startFraction parameter to Engine::playFile with pre-seek & DSP buffer clear | M1 | ORIGINAL_REQUEST §R1 |
| 3 | R1.3 Bridge audio.play Playhead Phase Sync | Calculate startFraction = clamp(fmod(fullbeats, loopBeats) / loopBeats, 0.0, 0.999) when DAW playing | M1 | ORIGINAL_REQUEST §R1, A1 |
| 4 | R2.1 Mechanism A Native CF_HDROP Drag | Route original sample path directly to beginDrag and queueSyncPlayrate, eliminating double-stretch and UI drag latency | M2 | ORIGINAL_REQUEST §R2, A2 |
| 5 | R2.2 REAPER Native Take Sync & Mechanism B Safeguard | Apply D_PLAYRATE, B_PPITCH=1, D_PITCH, D_LENGTH in processPendingSyncPlayrates; preserve D_PLAYRATE=1.0 for baked WAVs | M2 | ORIGINAL_REQUEST §R2, A2 |
| 6 | R3.1 Comprehensive Test Suite (183+ Tests) | 183 automated tests across 11 test suites covering Phase Sync, Drag Alignment, DSP, Bridge, AI, DB, and End-to-End | M3 | ORIGINAL_REQUEST §R3, A3 |
| 7 | R3.2 Zero-Warning C++20 MSVC Build | Enforce /W4 /permissive- /utf-8 /FS clean build across all core, bridge, extension, app, and test targets | M4 | ORIGINAL_REQUEST §R3, A3 |
| 8 | R3.3 Automated DLL Deployment | Post-build deployment command copying reaper_realslab.dll to %APPDATA%\REAPER\UserPlugins\reaper_realslab.dll | M4 | ORIGINAL_REQUEST §R3, A3 |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | M1_PlayheadPhaseSync | R1.1, R1.2, R1.3: HostTransport API, Engine startFraction, Bridge audio.play phase sync formula | none | DONE |
| 2 | M2_DragAlignment_DoubleDSPFix | R2.1, R2.2: Mechanism A native CF_HDROP routing, Mechanism B safeguard, zero-lag drag start | none | DONE |
| 3 | M3_TestSuite183_Verification | R3.1: 183+ test cases verification in reals_tests.exe with 100% pass rate | M1, M2 | DONE |
| 4 | M4_ZeroWarning_DLLDeploy | R3.2, R3.3: Zero-warning C++20 build and automated DLL deployment to %APPDATA%\REAPER\UserPlugins\ | M3 | DONE |


## Interface Contracts
### `reals::bridge::IHostActions` ↔ `reaper_realslab` / `reals_shell_win`
- `struct HostTransport { int playState; double playPosition; double fullBeats; int measure; int beatsPerMeasure; int denom; double bpm; bool isPlaying() const; };`
- `virtual HostTransport hostTransport() const`
- `virtual void queueSyncPlayrate(const std::string& path, double playrate, double pitchSemitones)`
- `virtual void beginDrag(const std::string& path)`

### `reals::audio::Engine`
- `bool playFile(const std::string& path, bool loop = false, double startFraction = 0.0);`

### `reals::audio::DragExporter` (Mechanism B Utility)
- `static DragExportResult exportTempWav(const std::string& inputPath, const DragExportOptions& options);`
- `static std::string getTempExportPath(const std::string& inputPath, float timeRatio, float pitchSemitones);`
- `static void cleanupTempFiles(uint64_t maxAgeSeconds = 86400);`

## Code Layout
- `bridge/src/Bridge.cpp`: RPC handlers for audio.play and browser.beginDrag (Mechanism A dispatch)
- `extension/src/reaper_plugin.cpp`: ExtHostActions, processPendingSyncPlayrates (take sync and Mechanism B safeguard)
- `extension/CMakeLists.txt`: Post-build deployment to `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`
- `core/include/reals/audio/Engine.h` & `core/src/audio/Engine.cpp`: Engine startFraction
- `core/include/reals/audio/SoundTouchProcessor.h` & `core/src/audio/SoundTouchProcessor.cpp`: Low-latency DSP
- `tests/suites/`: 11 Test Suites containing 183 automated tests

