# Comprehensive Codebase Audit & Inspection Report

**Project**: Reals Lab REAPER Extension & Desktop App (`reals-lab-extension`)  
**Workspace Root**: `c:/Users/smk28/Desktop/reals lab extension`  
**Date**: 2026-08-29  
**Audit Methodology**: Multi-Agent Static & Empirical Codebase Inspection  
**Governing Documents**: `AGENTS.md`, `SPEC.md`, `PLAN.md`, `DESIGN.md`, `ORIGINAL_REQUEST.md`

---

## 1. Executive Summary & Defect Matrix

A comprehensive multi-agent audit was conducted across 100% of headers, implementation files, build configurations, and test suites across `core/`, `bridge/`, `extension/`, `shell/`, `ui-web/`, `assets/`, and `tests/`.

### Overall Findings Summary

| Severity | Count | Primary Impact Areas |
|---|:---:|---|
| **CRITICAL** | **6** | Real-time audio safety thread violation; Infinite loop hang in tempo detection; SQLite duplicate compilation & missing compile definitions; `CMakePresets.json` test preset execution failure; `app.js` i18n desynchronization causing missing UI keys; Unguarded Win32 headers in `HttpClient.cpp` breaking cross-platform builds |
| **MAJOR** | **11** | Take playrate sync business logic leak in `reaper_plugin.cpp`; Direct Win32 API calls in `core/` modules; Out-of-bounds FFT heap access on non-power-of-2 sizes; Missing `#pragma once` header guard in `SearchEngine.h`; Concurrency data races in `Database::close()` and `BrowserModel` reference getters; Owning raw pointers `m_impl` in `Engine` and `HttpClient`; Zero test coverage for `core/net` and `core/lab`; Monolithic source files exceeding 400–2600 lines; `applyI18n()` ignoring `data-i18n-title` |
| **MINOR** | **10** | Untested `platform::DirWatch` IOCP watcher; `TestSuite_AIInference` testing `ModelMocks` heuristics instead of core AI classes; Hardcoded tooltips in `index.html`; Hardcoded toast strings in `app.js`; 76 keys missing from C++ embedded fallback table; Missing `browser.noResults` key; Manual string path concatenation in `I18n.cpp`; Unbounded memory cache in `DragExporter.cpp`; Missing corrupt JSON tests in `I18n`; Test suite redundancy between standalone binaries and `reals_tests` |
| **STYLE / LINT** | **5** | Global static variables using `g_` prefix instead of `s_`; `REALS_WARNINGS_AS_ERRORS` defaults to `OFF`; Empty `app/` shell directory; Redundant `mockup.html` prototype; Redundant compiler warning flags on C files |
| **TOTAL** | **32** | **Full inventory categorized and actionable below** |

---

## 2. Requirement 1: Architecture & Layer Boundary Audit (R1)

### 2.1 `#include` & Dependency Isolation
- **`core/` Layer Isolation**: 100% compliant with zero inclusions of ImGui, GLFW, or REAPER SDK headers.
- **Defect (CRITICAL-01)**: `core/src/net/HttpClient.cpp:14–22` includes `<windows.h>` and `<winhttp.h>` unconditionally without `#ifdef _WIN32`, breaking macOS and Linux builds.
- **Defect (MAJOR-01)**: Direct Win32 API calls bypass `platform/` abstractions in:
  - `core/src/browser/BrowserModel.cpp:23, 42–49` (`MultiByteToWideChar`, `CharLowerBuffW`)
  - `core/src/config/Config.cpp:22–30` (manual `utf8Path()` instead of `platform::u8path()`)
  - `core/src/scanner/BackgroundScanner.cpp:25, 624, 626` (`SetThreadPriority`)
  - `core/src/util/Log.cpp:8, 39–42, 79` (`MultiByteToWideChar`, `_wfopen_s`, `OutputDebugStringA`)
- **`bridge/` Layer Isolation**: 100% compliant. Dispatches purely through abstract `IHostActions` interface.
- **`shell/` Layer Isolation**: Clean separation between Win32 host (`shell/win`) and DAW plugin.

