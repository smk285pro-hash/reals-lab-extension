## 2026-08-31T15:21:02Z
You are a Worker implementing the Reals Lab Theme Engine.
Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\worker_theme_engine_1

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

Mandatory Input Files & References:
- c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
- c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
- c:\Users\smk28\Desktop\reals lab extension\PROJECT.md
- c:\Users\smk28\Desktop\reals lab extension\TEST_INFRA.md
- c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1\handoff.md
- c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_ui_survey_1\handoff.md
- c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_cpp_survey_1\handoff.md

Your Owned Files:
- ui-web/index.html
- ui-web/app.js
- ui-web/tokens.css
- ui-web/app.css
- extension/src/reaper_plugin.cpp
- shell/win/WebViewHost.cpp

Instructions:
1. Remember to use GitNexus for code intelligence & impact analysis before modifying symbols.
2. Implement the following in `ui-web/index.html` and `ui-web/app.js`:
   a. In `ui-web/index.html` under `#tab-general` (inside `#modalSettings`), add the Theme Picker group (`#optTheme`) with 3 chips (`dark-studio`, `pastel-pink`, `cyberpunk`).
   b. In `ui-web/app.js`, add i18n entries in `I18N.vi` and `I18N.en` for `settings.theme`, `theme.darkStudio`, `theme.pastelPink`, `theme.cyberpunk`.
   c. In `ui-web/app.js` `ThemeManager.applyTheme()`, clean up CSS variable name extraction to match `tokens.css` (`--text-primary`, `--border-default`, `--accent`, etc.) and ensure any conflicting inline `--accent` styling is cleanly removed so theme tokens take effect.
   d. In `ui-web/app.js`, initialize an in-memory `canvasThemeColors` object and register a `themeUpdated` listener to update it and call `drawWaveform()` / `drawMeterSmoothed()` without running `getComputedStyle()` inside the 60FPS render loop.
   e. In `ui-web/app.js` `drawWaveform()` (both Audio and MIDI modes) and `drawMeterSmoothed()`, use the dynamic `canvasThemeColors` values instead of hardcoded hex/rgba values.
   f. In `ui-web/app.js` `renderSettingsModal()`, wire up `#optTheme` button chips: highlight the active theme and bind click handlers to call `window.themeManager.applyTheme(val, true)` and re-render.
3. In C++ (`extension/src/reaper_plugin.cpp`, `shell/win/WebViewHost.cpp`), verify:
   - Zero-FOUC initialization: transparent background (`put_DefaultBackgroundColor`), hidden prewarming (`put_IsVisible(FALSE)`), dark window brush `#0D0E11`.
   - REAPER `GetExtState("REALSLAB", "theme")` / `SetExtState("REALSLAB", "theme", name, true)`.
   - IPC string protocol `THEME_CHANGED:<name>`.
4. CRITICAL: Build with `cmake --build --preset windows` to compile all targets with zero warnings (`/W4` MSVC) and automatically deploy `reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll` BEFORE running tests, allowing manual verification in REAPER in parallel.
5. Verify the implementation:
   - Run `python tests/verify_tokens_test.py` to confirm 100% token parity (246 definitions).
   - Run `.\build\windows\tests\Debug\reals_tests.exe --suite=ThemeEngine` to confirm all 42 tests pass.
   - Run `ctest --preset windows` to confirm 100% test pass rate across all suites.
6. Write a complete handoff report to `c:\Users\smk28\Desktop\reals lab extension\.agents\worker_theme_engine_1\handoff.md` with:
   - Changes made
   - Build outputs and verification commands executed
   - Test results
7. Send a message to parent when completed with the path to your handoff file.
