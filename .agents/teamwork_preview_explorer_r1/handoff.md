# Handoff Report: Architecture & Layer Boundary Audit (R1)

**Auditor Role**: Architecture & Layer Boundary Auditor  
**Scope**: `core/`, `ui/`, `bridge/`, `shell/`, `extension/`, `app/`, `ui-web/`, `assets/i18n/`  
**Working Directory**: `c:/Users/smk28/Desktop/reals lab extension/.agents/teamwork_preview_explorer_r1/`  
**Governing Documents**: `AGENTS.md`, `SPEC.md`, `PROJECT.md`, `ORIGINAL_REQUEST.md`

---

## 1. Observation

Direct filesystem and codebase inspection across all project targets revealed the following verified facts:

### A. `#include` Directives & Layer Isolation
1. **`core/` Layer Isolation**:
   - Grep search confirmed **ZERO** inclusions of `imgui.h`, `GLFW/glfw3.h`, `reaper_plugin.h`, or `reaper_plugin_functions.h` across all files in `core/include/reals/` and `core/src/`.
   - **Direct OS / Win32 header leaks in `core/`**:
     * `core/src/browser/BrowserModel.cpp:23`: `#include <windows.h>`, lines 42–49 directly call Win32 APIs `MultiByteToWideChar`, `CharLowerBuffW`, `WideCharToMultiByte` for string lowering instead of using `platform::` or `util::` abstractions.
     * `core/src/config/Config.cpp:22`: `#include <windows.h>`, lines 23–30 define a local helper `utf8Path()` calling `MultiByteToWideChar` despite `platform::u8path()` already being defined in `reals/platform/Path.h`.
     * `core/src/scanner/BackgroundScanner.cpp:25`: `#include <windows.h>`, lines 624 & 626 invoke Win32 `SetThreadPriority(GetCurrentThread(), ...)` directly rather than using a platform thread abstraction (`platform::Thread`).
     * `core/src/util/Log.cpp:8`: `#include <windows.h>`, lines 39–42 call `MultiByteToWideChar` and `_wfopen_s`, and line 79 invokes `OutputDebugStringA`.
     * `core/src/net/HttpClient.cpp:20–21`: `#include <windows.h>` and `#include <winhttp.h>` are included **without** an `#ifdef _WIN32` guard (lines 14–22), breaking macOS and Linux builds where `windows.h` does not exist.
2. **`bridge/` Layer Isolation**:
   - `bridge/include/reals/bridge/Bridge.h` and `bridge/src/Bridge.cpp` have **ZERO** inclusions of GLFW, REAPER SDK, or WebView2 headers. All host operations are delegated cleanly through the abstract `IHostActions` interface.
3. **`shell/` Layer Isolation**:
   - `shell/win/WebViewHost.h` / `WebViewHost.cpp` and `shell/win/OleDrag.h` / `OleDrag.cpp` interact solely with Win32, WebView2, and OLE without referencing REAPER SDK headers.
4. **`app/` Standalone Shell**:
   - The directory `app/` is empty. The legacy ImGui standalone application entry point was removed following the WebView2 pivot (P1.5), and no desktop WebView2 standalone application (`reals_app.exe`) shell exists yet.

---

### B. UI Text Localization & i18n Cross-Referencing
1. **JSON Asset Integrity**:
   - `assets/i18n/strings_en.json`: 167 keys.
   - `assets/i18n/strings_vi.json`: 167 keys.
   - Symmetric key parity between EN and VI files is 100% (0 mismatched keys).
2. **Hardcoded User-Facing Strings in `ui-web/index.html`**:
   - `ui-web/index.html:29`: `title="Dock vào REAPER / Cửa sổ riêng"` (missing `data-i18n-title`)
   - `ui-web/index.html:30`: `title="Cài đặt"` (missing `data-i18n-title`)
   - `ui-web/index.html:31`: `title="Thu nhỏ"` (missing `data-i18n-title`)
   - `ui-web/index.html:32`: `title="Phóng to / Khôi phục"` (missing `data-i18n-title`)
   - `ui-web/index.html:33`: `title="Đóng"` (missing `data-i18n-title`)
   - `ui-web/index.html:102`: `title="Ẩn/Hiện Cây thư mục"` (missing `data-i18n-title`)
   - `ui-web/index.html:117`: `title="Yêu thích"` (missing `data-i18n-title`)
   - `ui-web/index.html:139–141`: `<option value="low">🟢 30% CPU</option>`, `<option value="normal" selected>🔵 50% CPU</option>`, `<option value="high">🟠 85% CPU</option>` (hardcoded text, missing `data-i18n`)
   - `ui-web/index.html:151`: `title="Kéo để chỉnh độ rộng Cây thư mục"` (missing `data-i18n-title`)
   - `ui-web/index.html:154`: `title="Kéo để chỉnh chiều cao Trình phát"` (missing `data-i18n-title`)
   - `ui-web/index.html:160`: `title="Key Transposer"` (missing `data-i18n-title`)
