# Comprehensive Survey & Analysis: Build Configuration, Test Infrastructure & Performance Benchmarking

**Author**: Explorer 3 (Build, Tests & Performance Benchmark Specialist)  
**Date**: 2026-09-01  
**Target Repository**: `reals-lab-extension`  
**Status**: COMPLETE SURVEY & ARCHITECTURAL BLUEPRINT  

---

## Executive Summary

This report delivers an exhaustive technical survey of the build configuration, test framework architecture, existing test suites, and performance benchmarking requirements for the **Reals Lab REAPER Extension**. The analysis evaluates the repository against requirements **R1** (Global Favorites), **R2** (Global Multi-Root Search), **R3** (Clean Default Roots), and **R4** (5,000+ Files Performance Benchmarking & Stress Testing).

### Key Findings Matrix

| Domain | Status | Key Characteristics / Findings |
|---|---|---|
| **Build System** | **Compliant** | CMake 3.22+, C++20 standard required, MSVC `/W4 /permissive- /utf-8 /FS` flags with warnings-as-errors (`/WX` or `-Werror`). Dedicated static target for SQLite (`SQLITE_THREADSAFE=1`, `SQLITE_ENABLE_FTS5=1`). Single DLL output `reaper_realslab.dll` with post-build auto-deploy to `%APPDATA%\REAPER\UserPlugins`. |
| **Test Infrastructure** | **Extensible** | Custom header-only GoogleTest-like runner (`tests/framework/TestRunner.h`) with zero external test dependencies, sub-millisecond execution timing, ANSI color logs, and CLI filters (`--suite`, `--filter`, `--list`). Full mock interfaces for `IHostActions` (`MockHostActions.h`), SQLite DB fixtures (`DbTestFixtures.h`), WAV audio synthesis & corruptors (`AudioTestFixtures.h`), and ML/DSP models (`ModelMocks.h`). |
| **Existing Test Coverage** | **18 Suites / 249+ Tests** | Comprehensive coverage across Bridge RPC dispatch, SearchEngine syntax/vector similarity, SoundTouch DSP time-stretching/pitch-shifting, phase sync diagnostics, drag mechanisms (A/B), database indexing, adversarial stress, and Theme Engine. |
| **R1 (Global Favorites) Gap** | **Identified** | Backend currently stores `m_favorites` as a list of file paths in `BrowserModel`, but lacks a dedicated unified global listing API that aggregates and returns full `FileEntry` metadata for favorites across all roots and subdirectories simultaneously. Missing automated integration tests for preview, key transpose, drag-and-drop, and live untag/unfavorite in global view. |
| **R2 (Global Search) Gap** | **Identified** | `Bridge.cpp` (`runSearch`) queries SQLite DB or falls back to `BrowserModel::search(base)`. When `base` is empty (Global Search across multiple roots), the directory crawl fallback does NOT iterate across all `model.roots()`. Missing multi-root integration tests verifying unified results across disparate drives/roots. |
| **R3 (Clean Default Roots) Gap** | **Identified** | `BrowserModel.cpp` (lines 153-168) explicitly hardcodes `Music`, `Desktop`, and `Downloads` on initialization. R3 requires starting with 0 default roots on a fresh install. Missing unit tests verifying clean state initialization and zero hardcoded OS folders. |
| **R4 (Performance Benchmarking) Gap** | **Identified** | Existing benchmark in `AdversarialHardening` tests 2,200 files. Missing dedicated benchmark suite explicitly testing 5,000+ files to verify the strict <30ms listing/search latency target, 60 FPS virtual scroll stability, 16-thread concurrency stress, and zero memory leaks. |

---

## 1. Build System & Toolchain Analysis

### 1.1 CMake Configuration (`CMakeLists.txt`)

The root `CMakeLists.txt` enforces modern CMake practices (minimum version 3.22) and strict language compliance:

```cmake
# Standard & Compliance (CMakeLists.txt:13-17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

#### Compiler Warnings & Conformance Flags

Target compiler configuration conforms strictly to `AGENTS.md` rules:

```cmake
# CMakeLists.txt:19-35
if (MSVC)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:/W4;/permissive-;/utf-8;/FS>"
    )
    if (REALS_WARNINGS_AS_ERRORS)
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/WX>")
    endif()
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
else()
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>"
    )
    if (REALS_WARNINGS_AS_ERRORS)
        add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Werror>")
    endif()
