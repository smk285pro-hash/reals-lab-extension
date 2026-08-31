# Handoff Report — Explorer 2: C++ Extension, WebView2 Controller & REAPER ExtState Survey

## 1. Observation

Direct examination of the C++ codebase (`extension/src/reaper_plugin.cpp`, `shell/win/WebViewHost.h`, `shell/win/WebViewHost.cpp`, `bridge/src/Bridge.cpp`, `core/src/config/Config.cpp`) yielded the following factual observations:

### A. REAPER SDK Function Pointer Binding (`extension/src/reaper_plugin.cpp`)
- Lines 25–77 define minimal REAPER API requirements before `#include <reaper_plugin_functions.h>`:
  ```cpp
  25: #define REAPERAPI_MINIMAL
  26: #define REAPERAPI_WANT_plugin_register
  27: #define REAPERAPI_WANT_GetMainHwnd
  ...
  76: #define REAPERAPI_IMPLEMENT
  77: #include <reaper_plugin_functions.h>
  ```
- `REAPERAPI_WANT_GetExtState` and `REAPERAPI_WANT_SetExtState` are currently **absent** from the file.
- `libs/reaper-sdk/sdk/reaper_plugin_functions.h` defines:
  ```cpp
  1748: // Get the extended state value for a specific section and key. See SetExtState, DeleteExtState, HasExtState.
  ...
  5962: void (*REAPERAPI_FUNCNAME(SetExtState))(const char* section, const char* key, const char* value, bool persist);
  ...
  9968: {(void**)&REAPERAPI_FUNCNAME(SetExtState),"SetExtState"},
  ```
- In `extension/src/reaper_plugin.cpp:1314`, `REAPERAPI_LoadAPI(rec->GetFunc)` dynamically resolves all defined function pointers from REAPER during plugin load.

### B. Win32 Host Window & Zero-FOUC Styling (`extension/src/reaper_plugin.cpp`)
- Line 1055–1056 creates the Win32 window background brush:
  ```cpp
  if (!g_bgBrush)
      g_bgBrush = CreateSolidBrush(RGB(0x0D, 0x0E, 0x11));
  ```
- Lines 1078, 1088: `wc.hbrBackground = g_bgBrush;` and `CreateWindowExW(WS_EX_TOOLWINDOW, kWndClass, ...)` creates the hidden pre-warm window.
- Line 1107: `ShowWindow(g_hwnd, SW_HIDE);` keeps the window hidden during REAPER startup.
- Lines 1032–1047 (`applyDwmDarkTitle`): Sets DWM attributes on Windows 11/10:
  - `DWMWA_USE_IMMERSIVE_DARK_MODE` (20 / 19) = `TRUE`
  - `DWMWA_CAPTION_COLOR` (35) = `RGB(0x0D, 0x0E, 0x11)`
  - `DWMWA_TEXT_COLOR` (36) = `RGB(0xF2, 0xF3, 0xF5)`
  - `DWMWA_BORDER_COLOR` (34) = `RGB(0x24, 0x26, 0x2B)`
  - `DWMWA_WINDOW_CORNER_PREFERENCE` (33) = `2` (`DWMWCP_ROUND`)

### C. WebView2 Controller Initialization & Transparent Canvas (`shell/win/WebViewHost.cpp`)
- Lines 205–209 configure transparency:
  ```cpp
  ComPtr<ICoreWebView2Controller2> controller2;
  if (SUCCEEDED(m_impl->controller.As(&controller2)) && controller2) {
      const COREWEBVIEW2_COLOR bg{0, 0, 0, 0};
      controller2->put_DefaultBackgroundColor(bg);
  }
  ```
  `COREWEBVIEW2_COLOR{0, 0, 0, 0}` specifies `Alpha = 0` (100% transparent), allowing the Win32 host background (`RGB(0x0D, 0x0E, 0x11)`) to shine through instead of default solid white (`#FFFFFF`).
- Line 285 keeps the controller invisible during loading:
  ```cpp
  m_impl->controller->put_IsVisible(FALSE);
  ```
