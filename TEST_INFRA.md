# Reals Lab — E2E Test Infrastructure & Specification

> Document Version: 1.0.0  
> Target System: Reals Lab (REAPER Extension & Standalone Desktop App)  
> Scope: End-to-End, Integration, and Contract Verification for M1–M6 Milestones  

---

## 1. Test Philosophy & Engineering Principles

The Reals Lab End-to-End (E2E) Testing Track is designed to provide comprehensive, deterministic, and hermetic verification of all core audio, AI inference, scanner, database, search, and UI bridge subsystems.

### 1.1 Core Principles
1. **Opaque-Box Verification**: Test behavior and observable contracts across public C++ API boundaries and JSON-RPC bridge interfaces without coupling tests to ephemeral internal implementation details.
2. **Zero Dummy/Facade Policy (Integrity Mandate)**:
   - All tests execute genuine computation: real DSP algorithms, real mathematical vector calculations (SIMD dot products / cosine similarities), real SQLite storage transactions, real file I/O operations, and real JSON-RPC serialization.
   - Hardcoded results, fake return strings, or artificial pass shortcuts are strictly forbidden.
3. **Hermetic & Self-Contained Execution**:
   - Tests do not depend on external cloud services or unstable external network environments.
   - Audio fixtures are procedurally synthesized in-memory (PCM sine sweeps, beat transients, multi-channel streams, corrupted headers).
   - Databases operate in isolated temporary directories or SQLite `:memory:` modes.
4. **Deterministic & Fast Feedback**:
   - The test suite executes within seconds on developer machines and continuous integration (CI) environments with zero flaky tests.
   - All timing-dependent tests utilize monotonic clocks and explicit synchronization primitives rather than arbitrary sleep delays.
5. **Zero-Warning Compilation**:
   - The test suite builds under MSVC `/W4 /permissive- /utf-8` and Clang/GCC `-Wall -Wextra -Wpedantic` with zero warnings.

---

## 2. Feature Inventory Coverage Mapping

Every feature from `PROJECT.md` is mapped to concrete test suites across the four testing tiers:

| # | Feature Name | Primary Module | Milestone | Test Suite | Tiers Covered |
|---|---|---|---|---|---|
| **F01** | SoundTouch DSP Engine | `core/audio` | M1 | `TestSuite_AudioDSP` | Tier 1, 2, 3, 4 |
| **F02** | Pitch-Neutral Time-Stretch (DAW Sync) | `core/audio` | M1 | `TestSuite_AudioDSP` | Tier 1, 2, 3, 4 |
| **F03** | Real-Time Pitch Shifter (±12 Semitones) | `core/audio` | M1 | `TestSuite_AudioDSP` | Tier 1, 2, 3, 4 |
| **F04** | ONNX Runtime C++ Host | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F05** | Model Weights Manager & SHA256 Validation | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F06** | Essentia TempoCNN & Onsets | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F07** | Essentia EDMA Key & Ensemble Voting | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F08** | Discogs-MAEST 400 Subgenres | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F09** | Mood-Jamendo Multi-Label Classifier | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F10** | CLAP 512-dim Embeddings | `core/ai` | M2 | `TestSuite_AIInference` | Tier 1, 2, 3, 4 |
| **F11** | SQLite Library Database & Vector BLOB | `core/db` | M3 | `TestSuite_DatabaseScanner` | Tier 1, 2, 3, 4 |
| **F12** | File Hash Checksum Cache (xxHash64/SHA256) | `core/db` | M3 | `TestSuite_DatabaseScanner` | Tier 1, 2, 3, 4 |
| **F13** | Multi-Threaded Background Scanner Pool | `core/scanner` | M3 | `TestSuite_DatabaseScanner` | Tier 1, 2, 3, 4 |
| **F14** | Syntax `/` Query Parser (`/tag`, `/bpm`, `/key`, `/fav`) | `core/search` | M4 | `TestSuite_SearchEngine` | Tier 1, 2, 3, 4 |
| **F15** | SIMD Cosine Semantic Search (AVX2/SSE2) | `core/search` | M4 | `TestSuite_SearchEngine` | Tier 1, 2, 3, 4 |
| **F16** | Bridge RPC Extended Contracts | `bridge` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F17** | Player Tag & Mood Badges Row | `bridge` / `ui-web` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F18** | Sync BPM Button Highlight & Ratio Math | `bridge` / `core/audio` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F19** | Mini Piano Keyboard Transposer Popup | `bridge` / `ui-web` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F20** | Original Key Reset Button | `bridge` / `core/audio` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F21** | Responsive UI & REAPER Docking | `shell` / `extension` | M5 | `TestSuite_BridgeUI` | Tier 1, 2, 3, 4 |
| **F22** | E2E Verification & Adversarial Hardening | End-to-End | M6 | `TestSuite_EndToEndWorkflows` | Tier 1, 2, 3, 4 |