3. **Hardcoded Strings in `ui-web/app.js`**:
   - `ui-web/app.js:511`: `toast('Lab error: ' + (data.error || ''))`
   - `ui-web/app.js:823`: `toast('Sync: không tìm thấy BPM, thử 120')` (hardcoded Vietnamese)
   - `ui-web/app.js:1530`: `toast(tr('browser.noResults') || 'No similar samples found')` (fallback hardcoded English; `browser.noResults` missing in dictionaries)
   - `ui-web/app.js:1534`: `toast('Error finding similar samples')` (hardcoded English)
   - `ui-web/app.js:2126`: `toast('Scanner error: ' + (err || ''))`
4. **i18n Desynchronization Defect**:
   - `ui-web/app.js` embeds a static JavaScript dictionary `const I18N = { vi: {...}, en: {...} }` containing only 129 keys. It does not fetch or synchronize with `assets/i18n/strings_*.json` (167 keys).
   - **15 active keys** called in `ui-web/app.js` are missing from `app.js`'s internal dictionary, resulting in raw translation keys displayed to users:
     * `browser.clearSimilar`, `browser.ctx.findSimilar`, `browser.ctx.rescanAll`, `browser.ctx.scanNew`, `browser.matchPercent`, `browser.similarTo`
     * `browser.noResults` (also completely absent from `assets/i18n/*.json`)
     * `scanner.addedCount`, `scanner.cancelled`, `scanner.cpuMode`, `scanner.cpuMode.high`, `scanner.cpuMode.highWarn`, `scanner.cpuMode.low`, `scanner.cpuMode.normal`, `scanner.starting`
   - `ui-web/app.js:449–452`: `applyI18n()` only queries `[data-i18n]` and `[data-i18n-ph]`. It ignores `[data-i18n-title]`, preventing element tooltips (e.g. lines 138, 146 in `index.html`) from updating when switching languages.
5. **C++ Embedded Fallback Gaps**:
   - `core/src/i18n/I18n.cpp:26–123`: `kEmbedded` contains only 93 keys, leaving 76 keys undefined if disk assets are missing.
6. **Hardcoded C++ UI Text in `extension/src/reaper_plugin.cpp`**:
   - Line 81, 1019, 1083: `"Reals Lab: Show Window"`
   - Line 339: `Undo_EndBlock("Reals Lab: Insert media", 0)`
   - Line 743, 831: `"Reals Lab"`

---

### C. File Size Limits & Responsibility Monoliths (>400 lines)
The following source and header files exceed the ~400 lines architectural guideline (`AGENTS.md` Rule 3):

| File Path | Total Lines | Primary Responsibilities Mixed In File |
|---|---|---|
| `ui-web/app.js` | **2607** | Embedded i18n dict, Bridge RPC, tree navigation, audio waveform canvas, tone transposer, AI similarity banner, scanner UI bar, Audio Lab polling, settings popover |
| `bridge/src/Bridge.cpp` | **1536** | Bridge lifecycle, event queues, background threads, and a 1000-line monolithic `Bridge::handle` dispatcher for fs, browser, audio, lab, scanner, reaper, window, config |
| `extension/src/reaper_plugin.cpp` | **1117** | REAPER DLL lifecycle, take playrate sync & stretch calculations (Mechanism A/B/C), Win32 window creation, subclassing, docking, message dispatch, `ExtHostActions` |
| `mockup.html` | **1118** | Stale design prototype |
| `ui-web/app.css` | **917** | Global CSS styles, resets, themes, layouts, component styles |
| `core/src/db/Database.cpp` | **874** | SQLite connection management, migrations, CRUD, FTS5 full-text queries, cosine similarity vector queries |
| `core/src/scanner/BackgroundScanner.cpp` | **816** | Multi-threaded tree scanner, thread pool, CPU throttling, AI feature extraction coordination, SQLite batch upserts |
| `core/src/audio/Engine.cpp` | **713** | miniaudio device handling, real-time lock-free ring buffer playback, SoundTouch DSP, offline BPM/Key analysis, waveform envelope generation |
| `core/src/browser/BrowserModel.cpp` | **498** | Directory listing, favorites/recents persistence, path filtering, file tagging |
| `core/src/util/Hash.cpp` | **444** | SHA-256 and audio fingerprinting implementations |
| `core/src/ai/FeatureExtractor.cpp` | **420** | Multi-stage audio feature extraction and spectral analysis |
| `core/src/net/HttpClient.cpp` | **406** | Windows WinHTTP client transport and multipart builder |
| `core/src/search/SearchEngine.cpp` | **400** | Combined text, attribute, and vector search query executor |

