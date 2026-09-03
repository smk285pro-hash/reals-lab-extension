# Original User Request

## Initial Request — 2026-09-02T22:29:27+07:00

You are the PROJECT ORCHESTRATOR for Reals Lab.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Original Request file: c:\Users\smk28\Desktop\reals lab extension\ORIGINAL_REQUEST.md
Your metadata directory: c:\Users\smk28\Desktop\reals lab extension\.agents\orchestrator_1

## Task: Comprehensive Adversarial Audit & Empirical Verification of Audio Preview and Transposition Pipeline

Please lead the complete execution of the user's request:

### R1. Audio DSP Quality & Hardware Hook Signal Integrity Audit
Perform a rigorous audit of the audio rendering and resampling pipeline:
1. Verify `ma_decoder` initialization with 4th-order Butterworth anti-aliasing low-pass filter (`lpfOrder = 4`) and uniform stereo float32 buffering across all mono/stereo files.
2. Verify SoundTouch DSP processing (`SETTING_USE_AA_FILTER = 1`, 64-tap Sinc filter, `SETTING_USE_QUICKSEEK = 0`, standard sequence windows) ensuring zero aliasing foldover, zero transient skipping, and zero phase distortion.
3. Verify REAPER `Audio_RegHardwareHook` direct 64-bit ASIO master output mixing (`reals::audio::Engine::instance().init(false)`) ensuring bit-perfect audio without Windows WASAPI loopback degradation.

### R2. Key Transposer & BPM Lock Invariant Verification
Audit the musical pitch shifting and state synchronization logic:
1. Verify that `state.isUserTargetKeyLocked` strictly preserves `state.userTargetNote` across sample selection, `audio.state` / `audio.syncState` events, and background metadata hydration.
2. Verify that `audio.play` and `browser.beginDrag` compute and pass the exact semitone shift relative to the sample's root note and the user's locked note without lag or glitch.
3. Verify SQLite metadata hydration in `fs.list` via `Database::getSamplesByPaths()`.

### R3. Automated Test Suite & Build Quality
1. Verify that `ctest --preset windows` (or `reals_tests.exe`) passes all unit and integration tests with 0 failures.
2. Verify MSVC compilation passes with 0 warnings.
3. Ensure all critical invariants are clearly documented with explanatory inline comments (`CRIT-*`) and recorded in `PLAN.md`.

## Mandatory Constraints & Protocol:
1. Use GitNexus tools (via MCP) for symbol context, impact analysis, query, and detect_changes.
2. Follow AGENTS.md rules strictly (C++20, zero-warning, smart pointers, thread safety, update PLAN.md).
3. Maintain your `progress.md` and `BRIEFING.md` in `.agents/orchestrator_1`.
4. When finished, produce a comprehensive handoff report (`handoff.md`) and notify parent.

## Follow-up — 2026-09-03T01:52:36+07:00

Requested team: Full investigative and engineering team ("lập tổ đội điều tra")

Investigate, diagnose with empirical DSP measurements, and completely eliminate all remaining bass popping, thumping (at playback onset and continuously during playback), and kick drum dropping/cancellation when previewing audio with tempo stretching and transport seeking.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

## Problem Context & Acoustic Symptoms
- **Symptom 1 (Playback Start / Seek)**: An audible pop/thump occurs at the initial start of playback or during transport seek operations.
- **Symptom 2 (Continuous Playback)**: Low-frequency pops, thumps, or transient distortion occur intermittently during continuous playback under accelerated tempo (timeRatio > 1.0), accompanied by intermittent kick attenuation on dense mixes (e.g. Slap House / Deep House).
- **Test Dataset**: `D:\Sample pack\Deep house, Slap house\Slap house\Sound Mafia - Slap House Essentials Vol.1\SMSH_Project_Files\Demo WAV` (31 commercial 24-bit 44.1kHz/48kHz demo loops).

## Requirements

### R1. Initial Playback & Seek Discontinuity Elimination
Diagnose and eliminate audio buffer gaps, silence zero-padding, or abrupt waveform step-functions occurring at initial playback start and during transport seeks in `Engine.cpp` and `Bridge.cpp`. Ensure that starting or seeking playback begins on a zero-crossing or smoothly crossfaded pre-roll without any DC step impulse.

### R2. Steady-State WSOLA Splice & Sub-Bass Phase Integrity
Ensure that time-stretching processing continuously preserves phase continuity across low frequencies (20Hz - 150Hz) and transient attack envelopes without causing step discontinuities, phase collisions, or kick attenuation at any tempo ratio between 0.75x and 1.5x.

### R3. Programmatic DSP Verification Suite
Develop and execute programmatic test assertions that inspect rendered PCM buffers directly for:
1. First and second derivative spikes (DC step impulses / pops).
2. Energy preservation of low-frequency transients (kick drum punch).
3. Zero-warning compilation and 100% test pass on Windows MSVC (`cmake --preset windows`).

## Acceptance Criteria

### Transient & Bass Quality
- [ ] 0 low-frequency popping/thump impulses (no sample-to-sample step discontinuities > threshold) at playback start, seek, and loop boundaries.
- [ ] 0 dropped or attenuated kick hits across all 31 reference test files in the Sound Mafia Demo WAV folder under tempo scaling (1.02x to 1.30x).
- [ ] Audio waveforms remain phase-continuous and clean across all volume levels without digital clipping or soft-limit distortion.

### Code & Verification Integrity
- [ ] All automated tests pass with 100% success rate (`ctest --preset windows` or `reals_tests.exe`).
- [ ] Windows preset builds cleanly with zero compilation errors and zero warnings (`cmake --build --preset windows`).
- [ ] Final `reaper_realslab.dll` is built and deployed to REAPER `%APPDATA%/REAPER/UserPlugins`.

