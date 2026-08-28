# R2 Code Quality, Memory Management, Concurrency & Real-Time Audio Safety Audit Report

## 1. Observation

Direct observations from source code inspections across `core/`, `bridge/`, `extension/`, `shell/`, and `tests/`:

### Obs 1: Audio Callback Mutex Lock & Dynamic Vector Resizing on Real-Time Audio Thread
- **File**: `core/src/audio/Engine.cpp`
- **Lines 83–182 (`dsp_on_read`)**:
```cpp
ma_result dsp_on_read(ma_data_source* pDataSource, void* pFramesOut, ma_uint64 frameCount, ma_uint64* pFramesRead) {
    ...
    std::lock_guard lock(ds->dspMutex); // Line 94: std::recursive_mutex locked on audio thread
    ...
    if (ds->readBuffer.size() < kChunkFrames * static_cast<size_t>(channels)) {
        ds->readBuffer.resize(kChunkFrames * static_cast<size_t>(channels)); // Line 149: heap allocation
    }
    ...
    const ma_result res = ma_decoder_read_pcm_frames(&ds->decoder, ds->readBuffer.data(), kChunkFrames, &framesRead); // Line 153: synchronous disk I/O
```
- **Lines 587, 604, 655**: Main thread takes `dspMutex` in `setTimeRatio`, `setPitchSemitones`, `setLoop`.

### Obs 2: Missing `#pragma once` Header Guard in `SearchEngine.h`
- **File**: `core/include/reals/search/SearchEngine.h`
- **Lines 1–10**:
```cpp
#include "reals/db/Database.h"
#include "reals/search/QueryParser.h"
#include "reals/search/SemanticSearch.h"
```
Header starts directly with `#include` without `#pragma once` or `#ifndef` include guard.

### Obs 3: Infinite Loop Hang on Non-Finite BPM in `TempoDetector::disambiguateBpm`
- **File**: `core/src/ai/TempoDetector.cpp`
- **Lines 20–29**:
```cpp
float disambiguateBpm(float bpm) {
    if (bpm <= 0.0f) return 120.0f;
    while (bpm < 70.0f) {
        bpm *= 2.0f;
    }
    while (bpm > 180.0f) {
        bpm /= 2.0f;
    }
    return bpm;
}
```
If `exactLag` is 0 or NaN at line 180, `calculatedBpm` evaluates to `+infinity` or `NaN`. For `+infinity`, `bpm > 180.0f` is permanently true (`inf / 2.0f == inf`), creating an unrecoverable infinite loop.

### Obs 4: FFT Heap Buffer Overflow Risk on Non-Power-of-Two Size
- **File**: `core/src/ai/FeatureExtractor.cpp`
- **Lines 89–102**:
```cpp
for (size_t len = 2; len <= n; len <<= 1) {
    ...
    for (size_t i = 0; i < n; i += len) {
        ...
        for (size_t k = 0; k < len / 2; ++k) {
            const std::complex<float> u = x[i + k];
            const std::complex<float> v = x[i + k + len / 2] * w; // Out of bounds if n not power of 2
            x[i + k] = u + v;
            x[i + k + len / 2] = u - v;
        }
    }
}
```
If `n` is not a power of 2, the loop bounds calculate `i + k + len / 2 >= n`, resulting in out-of-bounds array access on `std::vector::operator[]`.

### Obs 5: Missing Mutex Synchronization in `Database::close()`
- **File**: `core/src/db/Database.cpp`
- **Lines 213–219**:
```cpp
void Database::close() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
    m_path.clear();
}
```
`close()` mutates `m_db` and `m_path` without acquiring `m_mutex`, while all other methods (`open`, `isOpen`, `upsertSample`, `querySamples`, `getSampleById`) guard operations with `m_mutex`.

### Obs 6: Thread Safety & Data Race on Unsynchronized Reference Getters in `BrowserModel`
- **File**: `core/include/reals/browser/BrowserModel.h`
- **Lines 40, 54, 56, 63**:
```cpp
[[nodiscard]] const std::vector<Root>& roots() const { return m_roots; }
[[nodiscard]] const std::vector<std::string>& favorites() const { return m_favorites; }
[[nodiscard]] const std::deque<std::string>& recents() const { return m_recents; }
[[nodiscard]] const std::unordered_map<std::string, int>& tags() const { return m_tags; }
```
These getters return const references without locking `m_storeMutex`. Concurrent calls to `addRoot`, `removeRoot`, `toggleFavorite`, `addRecent`, `setTag`, `rewritePath`, `forgetPath` mutate these containers under `m_storeMutex`, causing data races and iterator invalidation on reader threads.

