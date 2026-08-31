# Original User Request

## 2026-08-31T15:16:06Z

Build a production-grade, zero-FOUC Theme Engine for the Reals Lab REAPER Extension (C++ DLL + WebView2) supporting instant theme switching (Dark Studio, Pastel Pink, Cyberpunk Neon), REAPER SetExtState/GetExtState persistence, CSS Custom Property design tokens, and live canvas waveform synchronization.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

## Requirements

### R1. CSS Custom Properties & Semantic Design Tokens
- Refactor existing UI styles into semantic design tokens in `ui-web/tokens.css` / `ui-web/app.css` using CSS Custom Properties (`:root` and `html[data-theme="..."]`).
- Implement 3 complete theme palettes:
  1. `dark-studio` (Default studio dark theme)
  2. `pastel-pink` (Light pastel pink / cutecore)
  3. `cyberpunk` (High-contrast neon cyberpunk)
- Every theme palette must override 100% of defined tokens with zero missing variables and zero hardcoded colors.
- All vector SVG icons must dynamically adapt using `fill="currentColor"`, `stroke="currentColor"`, or `var(--accent-primary)`.

### R2. Bidirectional Native Bridge & REAPER Persistence
- Implement native persistence using REAPER SDK `GetFunc` -> `SetExtState(section, key, value, persist=true)` and `GetExtState(section, key)`.
- Use the exact plain string IPC protocol in both directions:
  - JS to C++: `window.chrome.webview.postMessage("THEME_CHANGED:<name>")`
  - C++ to JS: `THEME_CHANGED:<name>` or direct `ExecuteScriptAsync("window.themeManager && window.themeManager.applyTheme('<name>', false);")`
- Host `ICoreWebView2Controller2::put_DefaultBackgroundColor` transparent and `put_IsVisible(FALSE)` until DOM is ready to eliminate white flash / FOUC.
- `localStorage` is used strictly as a fast inline bootstrap cache in `<head>`, while `GetExtState` is the absolute source of truth pushed on startup.

### R3. Dynamic Waveform Canvas & UI Theme Picker
- `ThemeManager` dispatches a `themeUpdated` CustomEvent with computed theme colors (`--waveform-fill`, `--waveform-fill-active`, `--waveform-bg`).
- Live Waveform Canvas and Piano Roll immediately redraw using the new theme colors without reloading the page or hitching audio playback.
- Theme Picker UI in settings/topbar allowing instant selection with active theme indicator.

### R4. Continuous Build, Early Deployment & Parallel Verification
- **CRITICAL**: The implementer MUST run `cmake --build --preset windows` and deploy `reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins/` as soon as the core implementation is ready, BEFORE running automated test suites, allowing the user to manually verify in REAPER in parallel.
- Maintain zero warnings (`/W4` on MSVC) and 100% test pass rate on `ctest --preset windows`.

## Acceptance Criteria

### Visual & Runtime Verification
- [ ] Extension launches with zero white flash or FOUC in dark and light themes.
- [ ] Theme switching occurs in <16ms (single frame) without page reloads or DOM destruction.
- [ ] Closing and reopening REAPER preserves the selected theme across sessions via `GetExtState`.
- [ ] Waveform canvas and piano roll update colors immediately upon theme change.
- [ ] Build succeeds with zero warnings and zero errors via `cmake --build --preset windows`.
- [ ] Automated e2e tests (`ctest --preset windows`) pass 100%.