endif()
```

#### Third-Party Target Isolation

Third-party dependencies are compiled as isolated static library targets with dedicated, tailored compiler flags:
1. **SQLite 3 (`sqlite3`)** (`CMakeLists.txt:46-60`):
   - Compiled with `/W3` (suppressing `/W4` third-party warnings).
   - Preprocessor definitions: `SQLITE_THREADSAFE=1`, `SQLITE_ENABLE_FTS5=1`, `SQLITE_ENABLE_JSON1=1`, `SQLITE_ENABLE_RTREE=1`, `SQLITE_DEFAULT_MEMSTATUS=0`.
2. **SoundTouch (`soundtouch`)** (`CMakeLists.txt:63-78`):
   - Compiled with `/W3` and `SOUNDTOUCH_FLOAT_SAMPLES=1`.
3. **JSON (`nlohmann_json`)**:
   - Header-only target with `nlohmann_json::nlohmann_json`.
4. **miniaudio**:
   - Single-header audio library configured directly in `reals_core`.

#### Core & Extension Targets

- **`reals_core`** (`CMakeLists.txt:80-128`): Static library containing `browser`, `audio`, `ai`, `db`, `scanner`, `search`, `i18n`, `config`, `net`, `platform`, and `util`. Links `sqlite3`, `soundtouch`, `nlohmann_json`, and Windows `winhttp.lib`.
- **`reals_bridge`** (`CMakeLists.txt:130-149`): Static library implementing JSON RPC dispatcher (`Bridge.h/cpp`). Links `reals_core`.
- **`reaper_realslab`** (`extension/CMakeLists.txt`): Shared library (DLL) implementing the Win32 host window and WebView2 integration. Post-build copies the DLL directly to `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll` for instant host testing.
- **`reals_tests`** (`tests/CMakeLists.txt`): Test executable linked against `reals_bridge` and `reals_core`.

### 1.2 Presets & Execution Commands (`CMakePresets.json`)

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "windows",
      "displayName": "Windows (MSVC, VS 2022)",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build/windows",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "REALS_BUILD_APP": "OFF",
        "REALS_BUILD_EXTENSION": "ON",
        "REALS_BUILD_TESTS": "ON",
        "REALS_WARNINGS_AS_ERRORS": "ON"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows",
      "configurePreset": "windows",
      "configuration": "Debug"
    }
  ],
  "testPresets": [
    {
      "name": "windows",
      "configurePreset": "windows",
      "configuration": "Debug",
      "outputOnFailure": true
    }
  ]
}
```

#### Command Line Workflows

```powershell
# 1. Configure
cmake --preset windows

# 2. Build Core, Extension DLL, and Test Executable
cmake --build --preset windows

# 3. Run All Automated Test Suites via CTest
ctest --preset windows

# 4. Direct Test Execution with Suite Filtering
.\build\windows\tests\Debug\reals_tests.exe --suite=SearchEngine
.\build\windows\tests\Debug\reals_tests.exe --filter=*Benchmark*
.\build\windows\tests\Debug\reals_tests.exe --list
```

---

## 2. Test Infrastructure & Mock Framework Architecture

The test suite in `tests/` is completely standalone, requiring zero external test libraries (no external GoogleTest or Catch2 binary dependencies). It is built upon four architectural components:

```
tests/
├── framework/
│   ├── TestRunner.h         # Zero-dependency test runner & assertion macros
│   ├── MockHostActions.h    # Mock IHostActions & BridgeTestHarness for JSON RPC
│   ├── AudioTestFixtures.h  # DSP wave synthesis, WAV encoder, corrupted headers, pitch estimator
│   ├── DbTestFixtures.h     # In-memory SQLite fixture, 512-dim embedding generator, MockDbStore
│   └── ModelMocks.h         # Mocks for TempoCNN, Key EDMA, Genre, Mood, and CLAP
├── suites/                  # 18 Test Suites (249+ Test Cases)
└── main.cpp                 # Entrypoint calling reals::test::TestRunner::run(argc, argv)
```

### 2.1 TestRunner Harness (`tests/framework/TestRunner.h`)