### Obs 7: Owning Raw Pointers in `Engine.h` and `HttpClient.h`
- **File**: `core/include/reals/audio/Engine.h` Line 89: `Impl* m_impl = nullptr;`
- **File**: `core/src/audio/Engine.cpp` Lines 268, 582, 599: `m_impl = new Impl();` / Line 262: `delete m_impl;`
- **File**: `core/include/reals/net/HttpClient.h` Line 51: `Impl* m_impl = nullptr;`
- **File**: `core/src/net/HttpClient.cpp` Line 173: `m_impl(new Impl())` / Line 175: `delete m_impl;`

### Obs 8: Hardcoded Path Separator in `I18n.cpp`
- **File**: `core/src/i18n/I18n.cpp`
- **Lines 156–163**:
```cpp
#ifdef _WIN32
constexpr char sep = '\\';
#else
constexpr char sep = '/';
#endif
loadTable(dir + sep + "strings_vi.json", g_vi);
loadTable(dir + sep + "strings_en.json", g_en);
```
Violates the rule that all path manipulations must route strictly through `reals::platform::joinPath`.

### Obs 9: Unbounded Memory Cache Growth in `DragExporter.cpp`
- **File**: `core/src/audio/DragExporter.cpp`
- **Lines 130, 230, 319**:
```cpp
static std::unordered_map<std::string, CachedEntry> s_memCache;
static std::mutex s_cacheMutex;
```
Entries are added indefinitely to `s_memCache` without eviction or capacity capping.

---

## 2. Logic Chain

1. **Audio Real-Time Safety Violation (Obs 1)**:
   - *Premise 1*: Audio callbacks on miniaudio threads operate in hard real-time deadlines (~5–10ms per block).
   - *Premise 2*: AGENTS.md Rule 3 and audio DSP best practices forbid mutex locking, dynamic allocations (`new`/`vector::resize`), and blocking OS I/O in audio callbacks.
   - *Inference*: Locking `dspMutex` in `dsp_on_read` when the UI thread can hold `dspMutex` during UI parameter changes introduces priority inversion and causes immediate audio buffer dropouts / glitches. Calling `ma_decoder_read_pcm_frames` inside `dsp_on_read` blocks the audio thread on storage subsystem latency.

2. **Infinite Hang in Tempo Detection (Obs 3)**:
   - *Premise 1*: `TempoDetector::detectAlgorithmic` computes BPM via `(frameRate * 60.0f) / exactLag`.
   - *Premise 2*: In silent, corrupted, or synthetic signals with zero lag delta or arithmetic edge cases, `exactLag` can be zero, yielding `+inf` or `NaN`.
   - *Inference*: Passing `+inf` to `disambiguateBpm` evaluates `inf > 180.0f` to true; `inf / 2.0f` remains `inf`, creating an infinite loop that consumes 100% CPU on worker threads and never returns.

3. **FFT Out-of-Bounds Indexing (Obs 4)**:
   - *Premise 1*: Radix-2 Cooley-Tukey FFT expects power-of-2 input lengths.
   - *Premise 2*: `FeatureExtractor::fft` performs butterfly operations without verifying power-of-2 length or bounds clamping on `i + k + len / 2`.
   - *Inference*: Any invocation with non-power-of-2 length (e.g. from custom `SpectrogramConfig`) will write outside the vector bounds, leading to memory corruption or crashes.

4. **Concurrency & Thread Race Conditions (Obs 5, Obs 6)**:
   - *Premise 1*: `Database` and `BrowserModel` are shared between the main UI thread, bridge dispatchers, and background scanner workers.
   - *Premise 2*: `Database::close()` mutates `m_db` without `m_mutex`, and `BrowserModel` getters return raw references to internal containers without holding `m_storeMutex`.
   - *Inference*: Simultaneous read and write access across threads induces undefined behavior, data races, and heap corruption during concurrent scans and UI navigation.

5. **Resource Management & Architecture Compliance (Obs 2, Obs 7, Obs 8)**:
   - *Premise 1*: AGENTS.md mandates `#pragma once` in all headers, `std::unique_ptr` for PIMPL ownership, and path resolution via `platform::joinPath`.
   - *Inference*: `SearchEngine.h` without header guard risks multi-inclusion redefinition errors; raw pointer `m_impl` violates smart pointer rules; manual path concatenation in `I18n.cpp` violates platform abstraction invariants.

---

## 3. Caveats

- **External Libraries**: Third-party libraries in `libs/` (reaper-sdk, SoundTouch, miniaudio) were audited at the interface boundary; their internal vendor implementations are treated as external dependencies.
- **Audio Thread Double-Buffering**: Eliminating disk I/O from `dsp_on_read` requires an asynchronous ring-buffer / pre-buffering reader thread rather than a synchronous decoder call inside the callback.
- **Test Coverage**: All test suites in `tests/` pass under normal execution, but stress tests on concurrency could intermittently expose the unsynchronized getters in `BrowserModel`.