### 2.2 Shell Logic & Thin Shell Constraints
- **Defect (MAJOR-02)**: `extension/src/reaper_plugin.cpp:114–265` embeds 165 lines of audio item stretching, tempo grid calculation, and take playrate management (`processPendingSyncPlayrates`). This violates `AGENTS.md` Rule 2 ("extension/ là shell mỏng — logic nghiệp vụ phải nằm ở core/"), forcing integration tests (`TestSuite_EmpiricalChallenger_R2.cpp:503`) to replicate rather than link the real logic.
- **Defect (STYLE-01)**: `app/` is currently an empty directory without a standalone WebView2 application entry point (`reals_app.exe`).

### 2.3 UI Localization & i18n Cross-Referencing
- **Asset Symmetry**: `assets/i18n/strings_en.json` (167 keys) and `assets/i18n/strings_vi.json` (167 keys) maintain 100% symmetrical parity.
- **Defect (CRITICAL-02)**: `ui-web/app.js:5–140` embeds a hardcoded JavaScript dictionary containing only 129 keys. 15 active keys called in `app.js` are missing, resulting in raw key strings rendered in the UI:
  - `browser.clearSimilar`, `browser.ctx.findSimilar`, `browser.ctx.rescanAll`, `browser.ctx.scanNew`, `browser.matchPercent`, `browser.similarTo`
  - `browser.noResults` (also missing from `assets/i18n/*.json`)
  - `scanner.addedCount`, `scanner.cancelled`, `scanner.cpuMode`, `scanner.cpuMode.high`, `scanner.cpuMode.highWarn`, `scanner.cpuMode.low`, `scanner.cpuMode.normal`, `scanner.starting`
- **Defect (MAJOR-03)**: `ui-web/app.js:449–452` `applyI18n()` only queries `[data-i18n]` and `[data-i18n-ph]`, completely ignoring `[data-i18n-title]`. As a result, tooltips (e.g. `index.html:138, 146`) never switch language.
- **Defect (MINOR-01)**: Hardcoded Vietnamese strings in `ui-web/index.html:29–33, 102, 117, 139–141, 151, 154, 160`.
- **Defect (MINOR-02)**: Hardcoded toast and log strings in `ui-web/app.js:511, 823, 1530, 1534, 2126`.
- **Defect (MINOR-03)**: C++ fallback table `core/src/i18n/I18n.cpp:26–123` (`kEmbedded`) contains only 93 keys (74 keys missing).

### 2.4 Monolithic File Sizing (>400 Lines)
The following files exceed the 400-line guideline and require modular decomposition:
- `ui-web/app.js`: **2607 lines** (Embedded dict, Bridge RPC, tree navigation, audio canvas, transposer, similarity banner, scanner bar, Audio Lab, settings).
- `bridge/src/Bridge.cpp`: **1536 lines** (Monolithic `Bridge::handle` dispatcher with fs, browser, audio, lab, scanner, reaper, window, config).
- `extension/src/reaper_plugin.cpp`: **1117 lines** (REAPER DLL hooks, playrate sync, Win32 windowing, subclassing, docking, message loop).
- `core/src/db/Database.cpp`: **874 lines** (Connection management, migrations, CRUD, FTS5 queries, SIMD vector queries).
- `core/src/scanner/BackgroundScanner.cpp`: **816 lines** (Crawler, worker thread pool, CPU throttling, AI extraction, SQLite batching).
- `core/src/audio/Engine.cpp`: **713 lines** (miniaudio device, lock-free playback, SoundTouch DSP, offline BPM/Key analysis, envelope generation).

---

## 3. Requirement 2: Code Quality, Memory, Concurrency & Real-Time Audio Safety Audit (R2)

### 3.1 Real-Time Audio Thread Safety
- **Defect (CRITICAL-03)**: In `core/src/audio/Engine.cpp:94,149,153` inside the miniaudio data source callback `dsp_on_read`:
  1. Line 94 acquires `std::lock_guard lock(ds->dspMutex)`. The UI thread also locks `dspMutex` during `setTimeRatio`, `setPitchSemitones`, and `setLoop`, causing priority inversion and audio dropouts.
  2. Line 149 executes `ds->readBuffer.resize(...)`, performing dynamic heap allocation on the audio thread.
  3. Line 153 invokes `ma_decoder_read_pcm_frames(&ds->decoder, ...)`, performing synchronous disk file decoding directly inside the audio callback.
  - *Rule Violated*: `AGENTS.md` Rule 3 ("Thread: mọi call audio API từ audio thread KHÔNG lock/allocate; giao tiếp qua lock-free queue hoặc atomic").