`TestRunner.h` provides:
1. **Registration Macros**: `TEST(SuiteName, TestName)`, `TEST_F(FixtureClass, TestName)`.
2. **Assertion Suite**: `EXPECT_TRUE`, `EXPECT_FALSE`, `EXPECT_EQ`, `EXPECT_NE`, `EXPECT_NEAR`, `EXPECT_LT`, `EXPECT_LE`, `EXPECT_GT`, `EXPECT_GE`, `EXPECT_THROW`, `EXPECT_NO_THROW`, and non-fatal `ASSERT_*` equivalents.
3. **Execution Diagnostics**:
   - Sub-millisecond execution duration per test (`std::chrono::high_resolution_clock`).
   - Terminal color formatting (Green `[ PASS ]`, Red `[ FAIL ]`, Cyan `── Suite: ... ──`).
   - Command-line arguments: `--suite=<Name>`, `--filter=<Pattern>`, `--list`.
   - Global test summary reporting total executed, passed, failed, and total wall-clock time.

### 2.2 Bridge RPC Mock Harness (`tests/framework/MockHostActions.h`)

Provides `MockHostActions` implementing `reals::bridge::IHostActions`:
- Tracks `m_insertedMedia`, `m_queuedSyncPaths`, `m_revealedPaths`, `m_draggedPaths`, `m_hostTransport`, and ExtState key-values.
- `BridgeTestHarness`: Encapsulates `reals::bridge::Bridge` and `MockHostActions` with a synchronous JSON RPC method:
  ```cpp
  json res = harness.call("audio.setPitchShift", {{"semitones", 3.0f}});
  EXPECT_TRUE(res.value("ok", false));
  ```
- Supports event queue polling (`drainEvents()`, `lastEvent()`) to verify asynchronous notifications emitted by C++ worker threads to WebView2.

### 2.3 Audio Synthesis & Corruption Fixtures (`tests/framework/AudioTestFixtures.h`)

Provides precision DSP synthesis and edge-case fuzzing utilities:
- **Wave Generators**: `generateSine`, `generateStereoSine`, `generateKickRhythm`, `generateChordTriad`, `generateSilent`, `generateNoise`, `generateDcOffset`.
- **WAV Encoders**: In-memory RIFF WAV encoder (`encodeWavMemory`) for 16-bit PCM and 32-bit Float formats; disk writer (`writeWavFile`).
- **Resilience Corruptors**: Generates corrupted WAV files for boundary testing (`WavCorruptionType::ZeroByte`, `TruncatedRiffHeader`, `CorruptedFmtChunkSize`, `InvalidChannelCount`, `TruncatedDataChunk`, `TrailingGarbageBytes`).
- **Mathematical Pitch Estimator**: Time-domain autocorrelation fundamental frequency estimator (`estimateFundamentalFrequency`) with parabolic interpolation, used as an objective mathematical oracle to verify pitch-preservation and transposition accuracy.

### 2.4 Database & Embedding Fixtures (`tests/framework/DbTestFixtures.h`)

- Manages temporary directory SQLite databases (`open(":memory:")` or disk-backed temporary directories).
- `generateUnitEmbedding(int seed)`: Generates deterministic, L2-normalized 512-dimensional float vectors.
- `generateSampleDataset(size_t count)`: Synthesizes large mock datasets (1,000 to 10,000 records) with realistic tempo (60-180 BPM), Camelot keys (1A-12B), genres, moods, and binary embedding BLOBs.

### 2.5 AI & DSP Model Mocks (`tests/framework/ModelMocks.h`)

- **TempoDetector**: Detects BPM and transients using simulated onset autocorrelation.
- **KeyDetector**: Computes chromagrams and projects against Krumhansl-Schmuckler and Temperley key profiles with Camelot Wheel and OpenKey notation mappings.
- **GenreClassifier**: Multi-class softmax prediction over Discogs-MAEST taxonomy.
- **MoodClassifier**: Multi-label sigmoid activation over Mood-Jamendo 56 tags.
- **ClapEmbedder**: Generates 512-dim normalized embedding vectors and performs SIMD cosine similarity ranking.

---

## 3. Inventory of Existing Test Suites (18 Suites)

The repository currently includes 18 specialized test suites:

