## 2026-08-31T14:46:33Z
You are Worker 2 for Milestone 2: Zero-FOUC & Native REAPER Bridge.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\worker_m2_bridge

Read ORIGINAL_REQUEST.md at: c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
Read PROJECT.md at: c:\Users\smk28\Desktop\reals lab extension\PROJECT.md
Read AGENTS.md at: c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
Read Explorer 2's survey at: c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_cpp\handoff.md

MANDATORY INTEGRITY WARNING:
DO NOT CHEAT. All implementations must be genuine. DO NOT hardcode test results, create dummy/facade implementations, or circumvent the intended task. A teamwork_preview_auditor will independently verify your work. Integrity violations WILL be detected and your work WILL be rejected.

File ownership:
- You own: `ui-web/index.html`, `ui-web/app.js` (ThemeManager JS class & IPC), `extension/src/reaper_plugin.cpp`, `shell/win/WebViewHost.h`, `shell/win/WebViewHost.cpp`.

Your mission:
1. Run GitNexus `impact` before modifying any C++ or JS symbols.
2. In `ui-web/index.html`:
   - Add synchronous inline bootstrap `<script>` inside `<head>` reading `localStorage.getItem('reals_theme')` (defaulting to `'dark-studio'`) and setting `document.documentElement.setAttribute('data-theme', theme)` before any stylesheet or body renders.
3. In `ui-web/app.js`:
   - Implement `ThemeManager` singleton (`window.themeManager`) supporting:
     - `applyTheme(themeName, notifyNative)`
     - `getTheme()`
     - C++ message listener (`window.chrome.webview.addEventListener('message', ...)`) handling `"THEME_CHANGED:<name>"`
     - `localStorage` caching
     - Calling `window.chrome.webview.postMessage("THEME_CHANGED:<name>")` when `notifyNative = true`.
4. In `shell/win/WebViewHost.h` & `shell/win/WebViewHost.cpp`:
   - Add `void executeScript(const std::wstring& script, std::function<void(const std::string&)> onComplete = nullptr);`
   - Add `void postString(const std::string& str);`
   - Implement using `ICoreWebView2::ExecuteScript` and `ICoreWebView2::PostWebMessageAsString`.
5. In `extension/src/reaper_plugin.cpp`:
   - Add `#define REAPERAPI_WANT_GetExtState` and `#define REAPERAPI_WANT_SetExtState` before line 76.
   - In `createHostWindow()`, enhance `setWebMessageHandler` to intercept plain string messages starting with `"THEME_CHANGED:"`:
     - Extract theme name.
     - Call `SetExtState("REALSLAB", "theme", name.c_str(), true)`.
     - Call `reals::config::Config::instance().set("theme", name)`.
     - Do not pass `"THEME_CHANGED:"` to `g_bridge->handle()` (which expects JSON).
   - In `createHostWindow()` onReady callback / `showHostWindow()`:
     - Query `GetExtState("REALSLAB", "theme")`. If non-empty, use it; otherwise fallback to `Config::instance().getString("theme", "dark-studio")`.
     - Push theme to webview via `g_web->executeScript(L"window.themeManager && window.themeManager.applyTheme('" + utf8ToWide(theme) + L"', false);")`.
6. Run GitNexus `detect_changes`.
7. Build the project (`cmake --build --preset windows`) with zero warnings (`/W4`) and run tests (`ctest --preset windows`).
8. Write your handoff to `c:\Users\smk28\Desktop\reals lab extension\.agents\worker_m2_bridge\handoff.md` and send a completion message.