### 3.2 Arithmetic & Algorithmic Safety
- **Defect (CRITICAL-04)**: In `core/src/ai/TempoDetector.cpp:20–29`, `disambiguateBpm` lacks a finite float check. If `exactLag` is 0 or NaN, `calculatedBpm` evaluates to `+infinity`. For `+inf`, `bpm > 180.0f` remains permanently true (`inf / 2.0f == inf`), creating an unrecoverable infinite loop that hangs worker threads at 100% CPU.
- **Defect (MAJOR-04)**: In `core/src/ai/FeatureExtractor.cpp:70–103`, the Cooley-Tukey Radix-2 FFT assumes `n` is a power of 2 without validating `(n & (n - 1)) == 0`. Non-power-of-2 input lengths cause out-of-bounds indexing on `x[i + k + len / 2]`.

### 3.3 Concurrency & Thread Synchronization
- **Defect (MAJOR-05)**: In `core/src/db/Database.cpp:213–219`, `Database::close()` clears `m_path` and closes `m_db` without acquiring `m_mutex`, creating a data race with concurrent queries.
- **Defect (MAJOR-06)**: In `core/include/reals/browser/BrowserModel.h:40,54,56,63`, getter methods (`roots()`, `favorites()`, `recents()`, `tags()`) return direct const references to internal containers without locking `m_storeMutex`. Concurrent modifications under `m_storeMutex` cause data races and iterator invalidation on reader threads.

### 3.4 Memory Management & Smart Pointers
- **Defect (MAJOR-07)**: In `core/include/reals/audio/Engine.h:89` and `core/include/reals/net/HttpClient.h:51`, PIMPL instances use owning raw pointers `Impl* m_impl = nullptr;` with manual `new`/`delete` in constructors/destructors, violating `AGENTS.md` Rule 3 ("Smart pointers (unique_ptr/shared_ptr), không owning raw new").
- **Defect (MINOR-04)**: In `core/src/audio/DragExporter.cpp:130,230`, `s_memCache` accumulates entries without an LRU eviction policy, leading to unbounded memory growth over long sessions.

### 3.5 C++20 Compliance & Header Hygiene
- **Defect (MAJOR-08)**: `core/include/reals/search/SearchEngine.h:1` is completely missing `#pragma once` or an include guard.
- **Header Cleanliness**: Zero instances of `using namespace std;` found in any public header.

---

## 4. Requirement 3: Build & Test Diagnostics (R3)

### 4.1 Build System Diagnostics
- **Defect (CRITICAL-05)**: In root `CMakeLists.txt:59–87`, a static library `sqlite3` is declared with required macros (`SQLITE_THREADSAFE=1`, `SQLITE_ENABLE_FTS5=1`, `SQLITE_ENABLE_JSON1=1`), but `reals_core` compiles `libs/sqlite3/sqlite3.c` directly without linking `sqlite3` and without these definitions. Target `sqlite3` is orphaned and unlinked.
- **Defect (CRITICAL-06)**: In `CMakePresets.json:28–31`, `testPresets` defines `"windows"` referencing the multi-config Visual Studio generator without specifying `"configuration": "Debug"`. Running `ctest --preset windows` fails immediately with `Test not available without configuration`.
- **Defect (MAJOR-09)**: `core/src/lab/LabApi.cpp:3,87` wraps its entire implementation in `#ifdef _WIN32 ... #endif`. Compiling on macOS/Linux results in an empty object file and unresolved external symbols at link time.
- **Defect (STYLE-02)**: `CMakeLists.txt:20` defaults `REALS_WARNINGS_AS_ERRORS` to `OFF`.