```
1.  TestSuite_AIInference.cpp           (ONNX bindings, FeatureExtractor FFT/chroma, TempoCNN, EDMA Key, CLAP)
2.  TestSuite_AIProduction.cpp          (Production AI classes: TempoDetector, KeyDetector, GenreClassifier, MoodClassifier)
3.  TestSuite_AdversarialHardening.cpp   (1,000 RPC stress, rapid piano clicks, background DB inserts, syntax fuzzing, 2,200 files benchmark)
4.  TestSuite_AudioDSP.cpp               (SoundTouch DSP, ratio math, chromatic intervals, phase sync math, DragExporter temp WAV rendering)
5.  TestSuite_AudioEngineCore.cpp        (Engine getters/setters, playback pipeline, live parameter atomics, probe, envelope)
6.  TestSuite_BoundariesCorners.cpp      (0-byte files, truncated RIFF headers, digital silence, DC offset, 10k records search latency)
7.  TestSuite_BridgeUI.cpp               (Bridge RPC commands: app.info, config.*, audio.*, reaper.insert, browser.favorites/tags/drag)
8.  TestSuite_CrossFeatures.cpp          (Scanner + AI + DB + syntax search, CLAP SIMD matching, DAW tempo sync stretch, Mechanism A drag)
9.  TestSuite_DatabaseScanner.cpp        (SQLite DB CRUD, FTS5 syntax search, BackgroundScanner thread pool, pause/resume/cancel)
10. TestSuite_DbScannerCore.cpp         (xxHash64 & SHA-256 hash tests, SQLite schema, embedding BLOBs, user tags, query filters)
11. TestSuite_EmpiricalChallenger_R1.cpp (Phase sync mathematical oracles, 12-bar / 6-bar / 3-bar loops, post-decode transport query)
12. TestSuite_EmpiricalChallenger_R2.cpp (Drag rendering, sub-50µs cache hit, Mechanism A take stretch, Mechanism B double-DSP safeguard)
13. TestSuite_EndToEndWorkflows.cpp      (Sample ingestion, rapid auditioning, 5,000 records indexing under active playback, error recovery)
14. TestSuite_PhaseSyncDiagnostics.cpp  (12-bar blues, 6-bar loop, 3-bar loop, 2.5-bar integer beat fallback, multi-rate 44.1k on 48k/96k)
15. TestSuite_PlatformResilience.cpp    (i18n embedded fallbacks, corrupt JSON recovery, FFT non-power-of-two safety guard, DirWatch/HttpClient)
16. TestSuite_SearchEngine.cpp          (QueryParser tokens /tag /bpm: /key: /fav, SIMD dot product & cosine similarity, Top-K selection, hybrid search)
17. TestSuite_SoundTouchCore.cpp        (SoundTouch latency, time-stretch compression/expansion, octave shifts, streaming chunk I/O)
18. TestSuite_ThemeEngine.cpp           (ExtState theme persistence, Dark Studio / Pastel Pink / Cyberpunk palettes, rapid switching, thread safety)
```

---

## 4. Performance & Benchmarking Strategy for 5,000+ Files

### 4.1 Performance Criteria & Requirements

| Metric | Target SLA | Verification Method |
|---|---|---|
| **Directory Walk & Listing (`fs.list`)** | **< 30 ms** for 5,000 files | Native C++ benchmark measuring `model.listDir(dir)` from clean cache on cold/warm NTFS cache. |
| **Directory Sorting (Name, Size, Date)** | **< 5 ms** for 5,000 entries | Measure `std::sort` execution with cached lowercase keys (`lowerName`). |
| **Global Search Across All Roots (`browser.search`)** | **< 30 ms** for 5,000 files | Measure combined SQLite FTS5 / recursive directory crawl latency across multiple root directories. |
| **Cache Hit Retrieval** | **< 0.5 ms** (< 500 µs) | Measure `model.listDir(dir)` when directory entries are cached in memory. |
| **UI Scroll Framerate** | **60 FPS** (< 16.6 ms/frame) | Virtual DOM scrolling in `ui-web/app.js` rendering only 20-30 visible rows out of 5,000. |
| **Waveform / Envelope Probing** | **Zero UI Hitching** | Audio metadata and envelope generation (`audio.probe`) debounced and offloaded to background thread pool. |
| **Concurrency & Thread Safety** | **0 Data Races** | 16 concurrent threads executing `listDir`, `search`, `addUserTag`, and `audio.probe` simultaneously under TSAN / MSVC lock verification. |
| **Memory Leaks** | **0 Leaks** | Continuous 10,000 iteration stress test measuring CRT Debug Heap / OS working set memory growth. |

