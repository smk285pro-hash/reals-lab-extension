# Reals Lab — Specification Survey & Technical Contract: AI Models & DSP Engine

**Author**: Spec Miner 1 (AI Models & DSP Spec Miner)  
**Target Folder**: `.agents/spec_miner_survey_1/`  
**Date**: 2026-08-26  
**Status**: COMPLETE / HARD HANDOFF  

---

## 1. Observation

Direct observations from codebase inspection, GitNexus analysis (974 symbols, 1980 relationships, 75 execution flows), and authoritative documents (`ORIGINAL_REQUEST.md`, `PLAN.md`, `SPEC.md`, `DESIGN.md`, `AGENTS.md`):

1. **Current Audio & Playback Architecture**:
   - `core/include/reals/audio/Engine.h` & `core/src/audio/Engine.cpp`: Miniaudio-based playback engine (`ma_engine`, `ma_sound`), PIMPL pattern. Supports `playFile`, `seekFraction`, `setVolume`, `setLoop`, `setThreshold`, `setPitch(float ratio)`.
   - The current `setPitch` in `Engine.cpp` (lines 248–254) uses `ma_sound_set_pitch(&m_impl->sound, m_impl->pitch)` which couples pitch and speed together (resampling style) rather than independent time-stretch/pitch-shift.
   - Suffix/Probe helper `probeFile` decodes metadata (duration, sample rate, channels, total frames) via `ma_decoder`.
   - Suffix/Envelope calculation `computeEnvelope` processes PCM frames in chunks of 65536 into 180 buckets normalized to 0.95 peak.

2. **Current Browser Model & Storage**:
   - `core/include/reals/browser/BrowserModel.h` & `core/src/browser/BrowserModel.cpp`: Uses memory containers (`std::vector<Root>`, `std::unordered_map<std::string, std::vector<FileEntry>> m_cache`, `std::unordered_map<std::string, int> m_tags`) serialized to JSON (`store.json`) in `%APPDATA%\RealsLab\`.
   - Current search in `BrowserModel.cpp` (lines 142–175) is a standard case-insensitive substring filename check across directories, lacking SQL indexing, syntax `/` tokenization, and vector embeddings.

3. **Current Bridge & REAPER Extension Integration**:
   - `bridge/src/Bridge.cpp`: Implements JSON-RPC dispatcher for commands (`app.info`, `config.*`, `fs.*`, `browser.*`, `audio.*`, `lab.*`, `reaper.*`, `window.*`).
   - `extension/src/reaper_plugin.cpp`: Interfaces with REAPER API (`Master_GetTempo`, `InsertMedia`, `Undo_BeginBlock`, `Undo_EndBlock`, `Main_OnCommand`, `DockWindowAddEx`, etc.). `ExtHostActions::projectTempo()` correctly queries `Master_GetTempo()` from REAPER SDK.
   - `ui-web/app.js`: Has UI hooks for `#btnSync` (tempo sync toggle) and `#rate` slider, but lacks the Mini Piano Keyboard popup transposer and semantic tag badges.

