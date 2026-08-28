# SCOPE — Milestone 5: Bridge RPC, Web UI & Mini Piano Transposer

## 1. Objectives & Scope
Implement the full Milestone 5 scope for Reals Lab as specified in PROJECT.md (Features 16-21) and ORIGINAL_REQUEST.md (R3, R4):
- **Feature 16 (Bridge RPC Extended Contracts)**:
  Extend `bridge/src/Bridge.cpp` with JSON-RPC commands:
  - `audio.setPitchShift`: `{ semitones: float }` -> `{ ok: true, pitchSemitones: float }`
  - `audio.setSyncBpm`: `{ enabled: bool, bpm?: float, sampleBpm?: float, ratio?: float }` -> `{ ok: true, syncBpm: bool, ratio: float }`
  - `audio.setOriginalKey`: `{}` -> `{ ok: true, pitchSemitones: 0.0 }`
  - `ai.analyzeFile`: `{ path: string }` -> `{ ok: true, analysis: { tempo, key, genres, moods, embedding } }`
  - `ai.searchSemantic`: `{ query: string, limit?: int }` -> `{ ok: true, results: [...] }`
  - `db.search`: `{ query: string, limit?: int, genre?: string, mood?: string, minBpm?: float, maxBpm?: float, key?: string }` -> `{ ok: true, results: [...] }`
  - `scanner.start`: `{ roots: [string] }` -> `{ ok: true, jobId: int }`
  - `scanner.status`: `{}` -> `{ ok: true, isScanning: bool, total: int, processed: int }`
  - Events:
    - `scanner.progress`: `{ total: int, processed: int, currentFile: string, isComplete: bool }`
    - `audio.syncState`: `{ syncBpm: bool, projectBpm: float, sampleBpm: float, ratio: float, semitones: float }`

- **Feature 17 (Player Tag & Mood Badges Row)**:
  - `#playerTagBar` (`.player-tag-bar`) container directly above `<canvas id="waveform">`.
  - Dynamic chip badges (Genre, Mood, Vocals, Choir, Male, Reverb, Ensemble, etc.).
  - Tag chips classified by type: `.tag-chip.tag-genre`, `.tag-chip.tag-mood`, `.tag-chip.tag-inst`, `.tag-chip.tag-prop`.

- **Feature 18 (Sync BPM Button Highlight)**:
  - `#btnSyncBpm` (Sync BPM button) on the player control row.
  - Active toggle state highlighted with Reals Orange accent (`--accent: #FF6B2C`).
  - Reads REAPER project tempo via bridge and calculates time-stretch ratio against sample BPM.

- **Feature 19 (Mini Piano Keyboard Transposer Popup)**:
  - Trigger button `#btnKeyTransposer` displaying `#playerKeyLabel` (e.g., "F# KEY" or "C KEY") and `#pitchShiftBadge` (e.g. "+2 st").
  - Popup modal `#pianoTransposerPop` (`.piano-popup`) featuring a 12-key chromatic piano keyboard (`C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`).
  - White keys (`.piano-key.white`) and black keys (`.piano-key.black`) accurately laid out.
  - Active key state (`.piano-key.active`) with semitone offset calculations.

- **Feature 20 (Original Key Reset Button)**:
  - `#btnResetKey` button styled with Reals orange/subtle style to immediately restore pitch to original key (0 semitones).
  - Resets active piano key visual state.

- **Feature 21 (Responsive UI, Styling & Localization)**:
  - Strictly compliant with DESIGN.md color tokens (`--bg-root`, `--bg-app`, `--bg-elevated`, `--border-card`, `--accent: #FF6B2C`, etc.).
  - Full Vietnamese & English localization in `assets/i18n/strings_vi.json` and `assets/i18n/strings_en.json` (zero hardcoded strings).
  - Clean C++20 build with zero compiler warnings.
