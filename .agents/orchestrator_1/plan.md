# Reals Lab — Playhead Phase Sync & DAW Drag & Drop Alignment Plan

## Overview
Build and thoroughly verify two core professional audio engine capabilities for Reals Lab (REAPER Extension & C++ Standalone App):
1. **Playhead Phase Synchronization (R1/A1)**: Bar/Beat phase-accurate preview synchronization with REAPER timeline transport & standalone audio engine without clicks/pops/jitter.
2. **DAW Drag & Drop Alignment (R2/A2)**: Zero-lag drag & drop with perfect REAPER timeline Grid Bar alignment, supporting Mechanism A (Native `CF_HDROP = p` with non-destructive REAPER stretch) and Mechanism B (Bake WAV Export), completely eliminating Double-DSP/Double-Stretch issues.
3. **Comprehensive Test Suite & Deployment (R3/A3)**: 183+ passing unit/integration tests, zero-warning C++20 MSVC build, automated DLL deployment to `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`.

## Execution Workflow

### Phase 0: Survey & Codebase Investigation (3 Parallel Explorers)
- **Explorer 1 (`.agents/explorer_survey_1/`)**: DAW Drag & Drop Alignment, Mechanism A vs Mechanism B, `processPendingSyncPlayrates()`, `CF_HDROP`, Double-Stretch prevention, Zero-lag drag start.
- **Explorer 2 (`.agents/explorer_survey_2/`)**: Playhead Phase Sync, formula normalization, DSP pre-configuration, buffer flushing/reset, startFraction seek, latency & artefact elimination.
- **Explorer 3 (`.agents/explorer_survey_3/`)**: Test Suites expansion (to 183+ tests), CMake build flags, and `%APPDATA%\REAPER\UserPlugins\` deployment automation.

### Phase 1: Synthesis & PROJECT.md Update
- Merge Explorer findings into updated `PROJECT.md` feature inventory, architectural contracts, and code layout.

### Phase 2: Implementation & Verification (Worker)
- Worker implements core/extension/bridge changes, adds test cases to reach 183+ passing tests, executes build and tests, and deploys DLL.

### Phase 3: Independent Review, Adversarial Challenge & Forensic Audit
- 2 Reviewers (`teamwork_preview_reviewer`)
- 2 Challengers (`teamwork_preview_challenger`)
- 1 Forensic Auditor (`teamwork_preview_auditor`)

### Phase 4: Gate Evaluation & Final Reporting
- Evaluate GATE_STATUS.md (strict AND criteria).
- Deliver comprehensive human report in Vietnamese.