---

## 2. Logic Chain

1. **Layer Boundary Violation in `core/`**:
   - `SPEC.md` Section 3 mandates that `core/` has zero dependencies on UI/host and that all OS/platform specific code resides behind `platform/` abstractions (`Path.h`, `Thread.h`, `Dll.h`).
   - Observations 1.A.1 show direct calls to Win32 APIs in `BrowserModel.cpp`, `Config.cpp`, `BackgroundScanner.cpp`, and `Log.cpp`. This bypasses `platform/` abstractions and introduces unneeded Windows API coupling in core business logic.
   - In `HttpClient.cpp`, omitting `#ifdef _WIN32` around `#include <windows.h>` and `<winhttp.h>` means non-Windows CMake targets will fail compilation immediately.

2. **Thin Shell Violation in `extension/src/reaper_plugin.cpp`**:
   - `AGENTS.md` Rule 2 dictates that `extension/` is a thin shell and business logic must reside in `core/` or `bridge/`.
   - Observation 1.C shows `reaper_plugin.cpp` containing 165 lines of audio item stretching and tempo synchronization logic (`processPendingSyncPlayrates`).
   - Because this logic is trapped inside the REAPER DLL shell file, test suites (`TestSuite_EmpiricalChallenger_R2.cpp:503`) were forced to replicate/emulate the logic rather than testing the real implementation.

3. **Localization Breakdowns**:
   - `AGENTS.md` Rule 0 & Rule 3 state: "UI text: KHÔNG BAO GIỜ hardcode — luôn qua tr('key'), string nằm trong assets/i18n/".
   - Hardcoded strings in `index.html` (tooltips, options) and `app.js` (toasts) violate this rule directly.
   - The architectural divergence where `app.js` embeds its own subset of translations instead of loading from `assets/i18n/` causes 15 missing translation keys in the Web UI, breaking localization at runtime.

---

## 3. Caveats

1. **Non-Windows Platforms (macOS / Linux)**:
   - Mac (WKWebView) and Linux (WebKitGTK) shell implementations are scheduled for Phase 6 (`SPEC.md`). The audit evaluated the Windows implementation and verified cross-platform code cleanliness in `core/` and `bridge/`.
2. **ImGui Deprecation (`ui/`)**:
   - The directory `ui/` was deprecated and removed following the WebView2 pivot (P1.5). `CMakeLists.txt` conditionally skips building `reals_app` and `reals_ui` when `app/main.cpp` is not found.

---

## 4. Conclusion & Categorized Findings

### Summary of Audit Findings

| Severity | Count | Summary |
|---|:---:|---|
| **Critical** | 2 | `HttpClient.cpp` unguarded Win32 headers break cross-platform builds; `app.js` i18n desynchronization causes 15 missing keys & broken UI strings |
| **Major** | 4 | Business logic leak in `reaper_plugin.cpp`; Direct Win32 API calls in `core/` (`BrowserModel`, `Config`, `Scanner`, `Log`); Monolithic files >1000 lines (`app.js`, `Bridge.cpp`, `reaper_plugin.cpp`); Missing `data-i18n-title` support in `applyI18n()` |
| **Minor** | 4 | 11 hardcoded tooltips/options in `index.html`; 5 hardcoded toast/log strings in `app.js`; Missing `browser.noResults` key in JSON; 76 keys missing from C++ embedded fallback table |
| **Style / Lint** | 2 | Empty `app/` folder without standalone WebView2 entry point; Redundant `mockup.html` file |

---

### Detailed Findings & Concrete Remediation Plans