### 4.2 Microbenchmark Architecture Design (`TestSuite_PerformanceBenchmark.cpp`)

To automate testing against 5,000+ files, we design a dedicated benchmark test suite containing 5 focused benchmark routines:

```cpp
// Architectural Outline: TestSuite_PerformanceBenchmark.cpp

// 1. Directory Listing Benchmark (5,000 Files)
TEST(PerformanceBenchmark, Benchmark_5000_Files_DirectoryListing_Under30ms) {
    // Generate temporary directory tree containing 5,000 media files across 25 subdirectories
    // Invalidate model cache -> Benchmark cold listDir() -> Verify durationMs < 30.0 ms
    // Benchmark warm cache listDir() -> Verify durationUs < 500.0 us
}

// 2. Multi-Root Global Search Benchmark (5,000 Files across 3 Roots)
TEST(PerformanceBenchmark, Benchmark_5000_Files_MultiRootSearch_Under30ms) {
    // Distribute 5,000 files across Root A (Drums, 2,000), Root B (Synths, 1,500), Root C (Vocals, 1,500)
    // Execute global search for "/bpm:120-130" and text queries -> Verify total search time < 30.0 ms
}

// 3. Multi-Threaded Concurrent Read/Write Stress (16 Worker Threads)
TEST(PerformanceBenchmark, Stress_16_ConcurrentThreads_ListingAndSearching) {
    // 8 threads reading listDir(), 4 threads running global search, 4 threads modifying tags/favorites
    // Run 5,000 total operations -> Verify 0 exceptions, 0 deadlocks, 100% completion
}

// 4. Memory Stability & Leak Detection (10,000 Operations)
TEST(PerformanceBenchmark, Stability_MemoryLeakDetection_10000Operations) {
    // Snapshot initial process memory -> Execute 10,000 cycles of parse, list, search, JSON RPC
    // Snapshot final process memory -> Assert memory growth < 2 MB (within normal heap fragmentation)
}

// 5. Virtual List UI Payload Serialization Latency
TEST(PerformanceBenchmark, Benchmark_5000_Entries_JsonSerialization_Under10ms) {
    // Serialize 5,000 FileEntry objects to JSON string payload for WebView2 IPC bridge
    // Verify JSON serialization completes in < 10 ms without heap bloat
}
```

### 4.3 Debug vs Release Profiling & Benchmark Thresholds

During empirical execution of the test runner (`reals_e2e_tests`), 304 of 306 assertions passed. Two benchmark tests failed timing thresholds because of unoptimized MSVC Debug flags (`/Od` with debug iterator verification):
1. **`BoundariesCorners.Corner_DB_HugeLibrary10kRecords`**: 10,000 512-dim vector comparisons executed via unoptimized scalar loop took 2,182ms in Debug mode (failing the `< 50ms` release threshold).
2. **`AdversarialHardening.Benchmark_Browser_Recursive2000FilesWalkAndSortUnder30ms`**: Synchronously writing and scanning 2,200 files on NTFS in Debug mode took 7,124ms total test time.

#### Strategic Recommendation for Benchmarking
- **SIMD Utilization**: Benchmark suites must invoke `reals::util::Simd::cosineSimilarity` (AVX2/FMA) rather than scalar fallbacks for high-dimensional vector math.
- **Threshold Calibration**: Test assertions in standard Debug mode (`ctest --preset windows`) should test logical correctness and loose timing bounds (< 3000ms), while strict micro-benchmarking (< 30ms for 5,000 files) should be gated under Release builds or executed via a dedicated benchmark target (`--suite=PerformanceBenchmark`).

---

## 5. Requirement Gap Analysis (R1, R2, R3, R4)

### 5.1 R1: Global Favorites (`★` / `#favOnly`)

#### Current Behavior
- `BrowserModel.h/cpp` maintains `std::vector<std::string> m_favorites` persisted in `browser_store.json`.
- `Bridge.cpp` handles `browser.favorites` by returning the raw vector of favorite file paths (`m_favorites`).
- `fs.list` only lists the specific requested directory (`path`). When `#favOnly` is toggled in the frontend, the UI filters only the items currently displayed in that folder.