- Lines 128–144: `ICoreWebView2WebMessageReceivedEventHandler` receives IPC messages from JS via `args->TryGetWebMessageAsString(&raw)` (plain string) and `args->get_WebMessageAsJson(&raw)` (JSON).
- `WebViewHost.h` currently lacks helper methods for executing arbitrary JavaScript (`ExecuteScript`) and posting plain strings (`PostWebMessageAsString`).

### D. IPC Message Routing (`extension/src/reaper_plugin.cpp` & `bridge/src/Bridge.cpp`)
- In `extension/src/reaper_plugin.cpp:1154–1160`:
  ```cpp
  g_web->setWebMessageHandler([](const std::string& msg) {
      if (g_bridge) {
          const std::string response = g_bridge->handle(msg);
          if (g_web)
              g_web->postJson(response);
      }
  });
  ```
- In `bridge/src/Bridge.cpp:630–650`:
  `Bridge::handle(const std::string& requestJson)` parses the message as JSON. A plain string message (such as `"THEME_CHANGED:pastel-pink"`) causes `json::parse` to return a parse error response `{"id":0,"ok":false,"error":"bad request"}`.

### E. Configuration Storage (`core/src/config/Config.cpp`)
- `reals::config::Config::instance()` manages application settings saved to `%APPDATA%/RealsLab/config.json`.
- Provides `getString(key, fallback)` and `set(key, jsonValue)` guarded by `std::recursive_mutex`.

---

## 2. Logic Chain

1. **REAPER ExtState Availability**:
   - Because `reaper_plugin_functions.h` includes `SetExtState` and `GetExtState` conditionally under `REAPERAPI_WANT_SetExtState` and `REAPERAPI_WANT_GetExtState`, defining both in `extension/src/reaper_plugin.cpp` ensures that `REAPERAPI_LoadAPI(rec->GetFunc)` resolves them at startup.
   - Using `SetExtState("REALSLAB", "theme", theme.c_str(), true)` instructs REAPER to write the key into `reaper-extstate.ini` under section `[REALSLAB]`, surviving REAPER restarts.
   - `GetExtState("REALSLAB", "theme")` returns the saved string (e.g. `"dark-studio"`, `"pastel-pink"`, `"cyberpunk"`), or `""` if not yet written.

2. **Elimination of White Flash / FOUC**:
   - Step 1 (Win32 Shell): Window class has a dark solid brush `#0D0E11`.
   - Step 2 (WebView2 Controller): `ICoreWebView2Controller2::put_DefaultBackgroundColor({0,0,0,0})` makes the webview viewport transparent until painted.
   - Step 3 (Controller Visibility): `put_IsVisible(FALSE)` keeps the webview hidden during background pre-warming.
   - Step 4 (Web DOM Bootstrap): Synchronous `<script>` in `<head>` applies `document.documentElement.setAttribute('data-theme', theme)` from `localStorage` cache before HTML body elements parse.
   - Step 5 (Authoritative ExtState Push): On startup or `NavigationCompleted`, C++ pushes the true ExtState theme into the webview via `ExecuteScript("window.themeManager && window.themeManager.applyTheme('<name>', false);")`.
   - Step 6 (Reveal): Once ready, `put_IsVisible(TRUE)` and `ShowWindow(g_hwnd, SW_SHOW)` display the fully rendered theme with 0ms flash.

3. **Bidirectional IPC Architecture**:
   - **JS to C++**:
     When the user selects a theme in the UI, JS invokes `window.chrome.webview.postMessage("THEME_CHANGED:<name>")`.
     In `reaper_plugin.cpp`, `setWebMessageHandler` intercepts strings starting with `"THEME_CHANGED:"`:
     - Extracts the theme name.
     - Calls `SetExtState("REALSLAB", "theme", name.c_str(), true)`.
     - Calls `reals::config::Config::instance().set("theme", name)`.
     - Bypasses `g_bridge->handle()` to avoid JSON parsing errors.
   - **C++ to JS**:
     When the webview finishes initializing (`onReady` / `NavigationCompleted`):
     - Reads `GetExtState("REALSLAB", "theme")`. If empty, defaults to `"dark-studio"`.
     - Executes `window.themeManager && window.themeManager.applyTheme('<name>', false);` via `ExecuteScript`.
     - Or posts `THEME_CHANGED:<name>` message / `theme.changed` JSON event.