#### [Critical-01] Unguarded Win32 / WinHTTP Headers in `core/src/net/HttpClient.cpp`
- **Location**: `core/src/net/HttpClient.cpp:14–22`
- **Rule Violated**: `SPEC.md` §2, `AGENTS.md` Rule 2 (Cross-platform core library)
- **Problem**: `<windows.h>` and `<winhttp.h>` are included without `#ifdef _WIN32`. Compiling `reals_core` on macOS or Linux will fail immediately.
- **Remediation**: Guard Windows-specific implementation with `#ifdef _WIN32` or split into `core/src/net/HttpClient_win.cpp` and `core/src/net/HttpClient_curl.cpp` selected via CMake.

#### [Critical-02] `ui-web/app.js` i18n Desynchronization & 15 Missing Translation Keys
- **Location**: `ui-web/app.js:5–140`
- **Rule Violated**: `AGENTS.md` Rule 0 ("UI text: KHÔNG BAO GIỜ hardcode — luôn qua tr('key'), string nằm trong assets/i18n/")
- **Problem**: `app.js` uses an outdated, hardcoded embedded dictionary (129 keys) rather than loading `assets/i18n/strings_*.json` (167 keys). 15 active keys (`browser.clearSimilar`, `browser.ctx.findSimilar`, `browser.ctx.rescanAll`, `browser.ctx.scanNew`, `browser.matchPercent`, `browser.similarTo`, `browser.noResults`, `scanner.addedCount`, `scanner.cancelled`, `scanner.cpuMode`, `scanner.cpuMode.high`, `scanner.cpuMode.highWarn`, `scanner.cpuMode.low`, `scanner.cpuMode.normal`, `scanner.starting`) render as raw key strings.
- **Remediation**:
  1. Add missing key `"browser.noResults": "No results found"` / `"Không tìm thấy kết quả"` to `strings_en.json` and `strings_vi.json`.
  2. Have `app.js` request localized strings via Bridge RPC or load them from assets at runtime, eliminating the duplicate static dictionary in JavaScript.

#### [Major-01] Business Logic & Playrate Synchronization Leak in `extension/src/reaper_plugin.cpp`
- **Location**: `extension/src/reaper_plugin.cpp:114–265`
- **Rule Violated**: `AGENTS.md` Rule 2 ("app/ và extension/ là shell mỏng — logic nghiệp vụ phải nằm ở core/")
- **Problem**: 165 lines of audio item stretching, tempo grid calculation, and take playrate management (`processPendingSyncPlayrates`) are embedded in the extension shell, making them untestable in unit test suites without emulated mock copies.
- **Remediation**: Extract into a dedicated class (e.g. `reals::extension::ReaperPlayrateSync` in `extension/src/ReaperPlayrateSync.cpp` / `.h`) with clean interfaces that can be linked and tested directly in test suites.

#### [Major-02] Direct Win32 API Calls in `core/` Modules
- **Location**:
  * `core/src/browser/BrowserModel.cpp:23, 42–49` (`MultiByteToWideChar`, `CharLowerBuffW`)
  * `core/src/config/Config.cpp:22–30` (manual `utf8Path()` instead of `platform::u8path()`)
  * `core/src/scanner/BackgroundScanner.cpp:25, 624, 626` (`SetThreadPriority`)
  * `core/src/util/Log.cpp:8, 39–42, 79` (`MultiByteToWideChar`, `_wfopen_s`, `OutputDebugStringA`)
- **Rule Violated**: `AGENTS.md` Rule 2 ("Path chỉ qua platform::. Core không phụ thuộc UI/host"), `SPEC.md` §3
- **Remediation**: Replace direct Win32 calls with `platform::` and `util::` helpers (`platform::u8path`, `platform::toLowerUtf8`, `platform::setThreadPriority`).