#### Identified Gap
- **Backend**: There is no dedicated backend command (e.g. `browser.listFavorites`) that aggregates and returns full `FileEntry` metadata (`name`, `path`, `isAudio`, `isDir`, `size`, `modified`, `tag`, `bpm`, `key`) for all favorited files across all roots and subdirectories.
- **Frontend Interaction**: The UI requires a unified view mode where activating `★` immediately queries and displays all global favorites across the entire library, regardless of which folder is currently selected in the tree.
- **Missing Tests**:
  1. Unit test verifying unified global favorites listing across multiple roots.
  2. Integration test verifying preview, pitch transposition, and drag-and-drop on an item in the Global Favorites view.
  3. Integration test verifying live list updates when an item in the Global Favorites view is un-favorited (instant row removal).
  4. Integration test verifying MIDI files in Favorites list have valid piano roll preview and playback.

---

### 5.2 R2: Global Search Across All Root Folders

#### Current Behavior
- In `Bridge.cpp` (`runSearch`), the search logic executes as follows:
  ```cpp
  // Bridge.cpp:520-560
  if (m_impl->searchEngine) {
      // 1. Search SQLite DB (FTS + Vector)
      results = m_impl->searchEngine->search(query, maxResults);
  }
  if (results.size() < maxResults && !base.empty()) {
      // 2. Fallback to BrowserModel::search on `base` directory
      auto fileResults = m_impl->browserModel.search(base, query, ...);
  }
  ```

#### Identified Gap
- When `base` is empty (representing a Global Search across the whole library), if the database is not yet fully indexed or if non-indexed sample folders are present, the directory crawler fallback does NOT iterate over all `model.roots()` — it simply skips the directory crawl because `base.empty() == true`.
- **Missing Tests**:
  1. Multi-root search integration test: Verify that searching for `"Trap"` across Root 1 (`D:\Drums`) and Root 2 (`E:\Synths`) returns matching files from both roots in a single query result.
  2. Syntax token parsing test on global multi-root search: Verify `/bpm:120-130`, `/key:Am`, `/fav` filter across all roots simultaneously.
  3. Search generation token discard test: Rapidly typing queries (`"k"`, `"ki"`, `"kic"`, `"kick"`) discards outdated search generation results (`gen`) without UI race conditions.
  4. Search reset test: Clearing the search input immediately restores the previous directory listing, active folder selection, and scroll offset.

---

### 5.3 R3: Clean Initial Default Roots

#### Current Behavior
In `core/src/browser/BrowserModel.cpp` (lines 153-168), the constructor explicitly adds default OS directories:
```cpp
// BrowserModel.cpp:153-168
BrowserModel::BrowserModel() {
    m_roots.push_back({"Music", platform::defaultMusicDir()});
    m_roots.push_back({"Desktop", platform::joinPath(platform::homeDir(), "Desktop")});
    m_roots.push_back({"Downloads", platform::joinPath(platform::homeDir(), "Downloads")});
    loadStore();
}
```

#### Identified Gap
- **Requirement R3 explicitly mandates**:
  > "Remove hardcoded default directories (Music, Desktop, Downloads) from default initialization. Fresh extension instances start with clean state prompting user to add sample library folders via `+📁` button or drag-and-drop."
- **Missing Tests**:
  1. Unit test verifying that a fresh `BrowserModel` instance with an empty store initializes with `roots().empty() == true` (0 roots).
  2. Integration test verifying that adding the first user directory via `fs.addRoot` or `fs.dropPaths` transitions state from 0 roots to 1 root without injecting any OS default folders.
  3. Integration test verifying that removing the last root returns the extension to the clean empty state without re-populating default OS folders.

---

### 5.4 R4: Performance & Stress Testing for 5,000+ Files

#### Current Behavior
- Existing test `AdversarialHardening.Benchmark_Browser_Recursive2000FilesWalkAndSortUnder30ms` tests 2,200 files.
- `BoundariesCorners.Corner_DB_HugeLibrary10kRecords` tests 10,000 simulated in-memory records.

#### Identified Gap
- No automated native filesystem benchmark exists that generates 5,000+ files across a realistic folder hierarchy to explicitly measure and enforce:
  1. `model.listDir()` cold walk & sort latency < 30ms.
  2. Multi-root `browser.search()` latency < 30ms / < 50ms.
  3. High-concurrency 16-thread stress without memory leaks or deadlocks.
  4. Virtual scroll payload delivery < 10ms.