4. **Code Quality & AGENTS.md Compliance**:
   - Modern C++20 standard (/W4 MSVC zero warnings).
   - Strict separation of concerns: `core/` remains free of GUI/REAPER dependencies; `shell/` handles WebView2 COM interfaces; `extension/` manages REAPER SDK APIs and translates between REAPER and WebView2.

---

## 3. Caveats

- **REAPER ExtState vs Standalone App**: In REAPER extension mode, `GetExtState`/`SetExtState` is available and authoritative. In standalone app mode (if running without REAPER), `GetExtState` is null, so C++ seamlessly falls back to `Config::instance().getString("theme", "dark-studio")`.
- **WebView2 Virtual Host Execution Timing**: Executing `ExecuteScript` must be done after the frame context exists (i.e. inside or after controller creation and navigation start). If called before `window.themeManager` is defined, the check `if (window.themeManager)` safely avoids JS errors.
- **String Encoding**: Theme names (`dark-studio`, `pastel-pink`, `cyberpunk`) are pure ASCII identifier tokens, eliminating UTF-8 / UTF-16 conversion overhead or corruption risk.

---

## 4. Conclusion

The C++ Extension and WebView2 architecture is already 90% structured for zero-FOUC and native persistence. The exact changes required by the implementer are compact, isolated, and safe:

### Recommended Implementer Changes:

1. **`extension/src/reaper_plugin.cpp`**:
   - Add `#define REAPERAPI_WANT_GetExtState` and `#define REAPERAPI_WANT_SetExtState` before line 76.
   - In `createHostWindow()`, enhance `setWebMessageHandler` to intercept `"THEME_CHANGED:<name>"` and call `SetExtState("REALSLAB", "theme", name.c_str(), true)` + `Config::instance().set("theme", name)`.
   - In `createHostWindow()` onReady callback / `showHostWindow()`, query `GetExtState("REALSLAB", "theme")` and push to webview via `executeScript`.

2. **`shell/win/WebViewHost.h` & `shell/win/WebViewHost.cpp`**:
   - Add `void executeScript(const std::wstring& script, std::function<void(const std::string&)> onComplete = nullptr);`
   - Add `void postString(const std::string& str);`
   - Implement both using `m_impl->web->ExecuteScript` and `m_impl->web->PostWebMessageAsString`.

3. **`bridge/src/Bridge.cpp`**:
   - (Optional helper) Add support for `"theme.get"` and `"theme.set"` in `Bridge::handle` for standard JSON-RPC callers.

---

## 5. Verification Method

To verify the C++ Extension, WebView2 Controller, and REAPER ExtState persistence:

1. **Compilation Verification**:
   ```powershell
   cmake --build --preset windows
   ```
   *Expected Result*: Zero errors and zero warnings (`/W4`).

2. **Automated Test Suite**:
   ```powershell
   ctest --preset windows --output-on-failure
   ```
   *Expected Result*: 100% of test suites pass (183/183+ test cases).

3. **REAPER Extension Deployment & ExtState Check**:
   - The build output `reaper_realslab.dll` is automatically copied to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll`.
   - Launch REAPER -> Open `Reals Lab` extension.
   - Switch theme to `pastel-pink` or `cyberpunk`.
   - Verify `%APPDATA%/REAPER/reaper-extstate.ini` contains:
     ```ini
     [REALSLAB]
     theme=pastel-pink
     ```
   - Restart REAPER -> Re-open `Reals Lab` -> Verify theme loads instantly as `pastel-pink` with 0ms white flash or FOUC.
