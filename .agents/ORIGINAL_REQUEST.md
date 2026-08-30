# Original User Request

## Initial Request — 2026-08-30T19:41:27Z

Investigate and fix the tempo mismatch (preview playing faster than DAW) and verify the REAPER Extension implementation against the official Playhead Phase Sync master specification.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

## Problem Summary
When previewing audio in the REAPER extension, the audio tempo is faster than the DAW tempo. The entire audio pipeline (host sample rate detection, resampling during decoding, SoundTouch tempo ratio, and OnAudioBuffer hardware mixing) needs a comprehensive audit and fix against the 8-point Playhead Phase Sync master specification.

## Reference Specification (Playhead Phase Sync trong REAPER Extension)
1. **Mục tiêu:** Đồng bộ pha nhịp giữa tiến trình nội bộ với playhead của REAPER không trôi, không giật khi seek/loop/đổi tempo.
2. **Hai tầng truy cập:** 
   - Main Thread (`GetPlayPosition`): UI, tempo map, seek/loop.
   - Audio Hook (`OnAudioBuffer`): DSP sample-accurate, không malloc/mutex/blocking.
3. **API đăng ký:** `Audio_RegHardwareHook(true, &g_hook)` với `g_hook.OnAudioBuffer = OnAudioBuffer`. Lấy buffer thật qua `reg->GetBuffer(isOutput, idx)`.
4. **Vị trí & Tempo:** `GetPlayPosition2Ex(nullptr)` cho vị trí audio block; `TimeMap_GetDividedBpmAtTime(playPos)` hoặc `Master_GetTempo()` cho BPM; `TimeMap2_timeToBeats` cho beat/phase.
5. **Vòng lặp tính phase:** Query lại từ REAPER mỗi block, phát hiện discontinuity (`fabs((playPos - lastPos) - expectedDelta) > 0.01`).
6. **Giao tiếp luồng:** `std::atomic` với `memory_order_relaxed`.
7. **Bù lệch pha:** PI controller cho lệch nhỏ; hard seek cho lệch lớn.
8. **Quy tắc an toàn:** Không malloc/new, không file I/O, không std::mutex trong audio thread.

## Requirements

### R1. Root Cause Investigation of Tempo Speedup
Audit the entire audio pipeline to identify all sources of tempo drift or acceleration:
- Sample rate conversion between audio files (44.1k/48k/96k) and host ASIO device sample rate.
- SoundTouch time-stretching ratio calculations in `Bridge.cpp` (`projectBpm / sampleBpm` vs `timeRatio`).
- Buffer frame rate mismatches in `OnAudioBuffer` and `Engine::renderFrames`.

### R2. Strict Compliance with 8-Point Playhead Phase Sync Specification
Ensure `extension/src/reaper_plugin.cpp`, `core/src/audio/Engine.cpp`, and `bridge/src/Bridge.cpp` comply 100% with the 8-point guide:
- `!isPost` phase: Read `GetPlayPosition2Ex(nullptr)`, `TimeMap_GetDividedBpmAtTime(playPos)`, and `TimeMap2_timeToBeats(nullptr, playPos, ...)`. Update atomic transport values and detect discontinuities.
- `isPost` phase: Mix audio into `reg->GetBuffer(true, 0)` and `reg->GetBuffer(true, 1)`.
- Zero file I/O, zero mutex locking, zero dynamic allocations on the audio callback path.

### R3. Exact Tempo and Phase Matching
Ensure that when a sample is played (with or without Sync BPM enabled), its playback speed and beat grid match the REAPER project tempo perfectly with 0ms phase lag.

## Acceptance Criteria

### Tempo & Sample Rate Accuracy
- [ ] Preview audio plays at exactly 1.0x original speed when BPM sync is OFF, regardless of whether REAPER runs at 44.1kHz, 48kHz, 88.2kHz, 96kHz, or 192kHz.
- [ ] Preview audio plays at exactly `projectBpm / sampleBpm` speed when BPM sync is ON, locking directly into REAPER's beat grid.
- [ ] Pitch is completely unaffected by host sample rate variations when pitch shift is 0 semitones.

### Audio Thread Safety & Build Quality
- [ ] Code builds with 0 errors and 0 warnings on Windows (`cmake --build --preset windows` / MSVC).
- [ ] All automated tests pass (`ctest --preset windows`).
- [ ] No `new`, `malloc`, disk reads (`ma_decoder_read_pcm_frames`), or blocking mutex locks occur inside `ReaperOnAudioBuffer`.