### 4.2 Test Suite Execution & Coverage Diagnostics
- **Test Runner Execution**: Executed `ctest --preset windows -C Debug --output-on-failure`. All 5 test targets (11 suites, 191 test cases) passed in 48.42s.
- **Defect (MAJOR-10)**: `core/net` (`HttpClient`) and `core/lab` (`LabApi`) have **0% automated test coverage**. Network timeouts, 401/429/500 HTTP errors, and malformed multipart responses are unverified in CI.
- **Defect (MAJOR-11)**: `core/platform/DirWatch.cpp` (IOCP directory watcher) has **0% automated test coverage**.
- **Defect (MINOR-05)**: `tests/suites/TestSuite_AIInference.cpp:45–180` calls static methods on `ModelMocks` rather than testing production classes (`ai::TempoDetector`, `ai::KeyDetector`, `ai::GenreClassifier`).
- **Defect (MINOR-06)**: Standalone test binaries (`test_ai`, `test_audio_engine`, `test_soundtouch_processor`, `test_db_scanner`) duplicate tests in `reals_tests`, doubling compilation time.

---

## 5. Comprehensive Findings Catalog & Traceability Matrix

| ID | Severity | File & Line Reference | Rule / Contract Violated | Defect Description | Concrete Remediation Proposal |
|---|:---:|---|---|---|---|
| **CRIT-01** | Critical | `core/src/net/HttpClient.cpp:14–22` | `SPEC.md` §2, `AGENTS.md` R2 | Unguarded `<windows.h>` & `<winhttp.h>` break macOS/Linux compilation. | Wrap in `#ifdef _WIN32` or split into `HttpClient_win.cpp` and `HttpClient_curl.cpp`. |
| **CRIT-02** | Critical | `ui-web/app.js:5–140` | `AGENTS.md` R0, `SPEC.md` §5.5 | `app.js` embedded i18n dict is missing 15 active keys; raw keys displayed in UI. | Synchronize `app.js` with `assets/i18n/*.json` or load via Bridge RPC. |
| **CRIT-03** | Critical | `core/src/audio/Engine.cpp:94,149,153` | `AGENTS.md` R3 | Mutex lock, dynamic allocation (`resize`), and disk I/O in audio callback `dsp_on_read`. | Use atomic variables/lock-free FIFO for parameters; preallocate buffer; stream via ring buffer. |
| **CRIT-04** | Critical | `core/src/ai/TempoDetector.cpp:20–29` | Arithmetic Safety | Infinite loop hang on non-finite (`+inf`/`NaN`) BPM inputs in `disambiguateBpm`. | Add `if (bpm <= 0.0f \|\| !std::isfinite(bpm)) return 120.0f;`. |
| **CRIT-05** | Critical | `CMakeLists.txt:59–87` | `SPEC.md` §3 | `reals_core` compiles `sqlite3.c` directly without FTS5/Threadsafe definitions; `sqlite3` target orphaned. | Remove `sqlite3.c` from `reals_core` and link `sqlite3` library target. |
| **CRIT-06** | Critical | `CMakePresets.json:28–31` | `AGENTS.md` §5 | Test preset missing `"configuration": "Debug"`, breaking `ctest --preset windows`. | Add `"configuration": "Debug"` to `testPresets` in `CMakePresets.json`. |
| **MAJ-01** | Major | `core/src/browser/BrowserModel.cpp:23, 42–49`<br>`core/src/config/Config.cpp:22–30`<br>`core/src/scanner/BackgroundScanner.cpp:25, 624`<br>`core/src/util/Log.cpp:8, 39–42` | `AGENTS.md` R2, `SPEC.md` §3 | Direct Win32 API calls in `core/` bypass `platform/` abstractions. | Replace with `platform::u8path`, `platform::toLowerUtf8`, `platform::setThreadPriority`. |
| **MAJ-02** | Major | `extension/src/reaper_plugin.cpp:114–265` | `AGENTS.md` R2 | 165 lines of take playrate sync & stretch logic embedded in shell. | Extract into `extension/src/ReaperPlayrateSync.cpp` / `.h`. |
| **MAJ-03** | Major | `ui-web/app.js:449–452` | `AGENTS.md` R0 | `applyI18n()` ignores `[data-i18n-title]`, failing to update element tooltips on locale switch. | Add `$$('[data-i18n-title]').forEach(e => e.title = tr(e.dataset.i18nTitle));`. |
| **MAJ-04** | Major | `core/src/ai/FeatureExtractor.cpp:70–103` | Memory Safety | Out-of-bounds heap write in FFT if non-power-of-2 size is provided. | Validate `(n & (n - 1)) == 0` or pad to next power of 2 before FFT loop. |
| **MAJ-05** | Major | `core/src/db/Database.cpp:213–219` | Concurrency Safety | `Database::close()` mutates `m_db` and `m_path` without acquiring `m_mutex`. | Add `const std::lock_guard lock(m_mutex);` in `Database::close()`. |
| **MAJ-06** | Major | `core/include/reals/browser/BrowserModel.h:40,54,56,63` | Concurrency Safety | Unsynchronized reference getters return containers without holding `m_storeMutex`. | Return snapshot copies under `std::lock_guard lock(m_storeMutex)`. |
| **MAJ-07** | Major | `core/include/reals/audio/Engine.h:89`<br>`core/include/reals/net/HttpClient.h:51` | `AGENTS.md` R3 | PIMPL uses owning raw pointers `Impl* m_impl` with manual `new`/`delete`. | Convert `Impl* m_impl` to `std::unique_ptr<Impl> m_impl`. |
| **MAJ-08** | Major | `core/include/reals/search/SearchEngine.h:1` | Header Hygiene | Missing `#pragma once` header guard. | Add `#pragma once` as line 1 of `SearchEngine.h`. |
| **MAJ-09** | Major | `core/src/lab/LabApi.cpp:3,87` | `SPEC.md` §2 | Wraps implementation in `#ifdef _WIN32`, causing empty object file on macOS/Linux. | Implement portable REST dispatch delegating to `net::HttpClient`. |
| **MAJ-10** | Major | `tests/CMakeLists.txt`<br>`core/src/net/HttpClient.cpp`<br>`core/src/lab/LabApi.cpp` | `SPEC.md` §6 | 0% automated test coverage for `core/net` and `core/lab`. | Add `tests/suites/TestSuite_NetLab.cpp` with mock HTTP transport. |
| **MAJ-11** | Major | `core/src/platform/DirWatch.cpp:1–121` | `SPEC.md` §4 | 0% automated test coverage for IOCP directory watcher. | Add `tests/suites/TestSuite_PlatformDirWatch.cpp` verifying callback delivery. |
| **MIN-01** | Minor | `ui-web/index.html:29–33, 102, 117, 139–141, 151, 154, 160` | `AGENTS.md` R0 | Hardcoded Vietnamese strings in `title` attributes and `<option>` elements. | Replace with `data-i18n-title` and `data-i18n` attributes. |
| **MIN-02** | Minor | `ui-web/app.js:511, 823, 1530, 1534, 2126` | `AGENTS.md` R0 | Hardcoded toast prefixes and error messages. | Wrap all messages in `tr(...)` backed by `strings_*.json`. |
| **MIN-03** | Minor | `core/src/i18n/I18n.cpp:26–123` | Defensive Resilience | `kEmbedded` contains only 93 keys (74 missing compared to JSON files). | Synchronize `kEmbedded` table with all 167 keys. |
| **MIN-04** | Minor | `core/src/audio/DragExporter.cpp:130,230` | Memory Management | Unbounded memory cache growth in `s_memCache`. | Introduce LRU eviction policy with a 256-entry capacity limit. |
| **MIN-05** | Minor | `tests/suites/TestSuite_AIInference.cpp:45–180` | Test Validity | Tests call `ModelMocks` heuristics rather than production `core/ai` classes. | Refactor suite to call `ai::TempoDetector` and `ai::KeyDetector` directly. |
| **MIN-06** | Minor | `tests/CMakeLists.txt:1–40` | Build Maintenance | Standalone test executables duplicate test suites in `reals_tests`. | Unify under `reals_tests` with `--suite` filter flags. |
| **MIN-07** | Minor | `assets/i18n/strings_*.json` | Localization Completeness | Missing key `"browser.noResults"`. | Add `"browser.noResults": "No results found"` to both JSON files. |
| **MIN-08** | Minor | `core/src/i18n/I18n.cpp:161–162` | `AGENTS.md` R2 | Manual string path concatenation instead of `platform::joinPath`. | Replace with `platform::joinPath(dir, "strings_vi.json")`. |
| **MIN-09** | Minor | `core/src/i18n/I18n.cpp:132–144` | Defensive Error Handling | Missing unit tests for malformed / missing disk JSON fallback. | Add unit tests verifying `tr()` resilience against corrupt JSON. |
| **MIN-10** | Minor | `core/include/reals/search/SearchEngine.h` | Architecture | `SearchEngine.h` line count reaches 400 lines threshold. | Split vector similarity logic into separate header. |
| **STY-01** | Style | `extension/src/reaper_plugin.cpp:84–111` | `AGENTS.md` R3 | Static global variables use `g_` prefix instead of `s_`. | Standardize prefix to `s_`. |
| **STY-02** | Style | `CMakeLists.txt:20` | `AGENTS.md` R3 | `REALS_WARNINGS_AS_ERRORS` defaults to `OFF`. | Default to `ON` in test presets and CI pipelines. |
| **STY-03** | Style | `app/` | Architecture | Empty directory without standalone WebView2 entry point. | Implement `app/src/main.cpp` using `shell/win/WebViewHost`. |
| **STY-04** | Style | `mockup.html:1–1118` | Repo Hygiene | Stale 1118-line design prototype retained in root. | Move to `docs/prototypes/` or remove. |
| **STY-05** | Style | `CMakeLists.txt:27` | Build Hygiene | `-Wall -Wextra -Wpedantic` applied indiscriminately to C files (`sqlite3.c`). | Isolate C third-party compile options. |

