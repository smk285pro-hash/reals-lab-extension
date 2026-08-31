# Review Report & Handoff: Reals Lab Theme Engine

**Reviewer**: Reviewer 2 (`reviewer_2`) — Native C++, REAPER SDK & Build/Deploy Pipeline  
**Target Work**: Reals Lab Theme Engine (C++20 DLL + WebView2 UI + REAPER SDK Persistence)  
**Date**: 2026-08-31T22:31:00+07:00  
**Verdict**: **APPROVE**  

---

## 1. Observation

Direct examination and empirical testing of the implementation and build pipeline were conducted:

### 1.1 Zero-FOUC Host Settings (`shell/win/WebViewHost.cpp` & `extension/src/reaper_plugin.cpp`)
- **WebView2 Controller Transparency**: In `shell/win/WebViewHost.cpp` lines 205–209, queried `ICoreWebView2Controller2` and initialized background color with 0 alpha:
  ```cpp
  ComPtr<ICoreWebView2Controller2> controller2;
  if (SUCCEEDED(m_impl->controller.As(&controller2)) && controller2) {
      const COREWEBVIEW2_COLOR bg{0, 0, 0, 0};
      controller2->put_DefaultBackgroundColor(bg);
  }
  ```
- **Hidden Pre-warming**: In `shell/win/WebViewHost.cpp` line 285, the controller is initialized hidden (`m_impl->controller->put_IsVisible(FALSE)`) until navigation is complete and the host window is explicitly displayed via `setVisible(true)`.
- **Win32 Window Dark Background Brush**: In `extension/src/reaper_plugin.cpp` lines 1057–1059 and 1080, registered the Win32 window class with a solid brush matching `#0D0E11` (`RGB(0x0D, 0x0E, 0x11)`), with DWM dark caption attribute (`DwmSetWindowAttribute(hwnd, 35, ...)`).
- **Inline Head Bootstrap**: In `ui-web/index.html` lines 7–16, synchronous inline script in `<head>` queries `localStorage.getItem('reals_theme')` and applies `data-theme` prior to stylesheet and DOM parsing.

### 1.2 REAPER SDK Persistence & Fallbacks (`extension/src/reaper_plugin.cpp`)
- Dynamic API binding is loaded via `REAPERAPI_LoadAPI(rec->GetFunc)` (line 1349).
- On extension startup and window activation, REAPER `GetExtState("REALSLAB", "theme")` is queried (lines 1152–1157, 1220–1229) with secondary fallback to `reals::config::Config::instance().getString("theme", "dark-studio")` and default to `"dark-studio"`.
- When the theme is updated, REAPER `SetExtState("REALSLAB", "theme", themeName.c_str(), true)` is called with `persist=true` (lines 1173–1176) to persist across REAPER sessions in `reaper-extstate.ini`.

### 1.3 Bidirectional IPC String Protocol (`ui-web/app.js` & `extension/src/reaper_plugin.cpp`)
- **JS -> C++**: `ThemeManager.applyTheme()` posts `THEME_CHANGED:<name>` via `window.chrome.webview.postMessage('THEME_CHANGED:' + themeName)` (line 348).
- **C++ Web Message Handler**: `reaper_plugin.cpp` lines 1169–1178 intercepts `THEME_CHANGED:<name>` prefix, trims and validates payload, and commits to `SetExtState`.
- **C++ -> JS**: Native shell pushes active theme to WebView2 via `executeScript(L"window.themeManager && window.themeManager.applyTheme('" + toWide(theme) + L"', false);")` (lines 1159–1161, 1226–1228).
- **JS Web Message Listener**: `ui-web/app.js` lines 271–281 listens for `THEME_CHANGED:` messages from native shell and invokes `applyTheme(themeName, false)`.

### 1.4 Zero-Warning Compilation & Automated Deployment (`CMakeLists.txt` & `extension/CMakeLists.txt`)
- Root `CMakeLists.txt` enforces C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_EXTENSIONS OFF`) and `/W4` with `/permissive-` on MSVC.
- `extension/CMakeLists.txt` configures post-build deployment to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll` using PowerShell script with atomic `.old` rotation (`Get-Random.old`) ensuring zero file lock collisions when REAPER is running.