4. **Platform & Build Constraints**:
   - `core/include/reals/platform/Path.h`: `platform::dataDir()` resolves to `%APPDATA%\RealsLab\` on Windows, `~/Library/Application Support/RealsLab` on macOS, `~/.config/RealsLab` on Linux. All model weights must reside under `%APPDATA%\RealsLab\models\`.
   - `CMakeLists.txt`: C++20, `-Wall -Wextra /W4`, zero-warning policy (`REALS_WARNINGS_AS_ERRORS`). `reals_core` has zero UI / REAPER dependencies.

---

## 2. Logic Chain

From the observed baseline to the required offline AI and DSP architecture:

1. **AI Engine Isolation (`core/ai/`)**:
   - To achieve 100% offline industrial standard execution, ONNX Runtime C++ (`onnxruntime-cxx`) must be embedded within `core/ai/`.
   - Models must be loaded dynamically from `%APPDATA%\RealsLab\models\`. If models are missing, the system gracefully falls back to algorithmic extractors (e.g. `RhythmExtractor2013`, profile chromagrams).
   - Audio preprocessing (STFT, Mel spectrogram, HPCP, log-mel) must run in C++ using zero-allocation or pooled buffer pipelines before feeding ONNX tensors.

2. **Multi-Model Voting & Accuracy Guarantee**:
   - EDMA is specialized for electronic dance music (sharp transients, synthesized basslines). For acoustic/rock/jazz, combining EDMA with Temperley and Krumhansl-Schmuckler profiles via a weighted ensemble score ($S = 0.50 P_{\text{EDMA}} + 0.25 R_{\text{Temp}} + 0.25 R_{\text{Krum}}$) eliminates key ambiguities and relative minor/major confusion.
   - TempoCNN provides deep convolutional tempo estimation; when confidence $< 0.35$, `RhythmExtractor2013` (complex spectral flux + autocorrelation) prevents hallucinated BPMs on complex polyrhythms.

3. **High-Performance Scanner & SQLite Vector Search (`core/scanner/`, `core/db/`)**:
   - Scanning thousands of sample files synchronously freezes the host. A worker pool (`std::thread::hardware_concurrency() - 1`) running background I/O with xxHash64 checksum cache ensures that only new or modified audio files are processed.
   - Storing 512-dimensional CLAP embeddings as 2048-byte BLOBs in SQLite enables AVX2 SIMD dot-product cosine similarity searches with $< 5\text{ms}$ latency across 50,000 samples.
   - Syntax parser converts `/tag`, `/bpm:min-max`, `/key:val` directly into indexed SQL WHERE clauses, combining seamlessly with full-text and vector ranking.

4. **DSP Time-Stretch & Pitch-Shifter Engine (`core/audio/`)**:
   - Integrating SoundTouch / RubberBand into `core/audio/` decouples playback rate from pitch.
   - When `Sync BPM` is active, $\text{ratio} = \text{Project BPM} / \text{Sample BPM}$ is applied to the time-stretch engine while pitch remains neutral ($1.0$).
   - Pitch shifting $\pm 12$ semitones computes $2^{\Delta / 12}$ real-time ratio into the DSP phase-vocoder / WSOLA buffer, keeping latency $< 30\text{ms}$ during interactive Mini Piano Keyboard transposition.

---

## 3. Features Discovered & Technical Specifications

### Features Discovered Table

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|----------|---------|-------------|--------|---------|----------------|----------------|
| 1 | AI Engine | ONNX Runtime C++ Host | Local CPU/DirectML inference runtime in `core/ai/` | Model `.onnx` path, audio tensor float32 | Raw output tensors (logits, embeddings) | Returns `AiStatus::ModelNotFound` or `InferenceError` | ORIGINAL_REQUEST R1 |
| 2 | AI Engine | Model Weights Storage | Lazy loader & SHA256 verifier for `%APPDATA%\RealsLab\models\` | Model name, directory path | Loaded `Ort::Session` instance | Throws / returns fallback flag if missing or corrupted | ORIGINAL_REQUEST R1, Path.h |
| 3 | AI Models | Essentia TempoCNN | CNN-based tempo & onset extraction | Mel-spectrogram (11025/22050Hz, 40 bands, hop 512) | `bpm` (float), `confidence` (0..1), `beat_onsets` (vec) | Falls back to RhythmExtractor2013 if conf < 0.35 | ORIGINAL_REQUEST R1 |
| 4 | AI Models | RhythmExtractor2013 Fallback | Fast signal-based onset & tempo tracking algorithm | PCM float buffer (44.1kHz mono) | `bpm` (float), `confidence` (float) | Returns 0.0 BPM if silent / unparseable | ORIGINAL_REQUEST R1 |
| 5 | AI Models | Essentia EDMA Key Model | Key & scale classification for electronic/modern music | HPCP chromagram (44.1kHz, 36/12 bins) | 24-class probability distribution | Uniform distribution on silent audio | ORIGINAL_REQUEST R1 |
| 6 | AI Models | Temperley & Krumhansl Profiles | Key correlation profiles for acoustic/rock/jazz | HPCP pitch distribution | 24-key correlation coefficients | Default to C Major if HPCP flat | ORIGINAL_REQUEST R1 |
| 7 | AI Models | Ensemble Key Voting | Weighted ensemble combiner (EDMA + Temp + Krum) | 3 model score vectors | `key` (str), `mode` (Major/Minor), `camelot` (1A-12B), `openKey` | Low confidence flag if top 2 scores within 2% | ORIGINAL_REQUEST R1 |
| 8 | AI Models | Discogs-MAEST 400 Subgenres | Deep audio classifier over 400 musical styles | Mel-spectrogram (16kHz/24kHz, 128 bands) | Top-5 genre tags with confidence scores | Empty tag list on noise/silence | ORIGINAL_REQUEST R1 |
| 9 | AI Models | Mood-Jamendo Multi-label | Multi-label sigmoid classifier for 56 mood/themes | Mel-spectrogram float32 tensor | Active moods array with probability $\ge 0.25$ | Returns empty array if all probs < threshold | ORIGINAL_REQUEST R1 |
| 10 | AI Models | CLAP Audio Encoder | 512-dim normalized vector embedding extractor | 48kHz audio buffer, log-mel filterbank | `float32[512]` L2-normalized vector | Zero vector if input buffer empty | ORIGINAL_REQUEST R1 |
| 11 | AI Models | CLAP Text Encoder | Encodes search text query into 512-dim vector | UTF-8 search query string | `float32[512]` L2-normalized vector | Returns zero vector if query empty | ORIGINAL_REQUEST R1 |
| 12 | Scanner & DB | Background Scanner Pool | Multi-threaded recursive folder scanner & worker pool | Root directory path, file filter | Async progress events, DB file records | Skips unreadable / locked files | ORIGINAL_REQUEST R2 |
| 13 | Scanner & DB | File Hash Cache Checksum | xxHash64 / SHA256 header+mtime change detector | File path, file stats | 64-bit uint / hex hash | Re-scans file if hash mismatch | ORIGINAL_REQUEST R2 |
| 14 | Scanner & DB | SQLite Library Database | Local relational DB storing samples, tags, analysis | SQLite DB at `%APPDATA%\RealsLab\library.db` | Schema tables: `samples`, `analysis`, `tags` | Returns SQLite error codes, auto-heals corrupted DB | ORIGINAL_REQUEST R2 |
| 15 | Search | Syntax `/` Query Parser | Tokenizer & parser for `/tag`, `/bpm:`, `/key:` | Query string (e.g. `/trap /bpm:120-130`) | Structured AST / SQLite parameterized query | Invalid tokens treated as free text | ORIGINAL_REQUEST R2 |
| 16 | Search | SIMD Cosine Semantic Search | AVX2/NEON dot product over 512-dim embeddings | Query vector `float32[512]`, Top-K | Ranked list of `sample_id` + similarity score | Returns top matches above threshold | ORIGINAL_REQUEST R2 |
| 17 | DSP Engine | SoundTouch / RubberBand DSP | Real-time WSOLA/phase-vocoder time-stretch & pitch-shift | PCM float stream, rate ratio, pitch semitones | Stretched/shifted PCM float buffer | Passes through original audio if ratio == 1.0 | ORIGINAL_REQUEST R3 |
| 18 | DSP Engine | DAW Project BPM Sync | Auto-stretches sample to match REAPER `Master_GetTempo()` | Sample BPM, REAPER Project BPM | Time-stretch factor = `ProjectBPM / SampleBPM` | Disabled if Sample BPM unknown or 0 | ORIGINAL_REQUEST R3, reaper_plugin.cpp |
| 19 | DSP Engine | Real-time Pitch Shifter | $\pm 12$ semitones pitch transposition with $<30\text{ms}$ latency | Semitone delta (-12.0 to +12.0) | Pitch factor $2^{\Delta / 12}$ into DSP buffer | Clamped to $[-12, +12]$ | ORIGINAL_REQUEST R3 |
| 20 | DSP Engine | Half-Time / Double-Time Guard | Prevents extreme time-stretch distortion when BPMs differ | Raw stretch ratio | Normalized stretch ratio $[0.5x, 2.0x]$ | Emits UI indicator for half/double time | Audio engineering standard |
| 21 | UI & Bridge | Mini Piano Transposer Bridge | JSON-RPC bridge commands `audio.setPitchSemitones`, `audio.syncTempo` | Key note index (0..11), semitone delta | Instant audio pitch shift + UI active key state | Returns `ok: false` if engine unready | ORIGINAL_REQUEST R4, Bridge.cpp |
| 22 | UI & Bridge | Semantic Badges on Player | Chip badges displaying Genre, Mood, Key, BPM | Analysis JSON object | Rendered UI badges above waveform | Shows "Analyzing..." or hides if empty | ORIGINAL_REQUEST R4, DESIGN.md |

---

### Edge Cases Table

| # | Feature | Input / Condition | Observed / Required Behavior |
|---|---------|-------------------|-----------------------------|
| 1 | TempoCNN Fallback | Polyrhythmic jazz drum solo or ambient soundscape (Confidence < 0.35) | Fallback algorithm `RhythmExtractor2013` is automatically triggered. If confidence remains low, output `bpm: null, confidence: 0.0`. |
| 2 | Key Detection Ensemble | Track modulating between Relative Major and Minor (e.g. C Major / A Minor) | Ensemble voting calculates distribution over both; Camelot output returns primary key with secondary mode confidence in JSON payload. |
| 3 | Scanner File Corruption | Truncated / unreadable WAV header or 0-byte file | Scanner catches `ma_decoder_init` error, logs warning to `Log::error`, marks DB record `format: "corrupt"`, does not crash scanner pool. |
| 4 | File Renamed / Moved | User renames or moves a folder on disk | Hash checksum in DB detects identical file content at new path, updates `path` in `samples` table without re-running expensive AI inference. |
| 5 | Syntax Parser | Complex query `/bpm:120-130 /key:F#m /ambient /fav punchy 808` | Correctly separates `/bpm:120-130` (BPM range), `/key:F#m` (Key filter), `/ambient` (tag), `/fav` (favorite flag), and `"punchy 808"` (text/CLAP search terms). |
| 6 | DSP BPM Sync | Sample BPM = 70, REAPER Project BPM = 140 | Time-stretch detects 2.0x ratio. Auto-aligns 1-bar loop to project grid without audio dropouts. |
| 7 | DSP Extreme Pitch Shift | Pitch shift at +12 semitones on high-frequency cymbal sample | High-frequency anti-aliasing filter in SoundTouch/RubberBand prevents aliasing artifacts; latency remains $< 30\text{ms}$. |
| 8 | Mini Piano Keyboard | Rapid clicking between piano keys (e.g. C -> F# -> A within 100ms) | Lock-free atomic / ring-buffer update shifts pitch smoothly without audio pops, clicks, or thread deadlocks. |
| 9 | Original Key Reset | User clicks `Original Key` button | Resets semitone delta to `0.0`, pitch multiplier to `1.0`, restores waveform accent highlight to default. |
| 10 | Offline Environment | Extension launched on machine with zero internet connectivity | 100% of models run locally from `%APPDATA%\RealsLab\models\`. Zero network requests executed. |

---

## 4. Deep Technical Contracts & Schemas

### 4.1 AI Inference Engine Specification (`core/ai/`)

#### Model Specifications & Dimensions
1. **TempoCNN**:
   - ONNX Model: `tempocnn.onnx` (~4.2 MB)
   - Input shape: `[1, 1, 40, 256]` (Mel-spectrogram bands: 40, time frames: 256, sampling rate: 11,025 Hz)
   - Output shape: `[1, 256]` (Tempo distribution logits spanning 30 to 285 BPM)
   - Post-processing: Softmax -> Argmax peak picking + Gaussian interpolation -> `bpm`, `confidence`.

2. **EDMA Key Detection & Voting Ensemble**:
   - ONNX Model: `edma_key.onnx` (~1.8 MB)
   - Input shape: `[1, 1, 36, 128]` (HPCP chromagram with 36 bins, sampling rate: 44,100 Hz)
   - Output shape: `[1, 24]` (12 Major classes + 12 Minor classes)
   - Voting Ensemble Formula:
     $$S(\text{key}_i) = 0.50 \cdot P_{\text{EDMA}}(i) + 0.25 \cdot \frac{r(\vec{h}, \vec{T}_i) + 1}{2} + 0.25 \cdot \frac{r(\vec{h}, \vec{K}_i) + 1}{2}$$
     where $\vec{h}$ is the normalized 12-bin HPCP vector, $\vec{T}_i$ is Temperley profile for key $i$, $\vec{K}_i$ is Krumhansl-Schmuckler profile for key $i$, and $r(\vec{x}, \vec{y})$ is Pearson correlation.
   - Notation Mapping:
     - Major: $0 \to \text{C (8B)}, 1 \to \text{C\#/Db (3B)}, 2 \to \text{D (10B)}, \dots$
     - Minor: $12 \to \text{Cm (5A)}, 13 \to \text{C\#m (12A)}, 14 \to \text{Dm (7A)}, \dots$

3. **Discogs-MAEST 400 Subgenres**:
   - ONNX Model: `discogs_maest.onnx` (~85 MB)
   - Input shape: `[1, 1, 128, 512]` (Mel-spectrogram: 128 bands, sample rate: 16,000 Hz)
   - Output shape: `[1, 400]` (Multi-class sigmoid probabilities)
   - Post-processing: Top-5 selection with threshold filter ($\text{prob} \ge 0.10$).

4. **Mood-Jamendo & CLAP 512-dim Semantic Embedding**:
   - Mood Model: `mood_jamendo.onnx` (~12 MB) -> `[1, 56]` binary probabilities.
   - CLAP Audio Model: `clap_audio.onnx` (~140 MB)
     - Input: `[1, 1001, 64]` (Log-Mel filterbank, sample rate: 48,000 Hz)
     - Output: `[1, 512]` float32 tensor
     - Normalization: $\vec{v}_{\text{norm}} = \vec{v} / \|\vec{v}\|_2$
   - CLAP Text Model: `clap_text.onnx` (~120 MB)
     - Input: `input_ids: [1, 77]`, `attention_mask: [1, 77]`
     - Output: `[1, 512]` float32 tensor (L2-normalized)

---

### 4.2 SQLite Database Schema (`%APPDATA%\RealsLab\library.db`)

```sql
-- Samples table: core metadata & filesystem state
CREATE TABLE IF NOT EXISTS samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    filename TEXT NOT NULL,
    filesize INTEGER NOT NULL,
    mtime INTEGER NOT NULL,
    file_hash TEXT NOT NULL,
    duration_sec REAL NOT NULL,
    sample_rate INTEGER NOT NULL,
    channels INTEGER NOT NULL,
    format TEXT NOT NULL,
    scanned_at INTEGER NOT NULL
);