---

## 6. File Sizing & Modular Decomposition Blueprint

Files exceeding ~400 lines violate `AGENTS.md` Rule 3 and must be decomposed as follows:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       MODULAR DECOMPOSITION BLUEPRINT                       │
└─────────────────────────────────────────────────────────────────────────────┘

1. ui-web/app.js (2607 lines) ────────────────────────────────────────────────
   ├── ui-web/js/i18n.js        (~150 lines) : Runtime loader & tr() translator
   ├── ui-web/js/bridge.js      (~200 lines) : WebMessage RPC & event dispatcher
   ├── ui-web/js/browser.js     (~400 lines) : Tree view, file list & search
   ├── ui-web/js/player.js      (~350 lines) : Waveform canvas & mini-piano transposer
   ├── ui-web/js/scanner.js     (~250 lines) : Progress bar & CPU mode selector
   ├── ui-web/js/lab.js         (~300 lines) : Stem/key/tempo job submission & polling
   └── ui-web/js/app.js         (~200 lines) : Main tab navigation & lifecycle

2. bridge/src/Bridge.cpp (1536 lines) ────────────────────────────────────────
   ├── bridge/src/handlers/FsHandlers.cpp       (~250 lines) : fs.* RPC methods
   ├── bridge/src/handlers/BrowserHandlers.cpp  (~300 lines) : browser.* RPC methods
   ├── bridge/src/handlers/AudioHandlers.cpp    (~250 lines) : audio.* RPC methods
   ├── bridge/src/handlers/ScannerHandlers.cpp  (~200 lines) : scanner.* RPC methods
   ├── bridge/src/handlers/LabHandlers.cpp      (~200 lines) : lab.* RPC methods
   └── bridge/src/Bridge.cpp                    (~300 lines) : Event loops & registration

