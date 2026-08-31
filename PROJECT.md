# Project: Reals Lab Theme Engine

## Architecture
- **Layer 1: Frontend Design Tokens & UI (`ui-web/`)**:
  - `tokens.css`: 82 semantic tokens defined in `:root, html[data-theme="dark-studio"]` and 100% overridden in `html[data-theme="pastel-pink"]` and `html[data-theme="cyberpunk"]`.
  - `app.css`: UI component styling without hardcoded colors.
  - `app.js`: `ThemeManager` class, `themeUpdated` CustomEvent dispatch, in-memory canvas color cache, Settings modal theme picker chips, i18n support.
  - `index.html`: Inline `<head>` fast bootstrap script reading `localStorage.getItem('reals_theme')`, transparent app shell, Settings theme picker UI.
- **Layer 2: WebView2 Host & Zero-FOUC Window Shell (`shell/win/WebViewHost.cpp`)**:
  - Win32 host window with dark background brush `#0D0E11` and DWM dark title bar.
  - `ICoreWebView2Controller2::put_DefaultBackgroundColor({0,0,0,0})` transparent initialization.
  - Hidden pre-warming via `put_IsVisible(FALSE)` on REAPER startup.
  - Virtual host folder mapping `https://app.local` to `ui-web/`.
- **Layer 3: REAPER Extension & Native Persistence (`extension/src/reaper_plugin.cpp`)**:
  - Dynamic REAPER API binding via `REAPERAPI_LoadAPI(rec->GetFunc)`.
  - Section `"REALSLAB"`, key `"theme"`, `persist = true` via `SetExtState` and `GetExtState`.
  - Bidirectional plain-string IPC protocol `THEME_CHANGED:<name>` handling and JavaScript execution push.
  - Secondary fallback to `reals::config::Config::instance()`.
- **Layer 4: Automated Build, Deployment & Test Harness (`CMakeLists.txt`, `tests/`)**:
  - C++20 standard, `/W4` zero warnings on MSVC.
  - Automated post-build copy of `reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins/` with atomic `.old` rotation.
  - Consolidated test executable `reals_tests.exe` with dedicated `TestSuite_ThemeEngine` (42 tests) and CTest integration.