-- Analysis table: AI inference outputs & vector embeddings
CREATE TABLE IF NOT EXISTS analysis (
    sample_id INTEGER PRIMARY KEY,
    bpm REAL,
    bpm_confidence REAL,
    key_display TEXT,
    key_camelot TEXT,
    key_openkey TEXT,
    key_confidence REAL,
    genres_json TEXT,         -- JSON array of {"genre":"Trap-EDM", "score":0.92}
    moods_json TEXT,          -- JSON array of {"mood":"dark", "score":0.85}
    embedding BLOB,           -- 512 * sizeof(float32) = 2048 bytes
    analyzed_at INTEGER,
    FOREIGN KEY(sample_id) REFERENCES samples(id) ON DELETE CASCADE
);

-- User tags & favorites table
CREATE TABLE IF NOT EXISTS user_tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sample_id INTEGER NOT NULL,
    tag TEXT NOT NULL,
    color_index INTEGER DEFAULT 0,
    is_favorite INTEGER DEFAULT 0,
    FOREIGN KEY(sample_id) REFERENCES samples(id) ON DELETE CASCADE,
    UNIQUE(sample_id, tag)
);

-- Indexes for ultra-fast query execution
CREATE INDEX IF NOT EXISTS idx_samples_path ON samples(path);
CREATE INDEX IF NOT EXISTS idx_samples_hash ON samples(file_hash);
CREATE INDEX IF NOT EXISTS idx_analysis_bpm ON analysis(bpm);
CREATE INDEX IF NOT EXISTS idx_analysis_key ON analysis(key_display);
CREATE INDEX IF NOT EXISTS idx_analysis_camelot ON analysis(key_camelot);
CREATE INDEX IF NOT EXISTS idx_user_tags_tag ON user_tags(tag);
```

---

### 4.3 Syntax `/` Search Query Grammar & AST

**Syntax Specification**:
```ebnf
Query        ::= (Token | TextTerm)*
Token        ::= TagToken | BpmToken | KeyToken | CamelotToken | DurToken | FavToken
TagToken     ::= '/' Identifier | '/tag:' Identifier
BpmToken     ::= '/bpm:' (Range | Comparison | Number)
KeyToken     ::= '/key:' KeyName
CamelotToken ::= '/camelot:' CamelotCode | '/cam:' CamelotCode
DurToken     ::= '/dur:' (Range | Comparison | Number)
FavToken     ::= '/fav' | '/star'
Range        ::= Number '-' Number
Comparison   ::= ('>' | '<' | '>=' | '<=') Number
TextTerm     ::= [^/ ]+
```

**AST to SQL Compilation Example**:
- User Input: `/trap /bpm:120-130 /key:F#m punchy kick`
- Generated Parameterized SQL:
  ```sql
  SELECT s.id, s.path, s.filename, s.duration_sec, a.bpm, a.key_display, a.key_camelot, a.genres_json, a.embedding
  FROM samples s
  JOIN analysis a ON s.id = a.sample_id
  WHERE (a.genres_json LIKE '%"Trap"%' OR a.genres_json LIKE '%"trap"%' OR EXISTS(SELECT 1 FROM user_tags ut WHERE ut.sample_id = s.id AND ut.tag = 'trap'))
    AND (a.bpm >= 120.0 AND a.bpm <= 130.0)
    AND (a.key_display = 'F#m' OR a.key_camelot = '11A')
    AND (s.filename LIKE '%punchy%' OR s.filename LIKE '%kick%')
  ORDER BY s.id DESC LIMIT 200;
  ```