3. extension/src/reaper_plugin.cpp (1117 lines) ──────────────────────────────
   ├── extension/src/ReaperWindow.cpp           (~300 lines) : HWND, DWM & Subclassing
   ├── extension/src/ReaperPlayrateSync.cpp     (~200 lines) : Take stretch & grid sync
   ├── extension/src/ReaperHostActions.cpp      (~250 lines) : IHostActions implementation
   └── extension/src/reaper_plugin.cpp          (~250 lines) : REAPER entry point & hooks

4. core/src/db/Database.cpp (874 lines) ──────────────────────────────────────
   ├── core/src/db/DbCore.cpp                   (~300 lines) : Connection & migrations
   ├── core/src/db/DbFts.cpp                    (~250 lines) : FTS5 full-text indexing
   └── core/src/db/DbVector.cpp                 (~300 lines) : SIMD vector cosine queries

5. core/src/scanner/BackgroundScanner.cpp (816 lines) ────────────────────────
   ├── core/src/scanner/DirectoryCrawler.cpp    (~300 lines) : Recursive file discovery
   └── core/src/scanner/BackgroundScanner.cpp   (~350 lines) : Worker pool & batch upserts

6. core/src/audio/Engine.cpp (713 lines) ─────────────────────────────────────
   ├── core/src/audio/AudioPlayback.cpp         (~350 lines) : Real-time ring buffer engine
   └── core/src/audio/AudioAnalysis.cpp         (~300 lines) : Offline BPM/Key/Envelope
