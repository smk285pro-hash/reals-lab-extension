# DISPATCH

## 2026-09-01T01:25:11Z
You are the Lead Implementation Worker for Reals Lab REAPER Extension.
Your working directory is `c:\Users\smk28\Desktop\reals lab extension\.agents\worker_m1_m4\`.

You MUST read and strictly adhere to:
- `c:\Users\smk28\Desktop\reals lab extension\ORIGINAL_REQUEST.md`
- `c:\Users\smk28\Desktop\reals lab extension\PROJECT.md`
- `c:\Users\smk28\Desktop\reals lab extension\TEST_INFRA.md`
- `c:\Users\smk28\Desktop\reals lab extension\AGENTS.md`
- `c:\Users\smk28\Desktop\reals lab extension\PLAN.md`
- `c:\Users\smk28\Desktop\reals lab extension\DESIGN.md`
- `c:\Users\smk28\Desktop\reals lab extension\SPEC.md`
- Survey reports:
  - `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_core\analysis.md`
  - `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_ui\analysis.md`
  - `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_tests\analysis.md`

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

GitNexus Code Intelligence:
- Run impact analysis before modifying symbols.
- Run `detect_changes()` before completing.
- Adhere to C++20 standard, zero-warning policy (`/W4 /permissive- /utf-8 /FS /WX`).
- Ensure UI texts use `tr("key")` where applicable.

Your Implementation Objectives:
1. **R3 (Clean Default Roots)**:
   - In `core/src/browser/BrowserModel.cpp`, remove the hardcoded initial insertion of `Music`, `Desktop`, and `Downloads` on fresh installs. Fresh installs must start with 0 default roots and clean state.
2. **R1 (Global Favorites View `★`)**:
   - In `core/include/reals/browser/BrowserModel.h` and `core/src/browser/BrowserModel.cpp`, add `std::vector<FileEntry> getFavoriteEntries() const` (or `listFavorites()`) resolving all valid file entries for favorited paths across all roots and subfolders. Ensure thread safety with `m_storeMutex`.
   - In `bridge/src/Bridge.cpp`, add `browser.getFavoriteEntries` (or `browser.favorites.listEntries`) RPC command returning `{ "files": [...] }`.
   - In `ui-web/app.js`, update `#favOnly` toggle logic: when `#favOnly` is activated, fetch and display all favorited audio/MIDI files globally across the entire library in the file list with full support for preview, transpose, drag into REAPER, tagging, and untagging/un-favoriting. When toggled off, cleanly restore previous folder view and scroll position.
3. **R2 (Global Search Across All Roots & Syntax Filters)**:
   - In `bridge/src/Bridge.cpp` (`runSearch`), when `base` is empty (Global Search), query the database across the whole library, and in the crawler fallback loop through all configured roots in `model.roots()` recursively (<50ms response).
   - In `ui-web/app.js`, when search query is entered, pass empty `base` for global search. When search is cleared (`#searchClear` or empty input), immediately restore previous folder view and scroll position.
4. **R4 & E2E Testing Suites (Tiers 1-5)**:
   - Create `tests/unit/TestSuite_Requirements_R1_R2_R3.cpp` covering:
     - R3: Zero default roots on clean initialization.
     - R1: Global favorites listing, multi-directory favorites aggregation, invalid path pruning.
     - R2: Global multi-root search, `/tag`, `/bpm:range`, `/key:note` filters across multiple roots, view restoration.
   - Create `tests/benchmarks/TestSuite_PerformanceBenchmark.cpp` verifying:
     - 5,000+ files synthetic directory tree creation and listing in <30ms.
     - Global search across 5,000+ files in <30ms.
     - 16-thread concurrency stress test for thread safety across `BrowserModel` and `SearchEngine`.
     - Zero memory leaks verification.
   - Register the new test suites in `tests/main.cpp` and `CMakeLists.txt`.
5. **Verification**:
   - Run `cmake --build --preset windows` and ensure 0 warnings and 0 errors.
   - Run `ctest --preset windows` and ensure 100% tests pass.
