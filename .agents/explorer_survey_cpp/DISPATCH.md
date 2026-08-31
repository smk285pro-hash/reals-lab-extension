## 2026-08-31T14:26:52Z
Survey the C++ Extension, WebView2 Controller initialization, and REAPER ExtState persistence:
1. Examine `src/extension/`, `src/app/`, `src/core/`, and REAPER integration files.
2. Investigate how WebView2 is initialized, created, and managed (HWND, ICoreWebView2Controller2, ICoreWebView2).
3. Investigate `put_DefaultBackgroundColor` (transparent) and `put_IsVisible(FALSE)` until DOM ready / navigation completed to eliminate white flash/FOUC.
4. Investigate REAPER SDK `GetFunc` -> `SetExtState(section, key, value, persist=true)` and `GetExtState(section, key)`.
5. Investigate IPC message handling:
   - JS to C++: `window.chrome.webview.postMessage("THEME_CHANGED:<name>")` -> C++ parses and saves via `SetExtState`.
   - C++ to JS: On startup / ExtState loaded, push theme to webview via `ExecuteScriptAsync("window.themeManager && window.themeManager.applyTheme('<name>', false);")` or IPC message.
6. Verify compliance with AGENTS.md (C++20, /W4, separation of concerns).
7. Use GitNexus MCP tools (query, context, impact) as required.