```

---

## 7. Concrete Remediation Patches & Code Snippets

### Patch 1: Fix `CMakeLists.txt` SQLite Target & Definitions (CRIT-05)
```diff
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -59,7 +59,7 @@ endif()
 
 add_library(sqlite3 STATIC ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3/sqlite3.c)
 target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3)
-target_compile_definitions(sqlite3 PRIVATE
+target_compile_definitions(sqlite3 PUBLIC
     SQLITE_THREADSAFE=1
     SQLITE_ENABLE_FTS5=1
     SQLITE_ENABLE_JSON1=1
@@ -74,8 +74,7 @@ endif()
 
 file(GLOB_RECURSE REALS_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/core/src/*.cpp)
-set(SQLITE3_SOURCES ${CMAKE_CURRENT_LIST_DIR}/libs/sqlite3/sqlite3.c)
 
-add_library(reals_core STATIC ${REALS_CORE_SOURCES} ${SQLITE3_SOURCES})
-target_link_libraries(reals_core PUBLIC soundtouch nlohmann_json::nlohmann_json)
+add_library(reals_core STATIC ${REALS_CORE_SOURCES})
+target_link_libraries(reals_core PUBLIC sqlite3 soundtouch nlohmann_json::nlohmann_json)
```

### Patch 2: Fix `CMakePresets.json` Multi-Config Test Execution (CRIT-06)
```diff
--- a/CMakePresets.json
+++ b/CMakePresets.json
@@ -28,7 +28,11 @@
     {
       "name": "windows",
       "displayName": "Run Tests",
-      "configurePreset": "windows"
+      "configurePreset": "windows",
+      "configuration": "Debug",
+      "output": {
+        "outputOnFailure": true
+      }
     }
   ]
 }
```

### Patch 3: Fix Infinite Loop in `TempoDetector.cpp` (CRIT-04)
```diff
--- a/core/src/ai/TempoDetector.cpp
+++ b/core/src/ai/TempoDetector.cpp
@@ -20,7 +20,7 @@ constexpr int kFftSize = 1024;
 // Disambiguate tempo octaves preferring dance/standard music range (75 - 165 BPM)
 float disambiguateBpm(float bpm) {
-    if (bpm <= 0.0f) return 120.0f;
+    if (bpm <= 0.0f || !std::isfinite(bpm)) return 120.0f;
     while (bpm < 70.0f) {
         bpm *= 2.0f;
     }
```

### Patch 4: Fix Missing `#pragma once` in `SearchEngine.h` (MAJ-08)
```diff
--- a/core/include/reals/search/SearchEngine.h
+++ b/core/include/reals/search/SearchEngine.h
@@ -1,3 +1,5 @@
+#pragma once
+
 #include "reals/db/Database.h"
 #include "reals/search/QueryParser.h"
 #include "reals/search/SemanticSearch.h"
```

### Patch 5: Fix Data Race in `Database::close()` (MAJ-05)
```diff
--- a/core/src/db/Database.cpp
+++ b/core/src/db/Database.cpp
@@ -213,6 +213,7 @@ bool Database::open(const std::string& dbPath) {
 }
 
 void Database::close() {
+    const std::lock_guard lock(m_mutex);
     if (m_db) {
         sqlite3_close_v2(m_db);
         m_db = nullptr;
```

### Patch 6: Fix Unsynchronized Getters in `BrowserModel.h` (MAJ-06)
```diff
--- a/core/include/reals/browser/BrowserModel.h
+++ b/core/include/reals/browser/BrowserModel.h
@@ -40,11 +40,23 @@ public:
     // --- Roots ---------------------------------------------------------------
-    [[nodiscard]] const std::vector<Root>& roots() const { return m_roots; }
+    [[nodiscard]] std::vector<Root> roots() const {
+        const std::lock_guard lock(m_storeMutex);
+        return m_roots;
+    }
     // Returns false if `path` is already a root (no-op).
     bool addRoot(const std::string& name, const std::string& path);
     void removeRoot(size_t index);
 
     // --- Favorites / recents / tags -------------------------------------------
     [[nodiscard]] bool isFavorite(const std::string& path) const;
     void toggleFavorite(const std::string& path);
-    [[nodiscard]] const std::vector<std::string>& favorites() const { return m_favorites; }
+    [[nodiscard]] std::vector<std::string> favorites() const {
+        const std::lock_guard lock(m_storeMutex);
+        return m_favorites;
+    }
 
-    [[nodiscard]] const std::deque<std::string>& recents() const { return m_recents; }
+    [[nodiscard]] std::deque<std::string> recents() const {
+        const std::lock_guard lock(m_storeMutex);
+        return m_recents;
+    }
     void addRecent(const std::string& path);
     void clearRecents();
 
     // 0 = none, 1..7 = palette color
     [[nodiscard]] int tagOf(const std::string& path) const;
     void setTag(const std::string& path, int colorIndex);
-    [[nodiscard]] const std::unordered_map<std::string, int>& tags() const { return m_tags; }
+    [[nodiscard]] std::unordered_map<std::string, int> tags() const {
+        const std::lock_guard lock(m_storeMutex);
+        return m_tags;
+    }
```

### Patch 7: Fix Smart Pointer PIMPL Invariant (MAJ-07)
```diff
--- a/core/include/reals/audio/Engine.h
+++ b/core/include/reals/audio/Engine.h
@@ -88,3 +88,3 @@ private:
     struct Impl;
-    Impl* m_impl = nullptr; // PIMPL: keeps miniaudio out of public headers
+    std::unique_ptr<Impl> m_impl; // PIMPL: keeps miniaudio out of public headers
 };
--- a/core/include/reals/net/HttpClient.h
+++ b/core/include/reals/net/HttpClient.h
@@ -50,3 +50,3 @@ private:
     struct Impl;
-    Impl* m_impl = nullptr; // PIMPL: keeps WinHTTP out of public headers
+    std::unique_ptr<Impl> m_impl; // PIMPL: keeps WinHTTP out of public headers
 };
```

### Patch 8: Fix `applyI18n()` Tooltip Localization in `app.js` (MAJ-03)
```diff
--- a/ui-web/app.js
+++ b/ui-web/app.js
@@ -449,6 +449,7 @@ function applyI18n() {
     $$('[data-i18n]').forEach((el) => { el.textContent = tr(el.dataset.i18n); });
     $$('[data-i18n-ph]').forEach((el) => { el.placeholder = tr(el.dataset.i18nPh); });
+    $$('[data-i18n-title]').forEach((el) => { el.title = tr(el.dataset.i18nTitle); });
 }
```

---

## 8. Verification & Validation Protocol

To independently verify all findings and test remediations:

```powershell
# 1. Build Verification
cmake --preset windows
cmake --build --preset windows --config Debug
cmake --build --preset windows --config Release

# 2. Automated Test Suite Execution (with multi-config specifier)
ctest --preset windows -C Debug --output-on-failure

# 3. Layer Boundary & Include Audit
python -c "import os, glob; [print(f, l) for f in glob.glob('core/**', recursive=True) if f.endswith(('.h','.cpp')) for l in open(f, errors='ignore') if any(x in l.lower() for x in ['imgui', 'glfw', 'reaper_plugin'])]"

# 4. Localization Key Symmetry Verification
python -c "import json; en = set(json.load(open('assets/i18n/strings_en.json', encoding='utf-8')).keys()); vi = set(json.load(open('assets/i18n/strings_vi.json', encoding='utf-8')).keys()); print('Diff EN-VI:', en ^ vi)"

# 5. File Sizing (>400 lines) Verification
Get-ChildItem -Recurse -File | Where-Object { $_.FullName -notmatch '[\\/](build|\.git|\.agents|\.claude|\.gitnexus|libs)[\\/]' } | Select-Object @{N='Path';E={$_.FullName.Replace((Get-Location).Path+'\','')}}, @{N='Lines';E={(Get-Content -LiteralPath $_.FullName | Measure-Object -Line).Lines}} | Where-Object { $_.Lines -gt 400 } | Sort-Object Lines -Descending
```

---
*Report generated and validated by Project Orchestrator via multi-agent inspection.*