---

## 3. Test Runner & Harness Architecture

```
tests/
├── CMakeLists.txt              # Test build definitions & CTest registration
├── framework/
│   ├── TestRunner.h            # Lightweight C++20 Test Harness & Assertion macros
│   ├── AudioTestFixtures.h     # In-memory WAV synthesis, pulse/sine generators, corruption inject
│   ├── DbTestFixtures.h        # Hermetic SQLite DB environments & test dataset seeding
│   ├── MockHostActions.h       # Mock REAPER host tracking actions, undos, tempo, dock state
│   └── ModelMocks.h            # Deterministic AI model weight generators & embedding verifiers
├── suites/
│   ├── TestSuite_AudioDSP.cpp          # M1: Features 1, 2, 3 (SoundTouch, DAW Sync, Pitch Shift)
│   ├── TestSuite_AIInference.cpp       # M2: Features 4, 5, 6, 7, 8, 9, 10 (ONNX, Tempo, Key, Genres, CLAP)
│   ├── TestSuite_DatabaseScanner.cpp   # M3: Features 11, 12, 13 (SQLite, Hashes, Thread Pool Scanner)
│   ├── TestSuite_SearchEngine.cpp      # M4: Features 14, 15 (Syntax Query Parser, SIMD Vector Search)
│   ├── TestSuite_BridgeUI.cpp          # M5: Features 16, 17, 18, 19, 20, 21 (RPC, Piano, Tags, Sync BPM)
│   ├── TestSuite_BoundariesCorners.cpp # Tier 2: Boundary & Corner Cases across all modules
│   ├── TestSuite_CrossFeatures.cpp     # Tier 3: Pairwise & Multi-way Cross-Feature Integration
│   └── TestSuite_EndToEndWorkflows.cpp # Tier 4: Real-world DAW Workloads & Chaos Scenarios
└── main.cpp                    # Test binary entrypoint with CLI filtering, stats, & timing
```

### 3.1 Assertion Framework (`TestRunner.h`)
- Type-safe assertions: `EXPECT_TRUE`, `EXPECT_FALSE`, `EXPECT_EQ`, `EXPECT_NEAR`, `EXPECT_THROW`, `EXPECT_NO_THROW`.
- Detailed failure reporting with exact file, line number, expected vs actual values, and contextual error diagnostics.
- Execution metrics: Test execution count, pass/fail totals, per-test elapsed wall-clock duration in microseconds.
- Filtering support: Run all tests or filter by suite (`--suite=AudioDSP`) or test name substring.

### 3.2 Audio Test Fixtures (`AudioTestFixtures.h`)
- Procedural PCM Synthesizer:
  - Pure sine wave generators (440Hz A4, 1kHz calibration tone).
  - Beat rhythm generators (rhythmic kick/snare clicks at precise BPMs: 60, 120, 128, 140, 174 BPM).
  - Stereo/Mono/Multi-channel interleavers (44.1kHz, 48kHz, 96kHz, 192kHz).
- WAV File Encoder & Header Corruptor:
  - Generates valid IEEE Float / 16-bit PCM RIFF WAV binaries in memory and on disk.
  - Corruptor options: 0-byte truncation, corrupted `fmt ` chunk size, inverted byte orders, trailing garbage bytes, NaN/Inf sample values.

