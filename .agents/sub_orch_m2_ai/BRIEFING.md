# BRIEFING — 2026-08-26T14:50:00Z

## Mission
Implement Milestone 2: Local Offline AI Inference Engine & Models (`core/ai/`) for Reals Lab with ONNX Runtime wrapper, Model Manager, TempoCNN + RhythmExtractor2013, EDMA Key Voting + Camelot/OpenKey, Discogs-MAEST 400 subgenres, Mood-Jamendo 56 tags, CLAP 512-dim embeddings, native DSP feature pipelines, and unit tests.

## 🔒 My Identity
- Archetype: Sub-orchestrator / Implementer / QA / Specialist
- Roles: implementer, qa, specialist
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\sub_orch_m2_ai\
- Original parent: a7d204d0-f8fe-4ddf-adbd-9dbf52a9d99a
- Milestone: M2 - Local Offline AI Inference Engine & Models

## 🔒 Key Constraints
- Integrity mode: Genuine implementations, zero hardcoded cheat results, zero facade bypass.
- C++20 standard, zero-warning policy (`/W4 /WX` or clean build).
- Local offline 100% operation with ONNX Runtime C++ wrapper and model weights in `%APPDATA%/RealsLab/models/`.
- Must use GitNexus tools in every step.
- Must follow architectural layer boundaries: `core/ai/` has zero UI/host dependencies.

## Current Parent
- Conversation ID: a7d204d0-f8fe-4ddf-adbd-9dbf52a9d99a
- Updated: not yet

## Task Summary
- **What to build**: Full `core/ai/` module:
  1. `OnnxEngine.h/.cpp`: ONNX Runtime C++ wrapper with robust memory management, session management, tensor creation, inference execution.
  2. `ModelManager.h/.cpp`: Model loading/caching from `%APPDATA%/RealsLab/models/` with SHA256 integrity check and manifest support.
  3. `TempoDetector.h/.cpp`: Essentia TempoCNN (BPM, confidence, beat onsets) with algorithmic fallback `RhythmExtractor2013` (envelope autocorrelation / peak picking / comb filtering).
  4. `KeyDetector.h/.cpp`: Essentia EDMA key classification + ensemble voting combining Temperley and Krumhansl-Schmuckler chromagram profiles (Key, Mode, Camelot 1A-12B, OpenKey notation 1d-12m).
  5. `GenreClassifier.h/.cpp`: Discogs-MAEST (400 musical styles, top-5 output with confidence).
  6. `MoodClassifier.h/.cpp`: Mood-Jamendo multi-label classifier (56 mood/theme tags).
  7. `ClapEmbedder.h/.cpp`: CLAP Audio and Text 512-dim float embedding extraction.
  8. Audio feature extraction utilities (STFT, Mel spectrogram, Chroma, Log mel filterbanks) in C++ without external heavy python deps.
  9. Unit tests for all AI detector classes and feature extraction pipelines.
- **Success criteria**:
  - Full C++20 clean build with zero warnings.
  - Comprehensive unit test suite covering real mathematical/DSP audio feature extraction, Tempo detection + fallback, Key detection + voting + Camelot mapping, Genre classification, Mood classification, and CLAP embeddings.
- **Interface contracts**: PROJECT.md § Interface Contracts (`core/ai` ↔ `core/scanner` / `core/search`).
- **Code layout**: `core/include/reals/ai/` and `core/src/ai/`, `tests/`.

## Key Decisions Made
- Implement standalone native DSP feature extractors (STFT, Mel Filterbank, Chroma, Constant-Q / Pitch Profiles) inside `core/ai/` so inference pipeline is self-contained, real, and operates reliably in both full ONNX model mode and mathematical heuristic/fallback mode.

## Artifact Index
- `.agents/sub_orch_m2_ai/DISPATCH.md` — Assignment instructions
- `.agents/sub_orch_m2_ai/BRIEFING.md` — Working state and identity
- `.agents/sub_orch_m2_ai/progress.md` — Liveness and step tracking
- `.agents/sub_orch_m2_ai/SCOPE.md` — Detailed M2 scope breakdown
- `.agents/sub_orch_m2_ai/GATE_STATUS.md` — Verification and gating status

## Change Tracker
- **Files modified**: None yet
- **Build status**: Pending
- **Pending issues**: None

## Quality Status
- **Build/test result**: Pending
- **Lint status**: 0 violations
- **Tests added/modified**: Pending

## Loaded Skills
- None