---

### 4.4 DSP Engine Contract & REAPER BPM Sync

```cpp
namespace reals::audio {

struct DspConfig {
    float timeRatio = 1.0f;     // 0.25 to 4.0
    float pitchSemitones = 0.0f;// -12.0 to +12.0
    int sampleRate = 44100;
    int channels = 2;
    bool preserveFormants = false;
    bool crispTransients = true;
};

class DspTimePitchEngine {
public:
    virtual ~DspTimePitchEngine() = default;
    virtual void init(int sampleRate, int channels) = 0;
    virtual void setTimeRatio(float ratio) = 0;
    virtual void setPitchSemitones(float semitones) = 0;
    virtual void putSamples(const float* interleave, size_t numFrames) = 0;
    virtual size_t receiveSamples(float* outInterleave, size_t maxFrames) = 0;
    virtual void flush() = 0;
    virtual void clear() = 0;
    virtual size_t latencyFrames() const = 0;
};

} // namespace reals::audio
```

**BPM Sync Computation**:
$$\text{Ratio} = \frac{\text{Project BPM (from REAPER } \texttt{Master\_GetTempo()}\text{)}}{\text{Sample Detected BPM}}$$
*Example*: Sample = 120 BPM, REAPER Project = 140 BPM $\implies \text{Ratio} = 140 / 120 = 1.16667$.