### 3.3 Database Test Fixtures (`DbTestFixtures.h`)
- RAII-managed temporary database directories (`temp_dir/reals_test_XXXXXX/library.db`).
- Deterministic seeding with 10, 100, 1,000, and 10,000+ realistic sample metadata records (BPM, key, tags, 512-dim CLAP vector blobs).
- Automated cleanup on fixture destruction.

### 3.4 Mock Host & Bridge Fixtures (`MockHostActions.h`)
- Captures and records all `IHostActions` calls:
  - `insertMedia(path, mode)`
  - `insertMediaMany(paths, mode)`
  - `getProjectTempo()` (configurable DAW tempo simulation)
  - `getDockState()` / `setDockState()`
  - `undoBeginBlock()` / `undoEndBlock()`
- Bridge RPC message dispatcher validating incoming JSON requests and capturing outgoing push events (`toast`, `audio.state`, `scanner.progress`, `lab.result`).

---

## 4. Test Tier Specifications & Scenarios

### Tier 1: Feature Coverage (>=5 Test Cases per Feature)

#### Subsystem: Audio & DSP Engine (Features 1–3)
- **F01: SoundTouch DSP Engine**
  1. `AudioDSP_InitAndSampleRateChange`: Engine initializes and correctly adapts to sample rate changes (44.1kHz -> 48kHz -> 96kHz).
  2. `AudioDSP_ChannelHandling`: Verifies independent processing of Mono (1ch) and Stereo (2ch) buffers.
  3. `AudioDSP_BufferProcessingThroughput`: Measures chunked feed/receive processing latency (<5ms per 512-frame block).
  4. `AudioDSP_StateReset`: Ensures flushing and clearing internal buffers produces zero residual sound leakage.
  5. `AudioDSP_VolumeAndGainScaling`: Verifies linear and decibel scaling without clipping or digital wrap-around.
- **F02: Pitch-Neutral Time-Stretch (DAW Sync)**
  1. `TimeStretch_120To140BPM_RatioCalculation`: Verifies ratio calculation (140.0 / 120.0 = 1.16667x) and buffer duration match.
  2. `TimeStretch_DownsampleTempo`: Verifies 174 BPM drum loop stretched to 87 BPM (0.5x ratio) maintains pitch spectrum.
  3. `TimeStretch_UpsampleTempo`: Verifies 90 BPM vocal loop stretched to 135 BPM (1.5x ratio) maintains pitch spectrum.
  4. `TimeStretch_PhaseCoherence`: Spectral autocorrelation confirms harmonic peaks remain within ±1Hz of original pitch.
  5. `TimeStretch_DynamicTempoRamping`: Verifies smooth time-ratio modulation while playback is actively running.
- **F03: Real-Time Pitch Shifter (±12 Semitones)**
  1. `PitchShift_Plus12Semitones_OctaveUp`: 440Hz sine wave shifted +12 semitones produces fundamental frequency at 880Hz (±2Hz).
  2. `PitchShift_Minus12Semitones_OctaveDown`: 440Hz sine wave shifted -12 semitones produces fundamental frequency at 220Hz (±2Hz).
  3. `PitchShift_ChromaticScaleIntervals`: Steps through all 12 semitones (+1 to +12) and validates frequency multiplication factor $2^{n/12}$.
  4. `PitchShift_MicrotonalDetune`: Verifies fractional semitone pitch adjustments (e.g. +0.25 semitones / 25 cents).
  5. `PitchShift_ZeroSemitoneNeutrality`: Verifies 0 semitones shift is bit-exact / within floating point epsilon ($\Delta < 10^{-5}$) of original.

#### Subsystem: AI Inference & Essentia Models (Features 4–10)
- **F04: ONNX Runtime C++ Host**
  1. `OnnxHost_EngineInitialization`: Initializes ONNX runtime environment with CPU thread configuration.
  2. `OnnxHost_InputOutputTensorBinding`: Verifies dynamic shape allocation and float32 tensor memory binding.
  3. `OnnxHost_MultiThreadInference`: Concurrent model inference across 4 threads without memory corruption.
  4. `OnnxHost_InvalidTensorDimensionHandling`: Graceful error return when passing mismatched frame length.
  5. `OnnxHost_MemoryDeallocation`: Validates zero memory leaks after 100 consecutive session creations and teardowns.
