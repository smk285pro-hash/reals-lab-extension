# Project: Reals Lab REAPER Extension — Global Favorites, Global Search, Clean Roots & Performance Optimization

## Architecture
- **Core (`src/core/` / `core/`)**:
  - `BrowserModel`: Library root management, directory scanning (`Path::listDir`), favorites storage (`browser_store.json`), and file metadata cache.
  - `SearchEngine` & `QueryParser`: Full-text, tag (`/tag`), BPM (`/bpm:range`), key (`/key:note`), Camelot, OpenKey, genre, and mood search with SIMD AVX2 cosine similarity on 512-dim CLAP embeddings.
  - `Database`: SQLite 3 WAL-mode database storing file metadata, audio attributes, and tags.
- **Bridge (`src/bridge/` / `bridge/`)**:
  - JSON-RPC over WebView2 `postMessage` handling commands (`browser.list`, `browser.search`, `browser.favorites`, `browser.getFavoriteEntries`, `browser.roots`, `audio.preview`, `reaper.insertMedia`).
- **UI Frontend (`ui-web/`)**:
  - Webview2 application (`index.html`, `app.js`, `app.css`, `tokens.css`).
  - Virtualized list rendering (`paintVisible()`, `getRowH()`) supporting 10,000+ items at 60 FPS with sub-16ms frame times.
  - Global Favorites view (`#favOnly`), Global Search bar (`#search`, `#searchClear`, `#searchSuggest`), debounced audio envelope probing (`probeVisibleAudio()`).
- **Test Infrastructure (`tests/`)**:
  - Zero-dependency `TestRunner.h` with mock harnesses (`MockHostActions`, `DbTestFixtures`, `AudioTestFixtures`, `ModelMocks`).

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | R3: Clean Initial Default Roots | Remove hardcoded `Music`/`Desktop`/`Downloads` roots so fresh installs start with 0 default roots and clean add folder prompts. | M1 | ORIGINAL_REQUEST §R3 |
| 2 | R1: Global Favorites View (`★`) | Display all favorited audio samples and MIDI files across entire library (all roots & subfolders) when `#favOnly` is active with live preview, transpose, drag, and untag. | M2 | ORIGINAL_REQUEST §R1 |
| 3 | R2: Global Search Across All Roots | Search across all configured root folders recursively with <50ms response, supporting text, `/tag`, `/bpm:range`, `/key:note` filters and instant view/scroll restoration on clear. | M3 | ORIGINAL_REQUEST §R2 |
| 4 | R4: 5,000+ Files Performance & Benchmarking | Zero-lag directory listing (<30ms), global search (<30ms), 60 FPS virtualized rendering (<16ms frame), thread safety, and zero memory leaks. | M4 | ORIGINAL_REQUEST §R4 |
| 5 | R5: Comprehensive E2E Testing (Tiers 1-4) & Adversarial Hardening (Tier 5) | Comprehensive requirement-driven opaque-box test suite + white-box adversarial stress tests + forensic audit. | M5 | ORIGINAL_REQUEST §Acceptance Criteria |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | Clean Initial Default Roots | `BrowserModel.cpp`, `app.js`, clean initial state verification | none | PLANNED |
| M2 | Global Favorites View (`★`) | `BrowserModel.h/cpp`, `Bridge.cpp`, `app.js`, favorites query & UI | M1 | PLANNED |
| M3 | Global Recursive Search & Filters | `Bridge.cpp`, `BrowserModel.cpp`, `app.js`, multi-root crawler & clear restore | M1, M2 | PLANNED |
| M4 | Performance & Zero-Lag Benchmarks | `TestSuite_PerformanceBenchmark.cpp`, 5,000+ files stress tests | M1, M2, M3 | PLANNED |
| M5 | E2E Verification & Audit | Tiers 1-5 test suites, `TEST_READY.md`, Forensic Audit | M1, M2, M3, M4 | PLANNED |

## Interface Contracts
### `browser.getFavoriteEntries` (Bridge RPC)
- **Request**: `{ "cmd": "browser.getFavoriteEntries", "id": "<rpc_id>", "args": {} }`
- **Response**: `{ "id": "<rpc_id>", "result": { "files": [ { "name": "...", "path": "...", "isDir": false, "size": 12345, "ext": ".wav", "isAudio": true, "isMidi": false, "duration": 4.2, "bpm": 128.0, "key": "C min" } ] } }`
- **Error handling**: Missing files are filtered out; returns empty array if no favorites exist.

### `browser.search` (Bridge RPC with Global Multi-Root support)
- **Request**: `{ "cmd": "browser.search", "id": "<rpc_id>", "args": { "base": "", "query": "kick /bpm:120-130 /key:C", "audioOnly": false, "maxResults": 500 } }`
- **Behavior**: If `base` is empty, executes global search across all configured roots in `BrowserModel::roots()`.

## Code Layout
- `core/include/reals/browser/BrowserModel.h` — Browser model interface
- `core/src/browser/BrowserModel.cpp` — Browser model implementation
- `bridge/include/reals/bridge/Bridge.h` — Bridge interface
- `bridge/src/Bridge.cpp` — JSON-RPC bridge dispatcher & search handler
- `ui-web/app.js` — Frontend state machine, favorites view lifecycle, search bar controller
- `ui-web/index.html` — Webview DOM structure
- `tests/` — Test suites and benchmarks
