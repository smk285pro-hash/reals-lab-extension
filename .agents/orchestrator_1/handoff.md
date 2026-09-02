# Master Handoff Report: Comprehensive Audio & File Navigation Diagnostics & Fix Roadmap

**Orchestrator**: `orchestrator_1` (Project Orchestrator)  
**Date**: 2026-09-02  
**Status**: All Milestones M1, M2, M3, M4 Completed Successfully (100% Verified)  
**Artifacts Generated**:
- `tests/benchmarks/TestSuite_EmpiricalBenchmark_M4.cpp` (Empirical Verification Suite)
- `.agents/teamwork_preview_explorer_m1_1/handoff.md` (Key/Tone Transposition Deep Audit)
- `.agents/teamwork_preview_explorer_m2_1/handoff.md` (BPM Detection & Time-Stretch Deep Audit)
- `.agents/teamwork_preview_explorer_m3_1/handoff.md` (File Browser Metadata Hydration Deep Audit)
- `.agents/teamwork_preview_worker_m4_1/handoff.md` (Empirical Benchmark Execution Report)

---

## 1. Executive Summary & Root Cause Synthesis

### 1.1 Root Cause Summary by Domain

| Domain | Problem Observed | Exact Root Causes in Source Code | Empirical Impact |
|---|---|---|---|
| **R1. Key/Tone Transposer & Root Note Fallback** | Unlabelled samples default to 'C'; selecting piano target key causes severe pitch distortion. Minor keys stripped to major. | 1. **10 Hardcoded Fallback Sites**: `ui-web/app.js:1234, 2482, 3207`, `Bridge.cpp:270, 1310`, and `KeyDetector.cpp:74` force $R_{\text{assumed}} = 'C'$.<br>2. **Bridge Regex Mode Stripping**: `Bridge.cpp:265` uses non-capturing group for mode `(?:m\|maj\|min\|minor\|major)?`, returning only tonic note $km[1]$ (`Am` $\rightarrow$ `"A"`).<br>3. **Frontend Spaced Mode Dropping**: `app.js:1304` regex leaves mode outside $match[1]$ (`"C minor"` $\rightarrow$ `"C"`).<br>4. **Missing Enharmonics**: `Cb`, `B#`, `E#`, `Fb` missing in normalizers. | **91.67% Dissonance Rate** (132/144 transitions out of tune); mean pitch error of **3.000 semitones**; worst-case **6.000 semitones** (Tritone clash). |
| **R2. Algorithmic BPM Detection & Time-Stretch** | Unlabelled loops fail true tempo or produce octave errors (half-time/double-time); preview & DAW drag-and-drop lose sync. | 1. **Comb Filter Lag Asymmetry**: `TempoDetector.cpp:160` boosts short lags ($120–240\text{ BPM}$) by $+75\%$ harmonic energy while giving $+0\%$ to long lags ($40–70\text{ BPM}$), biasing toward $2\times$ octave doubling ($70 \rightarrow 139.5\text{ BPM}$).<br>2. **Unweighted Linear Spectral Flux**: `FeatureExtractor.cpp:328` linear STFT flux allows high-frequency hats to overpower kick transients, locking arpeggios to 16th-note sub-beats.<br>3. **Destructive Fallback in `Bridge.cpp:926`**: Sets `sampleBpm = projectBpm`, resulting in $ratio = 1.0\text{x}$ (zero time-stretch) for unlabelled loops.<br>4. **Regex False Positives**: `Bridge.cpp:99` matches arbitrary numbers in sample names and full folder paths (`Pack_90/` $\rightarrow 90\text{ BPM}$). | **60.6% Misclassification Rate**; octave doubling on $70\text{ BPM}$; sub-beat locking on melodic stems creating up to **7.734s (66.00 16th beats) of timeline grid misalignment** in REAPER. |
| **R3. File Browser Metadata Hydration & DB Sync** | File browser displays bare filenames with no BPM/Key badges, forcing frontend into filename regex heuristics. | 1. **Architectural Model Gap**: `BrowserModel::FileEntry` lacks fields for `bpm`, `keyRoot`, `keyMode`, `camelot`, `genre`, `mood`, `durationSec`.<br>2. **Unconnected Bridge Pipeline**: `Bridge::handleFsList` calls `model.listDir` and passes entries straight to `entryToJson` without querying `db::Database`.<br>3. **Missing Indexing**: SQLite `samples` table has `idx_samples_path` but lacks directory batch indexing. | **0.0% BPM, Key, and Duration coverage** during directory navigation, directly starving the UI and triggering all R1 & R2 fallback bugs. |