- **F05: Model Weights Manager & SHA256 Validation**
  1. `ModelManager_ValidChecksumPass`: Model file with matching SHA256 hash passes verification.
  2. `ModelManager_CorruptedChecksumReject`: Model file with altered byte fails SHA256 check and raises error.
  3. `ModelManager_PathResolution`: Correctly resolves model paths in `%APPDATA%\RealsLab\models\` on Windows.
  4. `ModelManager_MissingModelFallback`: Returns structured fallback status when model file does not exist.
  5. `ModelManager_AtomicCacheUpdate`: Ensures safe atomic overwrite when downloading updated model weights.
- **F06: Essentia TempoCNN & Onsets**
  1. `TempoCNN_Detect120BpmFourOnTheFloor`: Synthesized 120 BPM kick pattern detects 120.0 BPM (±0.5 BPM) with >0.90 confidence.
  2. `TempoCNN_Detect174BpmDrumAndBass`: Fast DnB loop detects 174.0 BPM (±0.8 BPM).
  3. `TempoCNN_OnsetTransientPositions`: Rhythmic beat timestamps match ground truth transient positions within 10ms.
  4. `TempoCNN_FallbackRhythmExtractor`: Triggers algorithmic fallback when CNN confidence is below threshold.
  5. `TempoCNN_IrregularTimeSignature`: Analyzes 3/4 and 7/8 time signature audio without throwing exceptions.
- **F07: Essentia EDMA Key & Ensemble Voting**
  1. `KeyDetector_EDMA_C_Major`: 261.63Hz chord triad (C-E-G) correctly classified as C Major (Camelot 8B / OpenKey 1d).
  2. `KeyDetector_EDMA_A_Minor`: 220Hz chord triad (A-C-E) correctly classified as A Minor (Camelot 8A / OpenKey 1m).
  3. `KeyDetector_TemperleyKrumhanslVoting`: Ensemble voting arbitrates ambiguous chord progressions.
  4. `KeyDetector_CamelotWheelConversion`: Validates all 24 Camelot codes (1A–12A, 1B–12B) and OpenKey equivalents.
  5. `KeyDetector_ConfidenceScoreThreshold`: Low signal-to-noise ratio returns low confidence flag.
- **F08: Discogs-MAEST 400 Subgenres**
  1. `Genre_Top5Predictions`: Returns sorted vector of top 5 subgenres with descending confidence probabilities.
  2. `Genre_ProbabilitySumConstraint`: Output probabilities sum to 1.0 (±0.01).
  3. `Genre_ElectronicSubgenreMapping`: Identifies `Tech House`, `Minimal Techno`, `Future Bass` taxonomy IDs.
  4. `Genre_AcousticSubgenreMapping`: Identifies `Delta Blues`, `Post-Rock`, `Bebop` taxonomy IDs.
  5. `Genre_ThresholdFilter`: Correctly filters out classifications below configured confidence threshold (e.g. <0.05).
- **F09: Mood-Jamendo Multi-Label Classifier**
  1. `Mood_MultiLabelActivation`: Output allows multiple simultaneous active moods (e.g. `dark` + `energetic` + `heavy`).
  2. `Mood_56ClassVocabulary`: Validates taxonomy of all 56 supported Mood-Jamendo emotional tags.
  3. `Mood_SoftmaxVsSigmoidOutput`: Ensures independent sigmoid probabilities per mood label.
  4. `Mood_CalmVsAggressiveDifferentiation`: Ambient drone scores high on `calm`/`relaxed` and near zero on `aggressive`.
  5. `Mood_EmptyAudioHandling`: Uniform low-probability distribution returned for near-silent audio.
- **F10: CLAP 512-dim Embeddings**
  1. `CLAP_AudioEmbeddingDimension`: Audio embedding vector contains exactly 512 float32 elements.
  2. `CLAP_TextEmbeddingDimension`: Text query embedding vector contains exactly 512 float32 elements.
  3. `CLAP_UnitL2Norm`: All output embeddings have Euclidean norm equal to 1.0 ($\|v\|_2 = 1.0 \pm 10^{-5}$).
  4. `CLAP_CosineSimilarityMatching`: Dot product between "punchy kick drum" text embedding and synthesized 808 kick audio embedding > 0.65.
  5. `CLAP_OrthogonalityDivergence`: Dissimilar audio (e.g. acoustic flute vs industrial distortion) yields cosine similarity < 0.20.

#### Subsystem: Database, Hash Cache & Scanner (Features 11–13)
- **F11: SQLite Library Database**
  1. `Database_SchemaInitialization`: Creates `samples`, `tags`, `embeddings`, `folders` tables with proper indexes.
  2. `Database_UpsertSampleRecord`: Inserts new sample record and updates existing record on primary key collision.
  3. `Database_VectorBlobStorage`: Stores and retrieves 512-float vector BLOBs with byte-exact precision.
  4. `Database_TransactionCommitRollback`: Failed batch insert rolls back without database corruption.
  5. `Database_QueryFiltering`: Filters records by BPM range, Key, Camelot, and duration bounds.
- **F12: File Hash Checksum Cache**
  1. `HashCache_ComputeChecksum`: Generates fast xxHash64 and SHA256 checksums from file contents.
  2. `HashCache_FastSkipUnchangedFiles`: Re-scanning file with unchanged size, mtime, and hash skips AI analysis.
  3. `HashCache_DetectFileModification`: Re-scanning modified file triggers hash recalculation and DB update.
  4. `HashCache_DatabasePersistence`: Hash table cached in SQLite survives application restart.
  5. `HashCache_EmptyAndLargeFiles`: Calculates valid hashes for 0-byte and 500MB+ audio files.
- **F13: Multi-Threaded Background Scanner**
  1. `Scanner_DirectoryDiscovery`: Discovers all `.wav`, `.mp3`, `.flac`, `.aif` files in nested directory tree.
  2. `Scanner_ThreadPoolConcurrency`: Parallelizes file ingestion across configured worker thread pool (e.g. 4 threads).
  3. `Scanner_ProgressReporting`: Emits progress percentage, current file path, and processed count callbacks.
  4. `Scanner_GracefulCancellation`: Stops scan job cleanly within 100ms when cancel signal is received.
  5. `Scanner_ErrorResilience`: Unreadable or locked files are logged and skipped without halting remaining scan tasks.

#### Subsystem: Search Engine (Features 14–15)
- **F14: Syntax `/` Query Parser**
  1. `QueryParser_TagTokens`: Parses `/trap /kick /808` into structured tag list `["trap", "kick", "808"]`.
  2. `QueryParser_BpmRangeToken`: Parses `/bpm:120-130` into `minBpm=120.0`, `maxBpm=130.0`.
  3. `QueryParser_KeyAndCamelotTokens`: Parses `/key:F#m /camelot:8A` into corresponding key filters.
  4. `QueryParser_FavoriteAndText`: Parses `/fav acoustic guitar` into `onlyFavorites=true`, `freeText="acoustic guitar"`.
  5. `QueryParser_InvalidTokensTolerance`: Malformed tokens (e.g. `/bpm:xyz /unknown:123`) ignored gracefully without crashing.
