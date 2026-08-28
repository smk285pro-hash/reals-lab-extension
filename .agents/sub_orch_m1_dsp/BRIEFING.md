# BRIEFING — 2026-08-26T14:50:00Z

## Mission
Milestone 1: DSP Engine & Real-time Pitch/BPM Sync for Reals Lab (SoundTouch DSP integration, SoundTouchProcessor, Engine pipeline updates, tests, zero-warning C++20 build).

## 🔒 My Identity
- Archetype: Sub-orchestrator / Implementer / QA / Specialist
- Roles: implementer, qa, specialist
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\sub_orch_m1_dsp\
- Original parent: a7d204d0-f8fe-4ddf-adbd-9dbf52a9d99a
- Milestone: Milestone 1 (DSP Engine & Real-time Pitch/BPM Sync)

## 🔒 Key Constraints
- Pure C++20, zero warnings under `/W4 /permissive- /utf-8` on MSVC.
- Zero UI/REAPER dependencies in `core/`.
- SoundTouch C++ DSP library integration in `libs/soundtouch/` and `core/audio/`.
- Provide `reals::audio::SoundTouchProcessor` and integrate into `Engine` pipeline.
- Independent time-stretching (`setTimeRatio(float ratio)`) for DAW BPM Sync without affecting pitch.
- Independent real-time pitch-shifting (`setPitchSemitones(float semitones)` for range -12.0 to +12.0) without altering duration (< 30ms latency).
- Reset pitch / original key (`resetPitch()` / `setOriginalKey()`) restoring 0.0 semitones.
- Unit tests covering `SoundTouchProcessor` and `Engine` DSP time-stretch and pitch-shift.
- Mandatory GitNexus usage in every step.

## Current Parent
- Conversation ID: a7d204d0-f8fe-4ddf-adbd-9dbf52a9d99a
- Updated: 2026-08-26T14:50:00Z

## Task Summary
- **What to build**: SoundTouch C++ DSP library integration (`libs/soundtouch/`), `SoundTouchProcessor` (`core/include/reals/audio/SoundTouchProcessor.h`, `core/src/audio/SoundTouchProcessor.cpp`), `Engine` audio processing pipeline updates with time-stretch & pitch-shift, unit test suite (`tests/test_soundtouch_processor.cpp`, `tests/test_audio_engine.cpp`), CMake configuration with CTest.
- **Success criteria**: Zero compiler warnings, 100% test pass, pitch-shifting ±12 semitones without tempo change, time-stretching without pitch change, latency < 30ms.
- **Interface contracts**: `PROJECT.md` § Interface Contracts: `core/audio` ↔ `bridge`
- **Code layout**: `PROJECT.md` § Code Layout

## Change Tracker
- **Files modified**: TBD
- **Build status**: Pending
- **Pending issues**: None

## Quality Status
- **Build/test result**: Not yet run
- **Lint status**: Clean
- **Tests added/modified**: 0

## Loaded Skills
- None

## Key Decisions Made
- Integrate standard SoundTouch C++ library (single/clean source set in `libs/soundtouch/`) compiled as part of `reals_core` or static lib with zero warnings.
- Implement `SoundTouchProcessor` encapsulating `soundtouch::SoundTouch` with clean RAII, configurable latency settings (`SETTING_SEQUENCE_MS`, `SETTING_SEEKWINDOW_MS`, `SETTING_OVERLAP_MS`, `SETTING_USE_QUICKSEEK`), sample conversion, and multi-channel processing.
- Connect `Engine` to decode PCM data through `ma_decoder` or `ma_data_source` and route through `SoundTouchProcessor` during playback.

## Artifact Index
- `.agents/sub_orch_m1_dsp/BRIEFING.md`
- `.agents/sub_orch_m1_dsp/progress.md`
- `.agents/sub_orch_m1_dsp/SCOPE.md`
- `.agents/sub_orch_m1_dsp/GATE_STATUS.md`
- `.agents/sub_orch_m1_dsp/handoff.md`