---

## 6. Actionable Implementation & Test Expansion Plan

### Phase A: Architecture & Code Fixes (For Implementer)

1. **R3 Fix (`BrowserModel.cpp`)**:
   - In `BrowserModel::BrowserModel()`, remove hardcoded `Music`, `Desktop`, and `Downloads` entries. Initialize `m_roots` as an empty vector. Only load roots from `browser_store.json` if present.
2. **R1 Fix (`BrowserModel.h/cpp` & `Bridge.cpp`)**:
   - Add `BrowserModel::listFavorites()` method that traverses all paths in `m_favorites`, verifies file existence, constructs full `FileEntry` objects, and returns a sorted `std::vector<FileEntry>`.
   - In `Bridge.cpp`, add handler for `browser.listFavorites` (or enhance `browser.favorites`) to return full `FileEntry` array for immediate virtual list display.
3. **R2 Fix (`Bridge.cpp` & `BrowserModel.cpp`)**:
   - Update `Bridge.cpp` (`runSearch`): If `base` is empty, iterate over all `m_impl->browserModel.roots()` to perform the fallback search across every configured root folder.
4. **R4 Virtual List Optimization (`ui-web/app.js`)**:
   - Ensure virtual scrolling container computes dynamic viewport offsets and renders only the visible slice (~20 DOM rows), maintaining 60 FPS scrolling for 5,000+ items.

### Phase B: New Concrete Test Suites to Add

We recommend adding two new dedicated test suite files to `tests/suites/`:

#### 1. `tests/suites/TestSuite_Requirements_R1_R2_R3.cpp`
- `TEST(Requirements_R3, FreshInstall_ZeroDefaultRoots)`: Asserts fresh `BrowserModel` starts with 0 roots.
- `TEST(Requirements_R3, AddFirstRoot_CleanTransition)`: Asserts adding first root creates exactly 1 root without default folders.
- `TEST(Requirements_R3, RemoveLastRoot_RemainsEmpty)`: Asserts removing all roots leaves 0 roots without re-populating.
- `TEST(Requirements_R1, GlobalFavorites_AggregatesAcrossAllRoots)`: Asserts `listFavorites()` returns full `FileEntry` objects for favorited files across disparate folders.
- `TEST(Requirements_R1, GlobalFavorites_PreviewAndTranspose)`: Asserts audio probe, pitch shift, and playhead sync work on global favorites.
- `TEST(Requirements_R1, GlobalFavorites_UntagRemovesFromList)`: Asserts toggling favorite off removes item from global favorites list.
- `TEST(Requirements_R2, GlobalSearch_MultiRootRecursion)`: Asserts search across multiple roots returns hits from all roots.
- `TEST(Requirements_R2, GlobalSearch_SyntaxFiltersCombined)`: Asserts `/bpm:`, `/key:`, `/tag` filters match across all roots.
- `TEST(Requirements_R2, GlobalSearch_ClearRestoresView)`: Asserts clearing search restores previous folder and view state.

#### 2. `tests/suites/TestSuite_PerformanceBenchmark.cpp`
- `TEST(PerformanceBenchmark, Listing_5000_Files_Under30ms)`: Generates 5,000 files in 25 folders; asserts `listDir()` cold < 30ms, warm cache < 500µs.
- `TEST(PerformanceBenchmark, GlobalSearch_5000_Files_Under30ms)`: Asserts search across 5,000 files completes in < 30ms.
- `TEST(PerformanceBenchmark, Concurrency_16Threads_Stress)`: Asserts 16 concurrent reader/writer threads complete 5,000 operations with 0 data races.
- `TEST(PerformanceBenchmark, MemoryStability_10000_Operations_ZeroLeaks)`: Asserts memory delta across 10,000 operations is zero (< 2MB heap delta).

---

## 7. Conclusion

The build system and test harness of `reals-lab-extension` provide a robust, modern C++20 foundation with zero external test dependencies. The current 18 test suites provide rigorous coverage over DSP, phase sync, AI inference, and bridge communications.

By implementing the clean roots fix (R3), global favorites listing API (R1), multi-root search traversal (R2), and integrating the 5,000+ files performance benchmark suite (R4), the extension will achieve complete compliance with all product requirements and performance SLAs.

---
*Report prepared by Explorer 3 (Build, Tests & Performance Benchmark Specialist).*