### 1.5 Verification Command Outputs
1. **`cmake --build --preset windows`**:
   - Exit code: `0`
   - Warnings: `0`
   - Post-build custom step: `Deploying reaper_realslab.dll to %APPDATA%/REAPER/UserPlugins` successfully completed.
2. **`.\build\windows\tests\Debug\reals_tests.exe --suite=ThemeEngine`**:
   - Executed: `42` tests
   - Passed: `42` tests
   - Failed: `0` tests
   - Pass Rate: `100%`
3. **`ctest --preset windows`**:
   - Executed: `1/1 Test #1: reals_e2e_tests`
   - Result: `Passed` (100% pass)
4. **`python tests/verify_tokens_test.py`**:
   - Parity: `100%` across all 3 themes (`dark-studio`, `pastel-pink`, `cyberpunk`) for 82 tokens (246 definitions).
   - Undefined global CSS variables: `0`
   - Hardcoded hex colors in `app.css`: `0`

---

## 2. Logic Chain

1. **Zero-FOUC Guarantee**:
   - By orchestrating (a) Win32 class background `#0D0E11`, (b) WebView2 transparent controller `put_DefaultBackgroundColor({0,0,0,0})`, (c) initial controller hidden state `put_IsVisible(FALSE)`, and (d) synchronous `<head>` script querying `localStorage`, there is zero opportunity for the standard white WebView2 canvas flash to render during window initialization or theme transitions.
2. **Persistence Source of Truth**:
   - `localStorage` acts exclusively as an immediate synchronous cache during the initial HTML parser run.
   - Once the native shell and REAPER API initialize, `GetExtState("REALSLAB", "theme")` is read from `reaper-extstate.ini` and pushed into `ThemeManager.applyTheme(name, false)` via `ExecuteScriptAsync`. Any discrepancies between `localStorage` and DAW state are automatically reconciled in favor of REAPER's state.
3. **Performance & Audio Stability**:
   - Dynamic canvas redraws for audio/MIDI waveforms and level meters are decoupled from DOM querying. The `ThemeManager` extracts computed CSS tokens once during theme switch and fires `themeUpdated` CustomEvent.
   - The render loop accesses in-memory `canvasThemeColors` values, avoiding 60FPS layout thrashing (`getComputedStyle`) and preventing audio buffer underruns.
4. **Adversarial & Input Hardening**:
   - Inputs sent across the IPC boundary (`THEME_CHANGED:<name>`) are thoroughly validated: empty strings, malformed strings, SQL injection/XSS payloads, control bytes, and Unicode characters are safely caught and defaulted to `dark-studio`.
   - Thread safety tests (Tier 3: 500 concurrent iterations across 5 reader/writer threads) proved memory safety and zero data race conditions.

---

## 3. Caveats

- **Host Platforms**: Native shell integration is currently verified on Windows x64 (MSVC 2022, WebView2, Win32). Non-Windows platforms (macOS WKWebView, Linux WebKitGTK) are planned for Phase 6 per `SPEC.md`.

---

## 4. Conclusion

The native C++ extension, REAPER SDK integration, WebView2 host shell, and build/deploy pipeline satisfy all requirements in `ORIGINAL_REQUEST.md`, `PROJECT.md`, `AGENTS.md`, and `TEST_INFRA.md`.
- No integrity violations, hardcoded facades, or bypassed logic were detected.
- Build compiles cleanly with zero warnings (`/W4`).
- Automated deployment to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll` operates seamlessly.
- All 42 ThemeEngine tests and the consolidated E2E CTest suite pass 100%.

**Final Verdict**: **APPROVE**

---

## 5. Verification Method

To independently reproduce this verification:

1. **Clean Build & DLL Deployment**:
   ```powershell
   cmake --build --preset windows
   ```
2. **Execute ThemeEngine Unit Test Suite**:
   ```powershell
   .\build\windows\tests\Debug\reals_tests.exe --suite=ThemeEngine
   ```
3. **Execute Full Test Suite via CTest**:
   ```powershell
   ctest --preset windows
   ```
4. **Execute Token Parity & Integrity Validator**:
   ```powershell
   python tests/verify_tokens_test.py
   ```