- **F15: SIMD Cosine Semantic Search**
  1. `SIMD_DotProductExactness`: AVX2/SSE2 vectorized dot product matches scalar reference implementation within $10^{-6}$.
  2. `SIMD_CosineSimilarityRank`: Correctly ranks 10,000 embedding vectors against a query vector in < 5ms.
  3. `SIMD_TopKSelection`: Selects top K (e.g. K=20) results with largest cosine similarity.
  4. `SIMD_ScalarFallbackOnUnsupportedCpu`: Scalar fallback path executes when AVX2 CPUID flag is disabled.
  5. `SIMD_ZeroVectorHandling`: Protects against division by zero if an all-zero vector is encountered.

#### Subsystem: Bridge, UI Contracts & Player Controls (Features 16–21)
- **F16: Bridge RPC Extended Contracts**
  1. `BridgeRPC_AudioSetPitchShift`: Validates `audio.setPitchShift` `{semitones: -3.5}` -> `{ok: true, pitchSemitones: -3.5}`.
  2. `BridgeRPC_AudioSetSyncBpm`: Validates `audio.setSyncBpm` `{enabled: true, bpm: 140.0}` -> `{ok: true, syncBpm: true, ratio: 1.16667}`.
  3. `BridgeRPC_DbSearch`: Validates `db.search` `{query: "/trap /bpm:120-130", limit: 50}` -> `{ok: true, results: [...]}`.
  4. `BridgeRPC_AiSearchSemantic`: Validates `ai.searchSemantic` `{query: "dark distorted bass", limit: 10}` -> `{ok: true, results: [...]}`.
  5. `BridgeRPC_InvalidMethodAndPayload`: Unknown RPC method returns structured JSON error `{ok: false, error: {code: -32601, message: "Method not found"}}`.
