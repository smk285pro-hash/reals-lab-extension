# Project: Reals Lab — Audio Tempo Mismatch & 8-Point Playhead Phase Sync Master Fix

## Architecture
- `core/audio`: SoundTouchProcessor, DragExporter, Engine (DSP, Decoder, Resampling, Audio Playback)
- `bridge`: Bridge RPC handler (`audio.play`, `audio.setSyncBpm`, `browser.beginDrag`), IHostActions abstract interface
- `extension`: reaper_plugin (ExtHostActions, REAPER C API transport, Audio Hardware Hook `ReaperOnAudioBuffer`, take synchronization)
- `shell/win`: OleDrag (Win32 IDropSource, IDataObject, CF_HDROP, CF_UNICODETEXT)
- `tests`: TestRunner, MockHostActions, AudioTestFixtures, 11+ Test Suites covering PhaseSyncDiagnostics, AudioDSP, SoundTouchCore

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | F1. Host Sample Rate Cold-Start & Atomic Target SR | Query GetAudioDeviceInfo("SRATE") on plugin load/init, make targetSampleRate atomic, handle standalone mode | M1 | ORIGINAL_REQUEST §R1, Survey |
| 2 | F2. Frame Metric & Loop Boundary Alignment | Synchronize TrackInfo totalFrames/sampleRate with targetSr, fix positionFraction(), seekFraction(), and Bridge setSyncBpm loopBoundaryFrames | M1 | ORIGINAL_REQUEST §R1, Survey |
| 3 | F3. Dynamic Project BPM Resolution in ExtHostActions | Query g_liveTransport.bpm or TimeMap_GetDividedBpmAtTime(playPos) instead of static Master_GetTempo() | M1 | ORIGINAL_REQUEST §R1, Survey |
| 4 | F4. Audio Thread Realtime Safety | Eliminate dspMutex in dsp_on_read, replace thread_local vector resizing with pre-allocated fixed arrays, zero-alloc in audio callback | M2 | ORIGINAL_REQUEST §R2, Survey |
| 5 | F5. 8-Point Phase Sync Compliance & Hardware Hook | Support mono master outputs, handle timeline seek/loop discontinuity in Engine | M2 | ORIGINAL_REQUEST §R2, Survey |
| 6 | F6. Automated Test Suite & Multi-Rate Diagnostics | Diagnostic and unit tests verifying 44.1k/48k/96k sample rates, 1.0x pitch-neutral playback, and tempo ratio accuracy | M3 | ORIGINAL_REQUEST §R3, Survey |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| 1 | M1_Tempo_SampleRate_Alignment | F1, F2, F3: Host sample rate init, frame metrics, dynamic BPM query | none | IN_PROGRESS |
| 2 | M2_AudioThreadSafety_8PointSync | F4, F5: Zero-alloc audio hook, lock-free dsp_on_read, discontinuity handling | M1 | PLANNED |
| 3 | M3_Verification_Audit_Hardening | F6: Full test suite passing, 2 Reviewers, 2 Challengers, Forensic Integrity Audit | M1, M2 | PLANNED |

## Interface Contracts
### `reals::audio::Engine`
- `void setTargetSampleRate(int sampleRate);` (thread-safe, atomic)
- `int targetSampleRate() const;`
- `bool playFile(const std::string& path, bool loop = false, double startFraction = 0.0);`
- `void setLoopBoundaryFrames(uint64_t frames);`
- `uint64_t loopBoundaryFrames() const;`
- `double positionFraction() const;`
- `void renderFrames(float* outL, float* outR, size_t frames);`

### `reals::bridge::IHostActions` ↔ `extension/src/reaper_plugin.cpp`
- `double projectTempo() const override;` (dynamic BPM from playhead / live transport)
- `HostTransport hostTransport() const override;`

## Code Layout
- `core/include/reals/audio/Engine.h` & `core/src/audio/Engine.cpp`: Audio playback engine, decoding, resampling, lock-free rendering
- `core/include/reals/audio/SoundTouchProcessor.h` & `core/src/audio/SoundTouchProcessor.cpp`: Low-latency time stretching
- `bridge/src/Bridge.cpp`: RPC handlers for audio.play and audio.setSyncBpm
- `extension/src/reaper_plugin.cpp`: ExtHostActions, ReaperOnAudioBuffer hardware hook, GetAudioDeviceInfo("SRATE")
- `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp`: Automated phase sync & sample rate unit tests