#### [Major-03] Giant Monolithic Source Files Violating ~400 Line Limit
- **Location**: `ui-web/app.js` (2607 lines), `bridge/src/Bridge.cpp` (1536 lines), `extension/src/reaper_plugin.cpp` (1117 lines), `core/src/db/Database.cpp` (874 lines), `core/src/scanner/BackgroundScanner.cpp` (816 lines), `core/src/audio/Engine.cpp` (713 lines)
- **Rule Violated**: `AGENTS.md` Rule 3 ("1 class trách nhiệm rõ ràng; file ≤ ~400 dòng thì tách")
- **Remediation Proposals**:
  * **`bridge/src/Bridge.cpp`**: Modularize `Bridge::handle` into domain command handlers: `FsHandlers.cpp`, `BrowserHandlers.cpp`, `AudioHandlers.cpp`, `ScannerHandlers.cpp`, `LabHandlers.cpp`, `ReaperHandlers.cpp`.
  * **`extension/src/reaper_plugin.cpp`**: Decompose into `ReaperWindow.cpp` (Win32 & DWM window lifecycle), `ReaperPlayrateSync.cpp` (take stretch & phase sync), `ReaperHostActions.cpp` (`IHostActions`), and `reaper_plugin.cpp` (entry point & plugin registration).
  * **`ui-web/app.js`**: Split into ES modules: `i18n.js`, `bridge.js`, `browser.js`, `player.js`, `scanner.js`, `lab.js`, `market.js`, `settings.js`.
  * **`core/src/db/Database.cpp`**: Separate vector similarity search into `DbVectorSearch.cpp` and FTS5 search into `DbFtsSearch.cpp`.
  * **`core/src/audio/Engine.cpp`**: Separate offline audio analysis (BPM/Key detection, envelope generation) into `AudioAnalyzer.cpp`.

#### [Major-04] `applyI18n()` Ignores `data-i18n-title` Attributes
- **Location**: `ui-web/app.js:449–452`
- **Rule Violated**: `AGENTS.md` Rule 0 ("UI text: KHÔNG BAO GIỜ hardcode — luôn qua tr('key')")
- **Problem**: `applyI18n()` only queries `[data-i18n]` and `[data-i18n-ph]`. Elements using `data-i18n-title` (such as `scannerCpuMode` and `btnScannerCancel` in `index.html`) never update their `title` attribute when switching languages.
- **Remediation**: Add `$$('[data-i18n-title]').forEach((e) => (e.title = tr(e.dataset.i18nTitle)));` to `applyI18n()`.

#### [Minor-01] Hardcoded Display Strings & Tooltips in `ui-web/index.html`
- **Location**: `ui-web/index.html:29–33, 102, 117, 139–141, 151, 154, 160`
- **Problem**: Hardcoded Vietnamese strings in `title` attributes and `<option>` elements.
- **Remediation**: Add `data-i18n-title` to title attributes and `data-i18n` to `<option>` tags, routing through standard keys in `strings_*.json`.

#### [Minor-02] Hardcoded Toast / Error Messages in `ui-web/app.js`
- **Location**: `ui-web/app.js:511, 823, 1530, 1534, 2126`
- **Problem**: Hardcoded string prefixes (`'Lab error: '`, `'Sync: không tìm thấy BPM, thử 120'`, `'Error finding similar samples'`, `'Scanner error: '`).
- **Remediation**: Wrap all toast and error messages with `tr(...)` backed by keys in `strings_*.json`.

#### [Minor-03] 76 Keys Missing from C++ Embedded Fallback Table
- **Location**: `core/src/i18n/I18n.cpp:26–123`
- **Problem**: `kEmbedded` contains only 93 keys while `strings_*.json` has 167 keys.
- **Remediation**: Synchronize `kEmbedded` table with all 167 keys from `strings_en.json` and `strings_vi.json`.

---

## 5. Verification Method

To independently verify all findings in this audit report:

1. **Verify #Include & Platform Isolation**:
   ```powershell
   # Check core isolation against forbidden UI/host headers:
   python -c "import os, glob; [print(f, l) for f in glob.glob('core/**', recursive=True) if f.endswith(('.h','.cpp')) for l in open(f, errors='ignore') if any(x in l.lower() for x in ['imgui', 'glfw', 'reaper_plugin'])]"
   # Verify HttpClient unguarded windows.h:
   Get-Content core/src/net/HttpClient.cpp -Head 25
   ```

2. **Verify Localization Keys & Desynchronization**:
   ```powershell
   # Run the verification script:
   python .agents/teamwork_preview_explorer_r1/comprehensive_i18n_audit.py
   ```

3. **Verify File Sizes**:
   ```powershell
   # Count lines in files exceeding 400 lines:
   Get-ChildItem -Recurse -File | Where-Object { $_.FullName -notmatch '[\\/](build|\.git|\.agents|\.claude|\.gitnexus|libs)[\\/]' } | Select-Object @{N='Path';E={$_.FullName.Replace((Get-Location).Path+'\','')}}, @{N='Lines';E={(Get-Content -LiteralPath $_.FullName | Measure-Object -Line).Lines}} | Where-Object { $_.Lines -gt 400 } | Sort-Object Lines -Descending
   ```