---

## 4. Conclusion & Actionable Findings Matrix

### Categorized Findings

| ID | Severity | File & Line | Rule / Contract Violated | Concrete Remediation |
|---|---|---|---|---|
| **R2-01** | **Critical** | `core/src/audio/Engine.cpp`:94,149,153 | Real-Time Audio Safety (AGENTS.md Rule 3: No locks/allocations/disk I/O in audio callback) | Replace `dspMutex` in `dsp_on_read` with atomic variables / lock-free FIFO queue. Pre-allocate `readBuffer` to fixed max chunk size (never `resize` in callback). Decouple disk decoding into a background fill thread with a lock-free ring buffer. |
| **R2-02** | **Critical** | `core/src/ai/TempoDetector.cpp`:20-29 | Arithmetic Safety / Loop Invariant | Add `!std::isfinite(bpm) \|\| bpm <= 0.0f` check at entry to `disambiguateBpm` to prevent infinite loop hang on `+inf`/`NaN`. |
| **R2-03** | **Major** | `core/src/ai/FeatureExtractor.cpp`:70-103 | Memory Safety / Bounds Checking | Validate that `x.size()` is a power of 2 (`(n & (n - 1)) == 0`). If not, resize/pad to next power of 2 before running FFT. |
| **R2-04** | **Major** | `core/include/reals/search/SearchEngine.h`:1 | Header Hygiene (AGENTS.md Rule 3: `#pragma once`) | Add `#pragma once` as line 1 of `SearchEngine.h`. |
| **R2-05** | **Major** | `core/src/db/Database.cpp`:213-219 | Concurrency & Thread Safety | Add `std::lock_guard lock(m_mutex);` inside `Database::close()` to prevent concurrent teardown races. |
| **R2-06** | **Major** | `core/include/reals/browser/BrowserModel.h`:40,54,56,63 | Thread Safety / Data Race | Return snapshot copies under `std::lock_guard lock(m_storeMutex)` for `roots()`, `favorites()`, `recents()`, and `tags()`. |
| **R2-07** | **Major** | `core/include/reals/audio/Engine.h`:89, `core/include/reals/net/HttpClient.h`:51 | Memory Management (AGENTS.md Rule 3: Smart pointers, no owning raw `new`/`delete`) | Convert `Impl* m_impl` to `std::unique_ptr<Impl> m_impl` in both `Engine` and `HttpClient`. |
| **R2-08** | **Minor** | `core/src/i18n/I18n.cpp`:161-162 | Architecture Isolation (AGENTS.md Rule 2: Path through `platform::`) | Replace manual `dir + sep + ...` string concatenation with `platform::joinPath(dir, "strings_vi.json")`. |
| **R2-09** | **Minor** | `core/src/audio/DragExporter.cpp`:130,230 | Memory Leak / Unbounded Cache | Add an LRU cache size cap (e.g. max 256 items) to `s_memCache` to prevent unbounded memory growth. |
| **R2-10** | **Style/Lint** | `extension/src/reaper_plugin.cpp`:84-111 | Naming Conventions (AGENTS.md Rule 3: `s_` static / camelCase) | Standardize global static variables from `g_` prefix to `s_` prefix. |

---

### Concrete Fix Proposals (Code Snippets)

#### Fix 1: `TempoDetector.cpp` (Prevent Infinite Loop)
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

#### Fix 2: `SearchEngine.h` (Missing `#pragma once`)
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

#### Fix 3: `Database.cpp` (Thread-Safe Close)
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

#### Fix 4: `BrowserModel.h` (Thread-Safe Snapshots)
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

#### Fix 5: `Engine.h` & `HttpClient.h` (Smart Pointer Ownership)
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

---

## 5. Verification Method

To independently verify all findings and test remediations:

1. **Header Guard Verification**:
   Verify compilation when `SearchEngine.h` is included multiple times in a single compilation unit:
   ```powershell
   cmake --build --preset windows --target reals_core
   ```

2. **Infinite Loop Reproduction in Tempo Detection**:
   Execute `test_ai` with NaN / non-finite inputs:
   ```cpp
   float nanBpm = std::numeric_limits<float>::infinity();
   // Calling disambiguateBpm(nanBpm) must return 120.0f without hanging
   ```

3. **Concurrency Stress Tests**:
   Run the test suite under MSVC Release / Debug:
   ```powershell
   ctest --preset windows --output-on-failure
   ```

4. **Real-Time Audio Safety Assertions**:
   Inspect audio callback profiling with zero-lock instrumentation on `dsp_on_read`.
