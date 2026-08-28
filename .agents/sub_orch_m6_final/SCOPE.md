# Milestone 6 Scope: Final E2E Verification & Adversarial Hardening

## 1. Scope Overview
Milestone 6 is the final validation and quality assurance milestone for Reals Lab. Its objective is to certify the entire system across all tiers (Tier 1: Feature coverage, Tier 2: Boundary & Corner Cases, Tier 3: Cross-Feature Interactions, Tier 4: Real-World Workload Scenarios, Tier 5: Adversarial Coverage Hardening).

## 2. Test Suites to Validate
1. `TestSuite_AIInference`:
   - Essentia TempoCNN (120/174 BPM, beat onsets, RhythmExtractor2013 fallback)
   - Essentia EDMA Key (C Maj, A Min, Camelot 1A-12B, OpenKey, Temperley/Krumhansl voting)
   - Discogs-MAEST 400 subgenres (top-5 ranking, probability sum constraint, taxonomy mapping)
   - Mood-Jamendo 56 mood classification (multi-label activations, sigmoid outputs)
   - CLAP 512-dim embedding generation (audio/text vector alignment, unit L2 norm, cosine similarity)
   - ONNX Runtime C++ local host lifecycle & ModelManager SHA256 integrity

2. `TestSuite_AudioDSP`:
   - SoundTouch DSP processing (sample rate switching, mono/stereo buffers, throughput < 5ms)
   - Pitch-neutral time-stretching for DAW BPM synchronization (120 -> 140 BPM, dynamic ramping)
   - Real-time pitch shifting (±12 semitones, microtonal detuning, zero-semitone bit neutrality)

3. `TestSuite_DatabaseScanner`:
   - SQLite library schema initialization (`samples`, `tags`, `embeddings`, `folders`)
   - 512-float vector BLOB storage and byte-exact retrieval
   - xxHash64 / SHA256 checksum caching and fast-skip unchanged file mechanism
   - Multi-threaded background scanner pool with cancelation and progress reporting

4. `TestSuite_SearchEngine`:
   - Syntax `/` query parser (`/tag`, `/bpm:min-max`, `/key:val`, `/camelot:val`, `/fav`)
   - SQL query generation with parameterized safety
   - SIMD AVX2/SSE2 vectorized cosine similarity ranking across 10,000+ embeddings

5. `TestSuite_BridgeUI`:
   - JSON-RPC message contracts (`audio.*`, `ai.*`, `scanner.*`, `db.*`, `reaper.*`)
   - Player tag & mood badges row formatting (`#playerTagBar`)
   - Sync BPM button highlight state (`#btnSyncBpm`) & DAW tempo ratio math
   - Mini Piano Keyboard transposer popup (`#pianoTransposerPop`, 12 chromatic keys)
   - Original Key reset button (`#btnResetKey`) restoring 0 semitones
   - i18n dictionaries (Vietnamese & English) and theme token compliance

6. `TestSuite_BoundariesCorners`:
   - 0-byte audio files, truncated RIFF headers, silent digital zero buffers, DC offset clamping
   - Extreme BPMs (0, 10, 999), extreme pitch shifts (±12, ±24 clamped)
   - Vietnamese Unicode filepaths, SQL injection attempts, emoji tags, 500-char paths
   - Empty library search, all-zero embedding vectors, locked DB resilience

7. `TestSuite_CrossFeatures`:
   - Scanner -> AI Tagging -> DB -> Syntax Search integration
   - Audio embedding -> Text embedding -> SIMD Cosine Search integration
   - Bridge RPC -> DSP Pitch Shifter -> Mini Piano UI Event integration
   - DAW BPM Change -> Sync BPM Button -> SoundTouch Time-Stretch integration
   - Unicode Directory Scan -> Hash Cache -> Invalidation Re-scan

8. `TestSuite_EndToEndWorkflows`:
   - Producer sample pack batch ingestion workflow (200+ samples)
   - Live DJ / Remixer rapid audition & real-time piano transposition workflow
   - Heavy background indexing during active real-time audio playback
   - Chaos engineering: missing models, corrupted DB headers, locked handles

## 3. Tier 5 Adversarial Coverage Hardening
- Concurrency stress: 1,000+ rapid async bridge RPC calls and multi-threaded queries
- Rapid Piano Key clicks: 50+ rapid transposition changes in sub-millisecond intervals
- Extreme Pitch Shifts: Beyond-boundary clamping and continuous frequency modulation
- Heavy I/O contention: High-throughput background scanning while playing looping audio

## 4. Acceptance Criteria Verification
- [ ] A1: AI Analysis & Accuracy (Tempo, Key, Genres, Moods, CLAP Embeddings)
- [ ] A2: Search & Scanner Features (Syntax `/` filtering, Semantic CLAP search, Background scanner)
- [ ] A3: DSP Sync DAW & Realtime Pitch Shifting (Time-stretch 1.166x, <30ms latency, Original Key reset)
- [ ] A4: Build & Architectural Rules (Zero-warning C++20 build, proper layering, REAPER 7.x stability)
