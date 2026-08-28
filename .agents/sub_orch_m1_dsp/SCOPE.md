# SCOPE — Milestone 1: DSP Engine & Real-time Pitch/BPM Sync

## Objectives
1. Integrate SoundTouch C++ DSP library into `libs/soundtouch/` and wire into CMake build (`reals_core`).
2. Create `core/include/reals/audio/SoundTouchProcessor.h` and `core/src/audio/SoundTouchProcessor.cpp`:
   - Wrap `soundtouch::SoundTouch` with clean C++20 interface.
   - Support `setChannels(int channels)`, `setSampleRate(int sampleRate)`.
   - Support `setTimeRatio(float ratio)` (tempo stretching independent of pitch).
   - Support `setPitchSemitones(float semitones)` (pitch shifting -12.0 to +12.0 independent of tempo).
   - Support `resetPitch()` / `setOriginalKey()` (sets semitones to 0.0).
   - Support `putSamples(const float* samples, size_t frames)` and `receiveSamples(float* output, size_t maxFrames) -> size_t`.
   - Support low-latency configuration (< 30ms latency for real-time preview).
   - Support `flush()`, `clear()`, `numSamplesAvailable()`.
3. Update `core/include/reals/audio/Engine.h` and `core/src/audio/Engine.cpp`:
   - Implement `setTimeRatio(float ratio)`, `getTimeRatio() const -> float`.
   - Implement `setPitchSemitones(float semitones)`, `getPitchSemitones() const -> float`.
   - Implement `resetPitch()`, `setOriginalKey()`.
   - Maintain backwards-compatible `setPitch(float ratio)`, `pitch() const -> float`.
   - Implement realtime DSP pipeline: stream decoded PCM from `ma_decoder` through `SoundTouchProcessor` directly in the miniaudio playback pipeline.
4. Integrate Unit Testing:
   - Create `tests/test_soundtouch_processor.cpp` and `tests/test_audio_engine.cpp`.
   - Test time-stretch: verify output duration changes proportionally without pitch change.
   - Test pitch-shift: verify output frequency / pitch changes without duration alteration, and latency < 30ms.
   - Test reset pitch / original key: restores 0.0 semitones.
   - Test multi-channel, edge cases (0 frames, large frames, rapid parameter changes).
5. Build and verify:
   - Zero warnings under MSVC `/W4 /permissive- /utf-8`.
   - All tests pass 100%.