---

## 2. Empirical Benchmark Verification Results (R4)

### 2.1 12 Chromatic Key Detection & Transposition Matrix
- **KeyDetector Accuracy**: **87.5% (21/24 keys)** on synthetic harmonic scales with 0.927 mean confidence.
- **Transposition Error Matrix (144 Cells)**:
  - Correct Transpositions: **12 / 144 (8.33%)** (only when actual root is C).
  - Dissonant Transpositions: **132 / 144 (91.67%)**.
  - Pitch error formula: $\text{Error} = R_{\text{actual}} \pmod{12}$.

### 2.2 70–175 BPM Tempo Detection & Time-Stretch Error
- **Exact / $\pm 1$ BPM Pass**: **39.4% (13/33 stems)**.
- **Octave Doubling ($2\times$)**: **9.1% (3/33)** (Systematic on $70\text{ BPM} \rightarrow 139.5\text{ BPM}$).
- **Octave Halving ($0.5\times$)**: **6.1% (2/33)** (Systematic on $175\text{ BPM} \rightarrow 87.6\text{ BPM}$).
- **Severe Sub-Beat Harmonic Locking**: **45.5% (15/33)**.
- **Max REAPER Grid Misalignment**: **7.7344 seconds (66.00 16th beats)**.

### 2.3 SQLite Metadata Hydration Benchmark
- **Coverage**: Bare `fs.list` = **0.0%** $\rightarrow$ Hydrated `db::Database` = **100.0%** (BPM, Key, Camelot, Duration).
- **Latency**: 50 files = 3.79ms, 100 files = 9.74ms, 500 files = 29.33ms, 1000 files = 53.41ms.

---

## 3. Prioritized Architectural Fix Roadmap

```
                                  FIX ROADMAP ARCHITECTURE
  
   [ Layer 1: Core / AI DSP ]
   ├── KeyDetector: Relative Major/Minor Chroma Triad Weighting + Enharmonic Expansion
   └── TempoDetector: 3-Band Log-Flux + Bayesian 120-BPM Prior + Bar-Length Snapping
                │
                ▼
   [ Layer 2: Core Database & Browser Model ]
   ├── FileEntry: Add bpm, keyRoot, keyMode, camelot, durationSec, isIndexed
   ├── Database: Add batch query `Database::getSamplesByPaths(vector<string>)`
   └── BrowserModel: Validate cache mtime against SQLite records
                │
                ▼
   [ Layer 3: Bridge Router ]
   ├── Bridge::handleFsList: Hydrate FileEntry vector from db::Database before serializing
   ├── entryToJson: Output bpm, key, camelot, duration, genre, mood
   ├── Regex Fixes: Strict token matching; eliminate mode-stripping; restrict to filename
   └── Fallback Safety: Unify fallback to explicit 0.0 with toast alerts (never mutate projectBpm)
                │
                ▼
   [ Layer 4: Web UI Frontend ]
   ├── Transposer State: Support Relative Shift Mode (±12 st) when root is unknown
   ├── Piano Keyboard: Live badge updates & automatic KeyDetector trigger
   └── Regex Fixes: Parse spaced modes ("C minor"), Camelot tokens ("8A"), full enharmonics
```