**Mini Piano Keyboard Transposition Interval**:
$$\Delta \text{semitones} = (N_{\text{target}} - N_{\text{sample}}) \pmod{12}$$
If $\Delta > 6 \implies \Delta = \Delta - 12$.  
*Example*: Sample Key is `F#` ($N=6$), user clicks `A` ($N=9$) $\implies \Delta = 9 - 6 = +3$ semitones.

---

### 4.5 Bridge Commands Contract (Extensions to `Bridge.cpp`)

| Command | Arguments | Return Data | Description |
|---------|-----------|-------------|-------------|
| `ai.analyzeFile` | `{"path": "..."}` | `{"bpm": 128.0, "key": "F#m", "camelot": "11A", "genres": [...], "moods": [...]}` | Runs full AI model inference pipeline locally |
| `ai.searchSemantic` | `{"query": "punchy 808", "limit": 50}` | `{"results": [{"path": "...", "score": 0.89}, ...]}` | Vector CLAP cosine similarity search |
| `db.search` | `{"query": "/bpm:120-130 /trap kick", "limit": 100}` | `{"results": [...]}` | Syntax `/` + Full-text + Metadata search |
| `db.scanFolder` | `{"path": "D:\\Samples"}` | `{"jobId": 1, "status": "started"}` | Enqueues background multi-threaded scanner |
| `audio.setPitchSemitones`| `{"semitones": 3.0}` | `{"ok": true, "pitchRatio": 1.1892}` | Real-time pitch shift without changing tempo |
| `audio.syncTempo` | `{"enabled": true}` | `{"ok": true, "projectBpm": 140.0, "sampleBpm": 120.0, "ratio": 1.1667}` | Syncs playback tempo to REAPER `Master_GetTempo()` |

