# Test Scope Specification — Reals Lab E2E Testing Track

## 1. Objectives
Establish an exhaustive, production-grade, opaque-box test framework and test suites to validate all features of Reals Lab (R1 to R4) across four tiers:

## 2. Test Tiers Overview
- **Tier 1: Feature Coverage (>=5 test cases per feature)**
  - Feature 1: SoundTouch DSP Engine (initialization, processing, quality, channels, sample rates)
  - Feature 2: Pitch-Neutral Time-Stretch (tempo sync, ratios 0.5x to 2.0x, phase preservation, stretch without pitch drift)
  - Feature 3: Real-Time Pitch Shifter (±12 semitones, microtonal shifts, chromatic scale verification, low latency)
  - Feature 4: ONNX Runtime C++ Host (session creation, execution provider selection, tensor conversions, error handling, thread safety)
  - Feature 5: Model Weights Manager (path resolution, SHA256 integrity verification, missing model fallback, cache management, corrupt file recovery)
  - Feature 6: Essentia TempoCNN & Onsets (tempo extraction, confidence score, onset detection, odd time signatures, fallback extractor)
  - Feature 7: Essentia EDMA Key & Ensemble Voting (EDMA key detection, Temperley voting, Krumhansl voting, Camelot wheel mapping, OpenKey notation)
  - Feature 8: Discogs-MAEST 400 Subgenres (top-5 genre predictions, confidence thresholding, multi-genre classification, taxonomy lookup)
  - Feature 9: Mood-Jamendo Multi-Label (multi-label mood tagging, probability scoring, mood category aggregation, low-confidence filtering)
  - Feature 10: CLAP 512-dim Embeddings (audio embedding dimension/norm, text embedding dimension/norm, joint space projection, deterministic encoding)
  - Feature 11: SQLite Library Database (schema migration, sample CRUD, metadata indexing, vector BLOB persistence, transaction safety)
  - Feature 12: File Hash Checksum Cache (xxHash64/SHA256 calculation, modification time tracking, fast skip on unchanged files, cache invalidation)
  - Feature 13: Multi-Threaded Background Scanner (directory traversal, task queuing, worker concurrency, scan progress reporting, cancellation)
  - Feature 14: Syntax `/` Query Parser (tag prefix `/tag`, BPM range `/bpm:120-130`, key filter `/key:F#m`, Camelot filter `/camelot:8B`, composite boolean queries)
  - Feature 15: SIMD Cosine Semantic Search (vector dot product AVX2/SSE2, cosine similarity ranking, top-K selection, normalization, fallback scalar)
  - Feature 16: Bridge RPC Extended Contracts (`audio.*`, `ai.*`, `scanner.*`, `db.*`, `search.*` schema validation, error codes, event streams)
  - Feature 17: Player Tag & Mood Badges (badge list generation, color categorization, dynamic update on track change, empty tag handling)
  - Feature 18: Sync BPM Button Highlight & Logic (toggle state persistence, DAW tempo synchronization math, ratio calculation, out-of-sync alert)
  - Feature 19: Mini Piano Keyboard Transposer (chromatic note mapping, semitone offset calculation, UI state reflection, realtime pitch callback)
  - Feature 20: Original Key Reset (reset state transition, 0 semitone restoration, UI button reset, rapid toggle resilience)
  - Feature 21: Responsive UI & REAPER Docking (window state handlers, layout dimension changes, docking event dispatch)
  - Feature 22: E2E Verification & Hardening (full pipeline integration, zero crash stability)

- **Tier 2: Boundary & Corner Cases (>=5 test cases per feature area)**
  - 0-byte audio and database files
  - Silent audio buffers (digital zero) & DC offset
  - Corrupted audio headers (truncated RIFF/WAV, invalid channels, bad chunk sizes)
  - Boundary BPMs (0 BPM, 10 BPM, 300 BPM, 999 BPM, non-integer BPMs like 127.354 BPM)
  - Extreme pitch shifts (+12, -12, 0, fractional semitones, out-of-bound requests)
  - Unicode/Vietnamese filenames and paths (`Nhạc_Mới_2026/Giai_Điệu_Trầm.wav`)
  - Empty queries, whitespace-only, malformed syntax tokens (`/bpm:abc`, `/key:XYZ`)
  - Scalability: 10,000+ sample database queries under memory and CPU constraints
  - Concurrency: Multiple background threads reading/writing database while audio plays

- **Tier 3: Cross-Feature Combinations**
  - Scanner + Hash Checksum + SQLite Database + Syntax Search
  - AI Embedding Extraction + SQLite Storage + SIMD Cosine Search + Ranking
  - Bridge RPC Request + Audio Engine Pitch Shift + DAW Tempo Sync + Event Push
  - Unicode File Path -> Background Scan -> Metadata Parsing -> Bridge Notification -> UI Player Load
  - Multi-threaded Scanning concurrent with Live Audio Playback and Interactive Search

- **Tier 4: Real-World Workload Scenarios**
  - Scenario 1: Producer Sample Pack Ingestion & AI Auto-Tagging Workflow
  - Scenario 2: Live Performance / DJ Tempo Matching & Key Transposition Workflow
  - Scenario 3: Heavy Multi-Source Library Indexing with Concurrent Search
  - Scenario 4: Error Recovery & Graceful Degradation (missing models, corrupt DB, read-only FS, network timeout)
