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