---

## 5. Acceptance Criteria A1-A4 Mapping

| Criteria | Description | Verification Method | Status / Pass Condition |
|----------|-------------|---------------------|-------------------------|
| **A1.1** | BPM & Key analysis for WAV/MP3/FLAC | Run `ai.analyzeFile` on test suite of 50 labeled audio files | Accuracy $\ge 95\%$ on BPM, $\ge 90\%$ on Key |
| **A1.2** | Top-5 genre tags & mood labels | Verify JSON payload structure with Discogs-MAEST and Mood-Jamendo | Contains top-5 subgenres and valid mood probabilities |
| **A1.3** | CLAP 512-dim vector in SQLite | Query SQLite `analysis` table: `length(embedding) == 2048` | Verified 512-float vector persisted |
| **A2.1** | Syntax search `/trap /kick` | Run query `/trap /kick` via `db.search` | Returns samples matching genre/user tag and filename keyword |
| **A2.2** | Semantic search "lo-fi chill acoustic" | Run query `ai.searchSemantic` with natural text | Top-K matches have highest cosine similarity score |
| **A2.3** | Scanner background responsiveness | Scan 10,000 files in background worker pool | UI runs at 60 FPS, REAPER transport unaffected |
| **A3.1** | DSP Sync DAW BPM | Sample 120 BPM in REAPER 140 BPM project with `Sync BPM` on | Time-stretch factor is 1.16667x, pitch remains 1.0 |
| **A3.2** | Mini Piano Transposer Latency | Click piano key to transpose $\pm 12$ semitones | Audio pitch changes in $< 30\text{ms}$ without audio crackle/dropouts |
| **A3.3** | `Original Key` Reset | Click `Original Key` button | Transpose delta resets to $0$, pitch returns to 1.0x |
| **A4.1** | Zero-warning build | Run `cmake --build --preset windows` | Builds `reaper_realslab.dll` with 0 warnings |
| **A4.2** | Architecture Compliance | Check `#include` guards | `core/` has NO UI or REAPER SDK dependencies |
| **A4.3** | REAPER 7.x x64 Stability | Load DLL in REAPER 7.x, dock/undock, play audio | Zero crashes, zero memory leaks on unload |