- **F17: Player Tag & Mood Badges Row**
  1. `PlayerUI_GenerateTagBadges`: Formats genre and mood results into UI chip badge objects (`#playerTagBar`).
  2. `PlayerUI_ColorMappingByTagType`: Assigns appropriate category CSS color classes (Genre -> blue, Mood -> purple, Instrument -> orange).
  3. `PlayerUI_MaxDisplayTagLimit`: Caps displayed badges to maximum visible width (e.g. top 8 chips) with `+N more` overflow.
  4. `PlayerUI_TagClickFilterAction`: Emits filter event when user clicks a player tag badge.
  5. `PlayerUI_EmptyTagListGraceful`: Renders clean empty state when sample has no metadata tags.
- **F18: Sync BPM Button Highlight & Logic**
  1. `SyncBpm_ToggleStateOn`: Toggling Sync BPM button on sets active orange highlight state (`#btnSyncBpm.active`).
  2. `SyncBpm_ToggleStateOff`: Toggling Sync BPM button off restores normal state and resets playback time ratio to 1.0x.
  3. `SyncBpm_RatioCalculationFromReaperTempo`: Reads REAPER project tempo (e.g. 140 BPM), sample tempo (120 BPM), calculates 1.1667x.
  4. `SyncBpm_ZeroBpmSampleProtection`: When sample has no detected BPM (0 BPM), Sync BPM disables gracefully with toast notice.
  5. `SyncBpm_ProjectTempoChangeNotification`: Adjusts time ratio dynamically when REAPER project tempo changes during playback.