## Feature Inventory
| # | Feature | Description | Milestone | Source |
|---|---------|-------------|-----------|--------|
| 1 | 82 Design Tokens Matrix | Full CSS custom properties across surfaces, borders, text, accents, badges, waveforms, meters, piano roll | M1 | Survey / DESIGN.md / tokens.css |
| 2 | 3 Official Theme Palettes | `dark-studio`, `pastel-pink`, `cyberpunk` with 100% token override parity (246 definitions) | M1 | Survey / ORIGINAL_REQUEST.md / tokens.css |
| 3 | Theme Variable Mapping Cleanup | Clean up legacy variable names in `app.js:applyTheme()` (`--text-primary`, `--border-default`, `--accent`) | M1 | Survey / app.js |
| 4 | Inline Accent Conflict Resolution | Ensure `applyTheme()` removes/harmonizes inline `--accent` overrides from `applyAccent()` | M1 | Survey / app.js |
| 5 | Dynamic SVG Color Inheritance | All vector SVGs adapt color via `currentColor` or CSS tokens | M1 | Survey / index.html / app.css |
| 6 | Head Inline Bootstrap Script | Zero-FOUC inline script in `<head>` of `index.html` querying `localStorage` synchronously | M2 | Survey / ORIGINAL_REQUEST.md / index.html |
| 7 | WebView2 Transparent Background | `ICoreWebView2Controller2::put_DefaultBackgroundColor({0,0,0,0})` | M2 | Survey / WebViewHost.cpp |
| 8 | Hidden Pre-warm Visibility Gating | Controller `put_IsVisible(FALSE)` until host window ready | M2 | Survey / WebViewHost.cpp |
| 9 | REAPER `GetExtState` Persistence | Query saved theme from `reaper-extstate.ini` on startup | M2 | Survey / reaper_plugin.cpp |
| 10 | REAPER `SetExtState` Persistence | Save theme with `persist=true` upon theme change | M2 | Survey / reaper_plugin.cpp |
| 11 | Bidirectional String IPC | `THEME_CHANGED:<name>` protocol from JS to C++ and C++ to JS | M2 | Survey / ORIGINAL_REQUEST.md |
| 12 | Input Sanitization & Fallback | Sanitize theme name (trim, lowercase, validator) falling back to `dark-studio` | M2 | Survey / reaper_plugin.cpp |
| 13 | Live Canvas Color Cache | In-memory `canvasThemeColors` object in `app.js` avoiding `getComputedStyle` in 60FPS render loop | M3 | Survey / app.js |
| 14 | `themeUpdated` Event Subscription | Canvas and Piano roll redraw immediately upon receiving `themeUpdated` CustomEvent | M3 | Survey / ORIGINAL_REQUEST.md / app.js |
| 15 | Waveform & Meter Dynamic Redraw | `drawWaveform()` and `drawMeterSmoothed()` render with active theme tokens | M3 | Survey / app.js |
| 16 | Piano Roll Dynamic Redraw | MIDI note gradients and keys adapt to active theme tokens | M3 | Survey / app.js |
| 17 | Theme Picker UI in Settings Modal | `#optTheme` button chips in `#tab-general` with active indicator and click binding | M3 | Survey / index.html / app.js |
| 18 | Theme Picker i18n Support | Vietnamese and English strings for theme names in `app.js` and `assets/i18n/` | M3 | Survey / app.js |
| 19 | Zero-Warning C++20 Build | Strict compilation with `/W4` on MSVC | M4 | Survey / AGENTS.md / CMakeLists.txt |
| 20 | Early DLL Deployment to UserPlugins | Automated copy to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll` with atomic swap | M4 | Survey / ORIGINAL_REQUEST.md |
| 21 | Automated E2E Test Suite (CTest) | Comprehensive verification via `ctest --preset windows` and `TestSuite_ThemeEngine` | M4 | Survey / ORIGINAL_REQUEST.md |

## Milestones
| # | Name | Scope | Dependencies | Status |
|---|------|-------|-------------|--------|
| M1 | UI Design Tokens & Theme Palettes | `tokens.css`, `app.css`, `app.js` token query mapping, inline accent handling, SVG inheritance | none | DONE |
| M2 | Native Bridge, Zero-FOUC & ExtState Persistence | `reaper_plugin.cpp`, `WebViewHost.cpp`, `<head>` bootstrap script, `THEME_CHANGED:` IPC, fallback sanitization | none | DONE |
| M3 | Dynamic Canvas Sync & Theme Picker UI | `app.js` canvas color cache, `themeUpdated` listener, `drawWaveform()`, `drawMeterSmoothed()`, Settings modal `#optTheme` chips, i18n | M1, M2 | DONE |
| M4 | Build, Early Deployment & Comprehensive Testing | Build zero-warning DLL, deploy to `%APPDATA%/REAPER/UserPlugins/`, run `TestSuite_ThemeEngine` & `ctest --preset windows`, Python token parity test | M1, M2, M3 | DONE |

## Interface Contracts
### JS (`ThemeManager`) ↔ Native C++ (`reaper_plugin.cpp`)
- **JS -> C++**: `window.chrome.webview.postMessage("THEME_CHANGED:<name>")`
- **C++ -> JS**: `ExecuteScriptAsync(L"window.themeManager && window.themeManager.applyTheme('" + theme + L"', false);")`
- **Supported Theme Names**: `"dark-studio"`, `"pastel-pink"`, `"cyberpunk"`
- **Sanitization Rule**: Any unknown, empty, or malformed input must be sanitized and defaulted to `"dark-studio"`.

### `ThemeManager` ↔ Canvas Renderers (`app.js`)
- **Event Name**: `CustomEvent('themeUpdated', { detail: { theme: name, tokens: {...} } })`
- **Token Map Keys**: `waveformBg`, `waveformFill`, `waveformFillActive`, `waveformPlayhead`, `waveformCenterline`, `meterBg`, `meterFill`, `meterFillWarn`, `meterFillClip`, `pianorollBg`, `pianorollGrid`, `pianorollNote`, `pianorollNoteActive`, `pianorollNoteEnd`.
- **Canvas Rendering**: Read directly from `canvasTheme` cache without calling `getComputedStyle()` inside draw loop.

## Code Layout
- `ui-web/tokens.css` — Semantic design tokens and 3 theme palettes
- `ui-web/app.css` — App stylesheets using CSS variables
- `ui-web/app.js` — Frontend JS application, `ThemeManager`, canvas rendering, Settings modal
- `ui-web/index.html` — Main HTML shell, `<head>` inline bootstrap, UI layout
- `extension/src/reaper_plugin.cpp` — REAPER extension entry point, ExtState persistence, IPC message handler
- `shell/win/WebViewHost.cpp` — Win32 WebView2 controller, zero-FOUC configuration, background color
- `tests/suites/TestSuite_ThemeEngine.cpp` — C++ Unit and Integration tests for Theme Engine
- `tests/verify_tokens_test.py` — Python token parity & CSS variable integrity test