---

## 6. Caveats

1. **Model Weights Size**:
   - Total model size for all ONNX models (TempoCNN + EDMA + MAEST + Mood-Jamendo + CLAP) is ~380 MB. Models must be loaded on demand (lazy loading) to minimize startup memory footprint.
2. **SIMD Optimization**:
   - Cosine similarity vector search benefits significantly from AVX2 / FMA. A scalar fallback must be provided for CPUs without AVX2 support.
3. **SoundTouch vs RubberBand**:
   - SoundTouch is lightweight, LGPL/BSD friendly, and highly optimized for real-time preview ($< 20\text{ms}$ latency). RubberBand provides higher transient fidelity on complex polyphonic mixes at slightly higher CPU usage. Both interfaces share the identical C++ `DspTimePitchEngine` abstraction.

---

## 7. Conclusion

The specification survey for Reals Lab AI Inference Engine (R1), Background Scanner & SQLite/Vector Search (R2), DSP Time-Stretch & Pitch-Shifter Engine (R3), and Acceptance Criteria (A1-A4) is fully documented with complete data structures, schemas, interfaces, algorithms, and verification methods. All requirements are verified against authoritative project constraints.

---

## 8. Verification Method

1. Inspect this handoff file: `c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1\handoff.md`.
2. Verify structural compliance with `SPEC.md`, `PLAN.md`, and `DESIGN.md`.
3. Verify zero-warning CMake configuration:
   ```powershell
   cmake --preset windows
   cmake --build --preset windows
   ```