- **F19: Mini Piano Keyboard Transposer Popup**
  1. `PianoTransposer_OpenPopupOnKeyClick`: Clicking Key label opens 12-key chromatic piano keyboard popup (`#pianoTransposerPop`).
  2. `PianoTransposer_12ChromaticKeys`: Validates all 12 note buttons (C, C#, D, D#, E, F, F#, G, G#, A, A#, B).
  3. `PianoTransposer_SemitoneOffsetCalculation`: Clicking a note calculates semitone difference relative to original root key.
  4. `PianoTransposer_RealtimePitchShiftDispatch`: Note click immediately sends `audio.setPitchShift` with calculated semitones.
  5. `PianoTransposer_ActiveKeyHighlight`: Active transposed note is highlighted with orange accent indicator.
- **F20: Original Key Reset Button**
  1. `ResetKey_RestoreOriginalPitch`: Clicking `Original Key` button (`#btnResetKey`) resets pitch shift to 0.0 semitones.
  2. `ResetKey_UIHighlightReset`: Piano key active highlights are cleared and root note indicator is restored.
  3. `ResetKey_AudioEngineVerification`: Verifies audio engine pitch shifter state returns to 1.0x frequency multiplier.
  4. `ResetKey_OriginalKeyLabel`: Player Key display reverts to original detected key name (e.g. `F# Minor`).
  5. `ResetKey_RapidToggleStability`: Rapidly toggling transpose and reset does not introduce DSP clicks or zipper noise.
- **F21: Responsive UI & REAPER Docking**
  1. `Docking_FloatingWindowMode`: UI adapts to standalone floating frameless window (min 800x600).
  2. `Docking_ReaperDockerEmbedded`: UI adapts to narrow REAPER docker layout (e.g. width 320px) collapsing sidebars gracefully.
  3. `Docking_ThemeTokensCompliance`: Confirms all CSS variables adhere to `DESIGN.md` color tokens.
  4. `Docking_I18nLanguageSwitch`: Hot-switching language between Vietnamese (`vi`) and English (`en`) updates all UI strings.
  5. `Docking_WindowControlButtons`: Minimize, Maximize/Restore, and Close buttons dispatch proper OS window commands.
- **F22: E2E Verification & Hardening**
  1. `E2E_FullPipelineExecution`: End-to-end flow from directory scan -> AI tagging -> SQLite store -> Syntax search -> DSP preview.
  2. `E2E_ZeroCrashUnderPressure`: 1,000 rapid concurrent UI and DSP actions complete with zero crashes.
  3. `E2E_MemoryLeakFree`: Continuous 10-minute automated stress test maintains flat memory footprint.
  4. `E2E_UnicodePathIntegrity`: Complete workflow executes flawlessly on Vietnamese Unicode paths.
  5. `E2E_CleanShutdown`: Extension unloads and frees all background threads, COM objects, and file handles without deadlock.

---

### Tier 2: Boundary & Corner Cases (>=5 Test Cases per Category)

1. **Audio File Integrity & Header Corruption**:
   - `Corner_Audio_0ByteFile`: 0-byte `.wav` file handled gracefully with structured error, no crash.
   - `Corner_Audio_CorruptedRiffHeader`: RIFF header truncated after 12 bytes; parser returns `InvalidHeader` error.
   - `Corner_Audio_SilentBufferDigitalZero`: Processing 10 seconds of all-zero PCM samples produces zero RMS and no NaN/Inf.
   - `Corner_Audio_DcOffsetClipping`: Audio buffer with extreme DC offset (+1.5f) clamped safely to `[-1.0, 1.0]`.
   - `Corner_Audio_HugeFileStreaming`: 2-hour 24-bit 96kHz WAV file opens via streaming decoder without exhausting RAM.

2. **Boundary BPM & Extreme Pitch Values**:
   - `Corner_DSP_BoundaryBpmZero`: BPM = 0.0 or negative BPM rejected safely; time stretch defaults to 1.0x ratio.
   - `Corner_DSP_BoundaryBpmExtremeLow`: BPM = 10.0 handled without floating point underflow.
   - `Corner_DSP_BoundaryBpmExtremeHigh`: BPM = 999.0 handled without floating point overflow.
   - `Corner_DSP_ExtremePitchShiftPlus12`: Pitch shift +12.0 semitones (exact 2.0x frequency multiplier).
   - `Corner_DSP_ExtremePitchShiftMinus12`: Pitch shift -12.0 semitones (exact 0.5x frequency multiplier).
   - `Corner_DSP_PitchShiftOutOfBounds`: Pitch shift +24.0 semitones clamped safely to maximum allowed range [±12.0].

3. **Unicode, Vietnamese Characters & Special Symbols**:
   - `Corner_Unicode_VietnameseFilePaths`: Path `C:\Nhạc\Giai Điệu Trầm_01.wav` scanned, opened, and played without ANSI mojibake.
   - `Corner_Unicode_SpecialSymbolsInSearch`: Query `/tag:808&bass /bpm:120-130` handles ampersands and special symbols.
   - `Corner_Unicode_SqlInjectionInQuery`: Search query containing `'; DROP TABLE samples; --` safely escaped via SQLite prepared statements.
   - `Corner_Unicode_EmojisInMetadata`: Sample tags containing emojis (`🔥`, `🥁`, `✨`) stored and retrieved in UTF-8 cleanly.
   - `Corner_Unicode_VeryLongPath`: 500-character Windows deep directory path handled without buffer overflow.

4. **Empty Metadata, Null Embeddings & Scalability**:
   - `Corner_DB_EmptyLibrarySearch`: Search against empty database returns empty result list `{ok: true, results: []}`.
   - `Corner_DB_AllZeroEmbeddingVector`: Cosine similarity calculation against all-zero embedding returns 0.0, avoiding NaN.
   - `Corner_DB_HugeLibrary10kRecords`: Vector search over 10,000 sample records completes in < 15ms.
   - `Corner_DB_ConcurrentReadWrite`: 4 worker threads writing new samples while main thread queries search index.
   - `Corner_DB_DatabaseFileLocked`: Database opened in read-only filesystem reports readable error without crash.

---

### Tier 3: Cross-Feature Combinations

1. **Scanner + AI Tagging + Database + Syntax Search**:
   - Directory scanner ingests 50 procedural audio files -> AI engine classifies BPM, Key, Genres -> Records upserted into SQLite -> Syntax query `/bpm:120-130 /key:C` retrieves exact matching subset.
2. **AI Audio Embedding + Text Embedding + SIMD Cosine Search**:
   - Audio file embedded into 512-dim vector -> Stored in database -> Text query "lo-fi acoustic" converted to text embedding -> SIMD engine computes cosine distance and returns nearest sample with similarity > 0.70.
3. **Bridge RPC + DSP Engine Pitch Shifter + Mini Piano UI Event**:
   - UI dispatches `audio.setPitchShift` `{semitones: 4}` -> Bridge routes to `Engine::setPitchSemitones(4.0f)` -> SoundTouch changes pitch -> Audio callback delivers transposed audio -> Bridge pushes `audio.state` event back to UI.
4. **DAW BPM Change + Sync BPM Button + SoundTouch Real-Time Stretch**:
   - Mock REAPER changes project tempo from 120 to 144 BPM -> Bridge receives tempo update -> Time ratio adjusted to 1.20x -> SoundTouch renders stretched audio matching 144 BPM with 0 semitones pitch change.
5. **Unicode Directory Scan + Hash Checksum Cache + Re-Scan Invalidation**:
   - Vietnamese folder scanned -> Checksums stored in DB -> One file modified -> Re-scan detects changed hash -> Only the modified file is re-analyzed by AI, saving CPU time.

---

### Tier 4: Real-World Workload Scenarios

1. **Scenario 1: Producer Sample Pack Ingestion Workflow**:
   - A producer imports a 200-sample trap pack.
   - Scanner indexes files asynchronously across worker threads.
   - AI models extract tempo, key, subgenres (`Trap-EDM`, `Future Bass`), and CLAP vectors.
   - User types `/trap /bpm:140-150` into search bar, reviews top 10 results, and previews audio with DAW Sync enabled.
2. **Scenario 2: Live DJ / Remixer Quick Audition Workflow**:
   - User rapidly clicks through 30 sample previews (10–20 clicks per second).
   - Audio engine rapidly cancels and restarts playback streams without thread deadlocks or memory leaks.
   - User clicks Mini Piano Keyboard notes to match the key of the playing DAW track in real-time.
   - User clicks `Original Key` to reset pitch before dragging the sample onto a REAPER track.
3. **Scenario 3: Heavy Background Indexing Under Simultaneous Playback**:
   - Multi-threaded scanner indexes 5,000 files under high CPU load.
   - Concurrently, real-time audio playback streams smoothly without buffer underruns, glitches, or UI freezes.
4. **Scenario 4: Error Recovery & Chaos Engineering**:
   - Simulated network disconnection, missing AI model weights directory, corrupted SQLite file header, and locked file handles.
   - System displays informative UI toasts, falls back to algorithmic estimators, and maintains application stability without crashing.

---

## 5. Verification & Acceptance Criteria

To achieve `TEST_READY.md` certification:
1. **Compilation**: 100% clean compilation under MSVC `/W4` with zero warnings.
2. **Execution**: All Tier 1, Tier 2, Tier 3, and Tier 4 test suites pass (0 failures).
3. **Performance**: Full test suite executes in < 15 seconds.
4. **Deterministic Integrity**: Zero dummy implementations, zero hardcoded cheat values, 100% genuine algorithmic and state verification.