### Phase 1: High Priority (P0) — Critical Bug Fixes (Zero Architecture Overhead)
1. **Fix Bridge Minor Mode Stripping** (`bridge/src/Bridge.cpp:265`):
   - Update regex to capture mode suffix into $km[2]$ and append to normalized key string (`"Am"`, `"F#m"`).
2. **Fix Frontend Spaced Mode Dropping** (`ui-web/app.js:1304`):
   - Include `(?:\s+(?:maj|min|minor|major))` inside capturing group $match[1]$.
3. **Fix Regex False Positives on File/Directory Paths** (`bridge/src/Bridge.cpp:99`):
   - Restrict regex search to `platform::pathToUtf8(p.filename())` (never directory path).
   - Require explicit `bpm`/`tempo` boundary anchors.
4. **Fix Destructive Fallback in `Bridge.cpp:926`**:
   - Do NOT set `sampleBpm = projectBpm`. Set `ratio = 1.0f` and keep `sampleBpm = 0.0f` to signal unknown tempo.

### Phase 2: High Priority (P1) — Database Metadata Hydration Pipeline
1. **Extend `FileEntry` struct** (`core/include/reals/browser/BrowserModel.h`):
   - Add `float bpm = 0.0f;`, `std::string key;`, `std::string camelot;`, `double durationSec = 0.0;`.
2. **Implement Batch Query in `Database`** (`core/include/reals/db/Database.h`, `core/src/db/Database.cpp`):
   - Add `std::unordered_map<std::string, SampleRecord> getSamplesByPaths(const std::vector<std::string>& paths);`.
3. **Hydrate in `Bridge.cpp` (`fs.list`)**:
   - In `Bridge::handleFsList`, extract all audio paths, query `db.getSamplesByPaths(paths)`, and populate `FileEntry` metadata.
4. **Update `entryToJson`** (`bridge/src/Bridge.cpp:58`):
   - Serialize `e["bpm"]`, `e["key"]`, `e["camelot"]`, `e["duration"]`.

### Phase 3: Medium Priority (P2) — Algorithmic AI & DSP Enhancements
1. **Multi-Band Logarithmic Spectral Flux in `FeatureExtractor.cpp`**:
   - Divide STFT into 3 bands (Low: 20–250 Hz, Mid: 250–4000 Hz, High: 4–20 kHz) with logarithmic compression $\log(1 + 100 \cdot |X|)$.
2. **Bayesian Tempo Prior in `TempoDetector.cpp`**:
   - Replace hard cutoff with Log-Normal prior $W(\text{bpm}) = \exp\left(-\frac{1}{2}\left(\frac{\log_2(\text{bpm}/120.0)}{0.6}\right)^2\right)$.
3. **Bar-Length Constraint Snapping**:
   - Snap detected BPM to $\text{BPM}_{\text{bar}} = \frac{240 \cdot b}{\text{Duration}}$ for cleanly trimmed loops.
4. **Extend Scanner Decode Window**:
   - Increase `BackgroundScanner.cpp` decode limit from 8s to 20s.

### Phase 4: Frontend UI Polish & Transposition Safety (P2)
1. **Relative vs Absolute Transposition Mode** (`ui-web/app.js`):
   - When key is unknown, display `"Root: Unknown (Relative Mode)"` and allow direct $\pm 12$ semitone pitch shifting without assuming 'C'.
2. **Automatic Background Key Detection on Selection**:
   - Trigger `audio.detectKey` immediately when an unlabelled sample is selected, updating the root note before the user interacts with the piano keyboard.

---

## 4. Verification & Testing Instructions
```powershell
# 1. Build full test suite with empirical benchmarks
cmake --build --preset windows --target reals_tests

# 2. Run M4 Empirical Benchmark Suite
.\build\windows\tests\Debug\reals_tests.exe --suite=EmpiricalBenchmark_M4

# 3. Run all unit tests
ctest --preset windows --output-on-failure
```
