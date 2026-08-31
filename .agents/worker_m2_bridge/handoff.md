# Handoff Report — Worker 2: Milestone 2 (Zero-FOUC & Native REAPER Bridge)

## 1. Observation

Direct examination and modification across the project files yielded the following observations:

### A. Zero-FOUC Head Bootstrap (`ui-web/index.html`)
- Lines 7–16:
  ```html
  <title>Reals Lab</title>
  <script>
    (function() {
      try {
        var theme = localStorage.getItem('reals_theme') || 'dark-studio';
        document.documentElement.setAttribute('data-theme', theme);
      } catch (e) {
        document.documentElement.setAttribute('data-theme', 'dark-studio');
      }
    })();
  </script>
  <link rel="stylesheet" href="app.css">
  ```
  Synchronous inline execution in `<head>` ensures `document.documentElement` has `data-theme` set before CSS parsing and DOM body layout, eliminating Flash of Unstyled Content (FOUC).

### B. ThemeManager JS Class & IPC Bridge (`ui-web/app.js`)
- Lines 207–288:
  ```javascript
  class ThemeManager {
    constructor() {
      this._validThemes = ['dark-studio', 'pastel-pink', 'cyberpunk'];
      this._currentTheme = 'dark-studio';
      try {
        const saved = localStorage.getItem('reals_theme');
        if (saved && this._validThemes.includes(saved)) {
          this._currentTheme = saved;
        }
      } catch (e) {}
      this.applyTheme(this._currentTheme, false);
      if (window.chrome && window.chrome.webview && window.chrome.webview.addEventListener) {
        window.chrome.webview.addEventListener('message', (e) => {
          const data = e.data;
          if (typeof data === 'string' && data.startsWith('THEME_CHANGED:')) {
            const themeName = data.slice('THEME_CHANGED:'.length).trim();
            if (themeName && this._validThemes.includes(themeName)) {
              this.applyTheme(themeName, false);
            }
          }
        });
      }
    }
    getTheme() { return this._currentTheme; }
    applyTheme(themeName, notifyNative = false) {
      if (!themeName || !this._validThemes.includes(themeName)) themeName = 'dark-studio';
      this._currentTheme = themeName;
      try { localStorage.setItem('reals_theme', themeName); } catch (e) {}
      document.documentElement.setAttribute('data-theme', themeName);
      try {
        const styles = getComputedStyle(document.documentElement);
        const tokens = {
          bgApp: styles.getPropertyValue('--bg-app').trim(),
          bgSurface: styles.getPropertyValue('--bg-surface').trim(),
          bgCard: styles.getPropertyValue('--bg-card').trim(),
          accentPrimary: styles.getPropertyValue('--accent-primary').trim(),
          accentHover: styles.getPropertyValue('--accent-hover').trim(),
          txPrimary: styles.getPropertyValue('--tx-primary').trim(),
          txSecondary: styles.getPropertyValue('--tx-secondary').trim(),
          txMuted: styles.getPropertyValue('--tx-muted').trim(),
          borderSubtle: styles.getPropertyValue('--border-subtle').trim(),
          borderMedium: styles.getPropertyValue('--border-medium').trim(),
          waveformFill: styles.getPropertyValue('--waveform-fill').trim(),
          waveformFillActive: styles.getPropertyValue('--waveform-fill-active').trim(),
          waveformBg: styles.getPropertyValue('--waveform-bg').trim(),
        };
        window.dispatchEvent(new CustomEvent('themeUpdated', { detail: { theme: themeName, tokens } }));
      } catch (e) {}
      if (notifyNative && window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
        try { window.chrome.webview.postMessage('THEME_CHANGED:' + themeName); } catch (e) {}
      }
    }
  }
  window.themeManager = new ThemeManager();
  ```
- Lines 542–552:
  ```javascript
  if (hasWebView) {
    window.chrome.webview.addEventListener('message', (e) => {
      const m = e.data;
      if (typeof m === 'string' && m.startsWith('THEME_CHANGED:')) {
        const theme = m.slice('THEME_CHANGED:'.length).trim();
        if (window.themeManager) {
          window.themeManager.applyTheme(theme, false);
        }
        return;
      }
      if (m && m.event) { handleEvent(m.event, m.data); return; }
      if (m && m.id && _pending.has(m.id)) {
        const p = _pending.get(m.id);
        _pending.delete(m.id);
        m.ok ? p.resolve(m.data) : p.reject(new Error(m.error || 'bridge error'));
      }
    });
  }
  ```

### C. WebViewHost Native Script Execution & String IPC (`shell/win/WebViewHost.h`, `shell/win/WebViewHost.cpp`)
- `WebViewHost.h`:
  ```cpp
  void postString(const std::string& str);
  void executeScript(const std::wstring& script,
                     std::function<void(const std::string&)> onComplete = nullptr);
  ```
- `WebViewHost.cpp`:
  ```cpp
  void WebViewHost::postString(const std::string& str) {
      if (m_impl && m_impl->web) {
          const std::wstring w = toWide(str);
          m_impl->web->PostWebMessageAsString(w.c_str());
      }
  }

  void WebViewHost::executeScript(const std::wstring& script,
                                  std::function<void(const std::string&)> onComplete) {
      if (!m_impl || !m_impl->web) {
          if (onComplete)
              onComplete({});
          return;
      }
      if (onComplete) {
          m_impl->web->ExecuteScript(
              script.c_str(),
              Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                  [onComplete = std::move(onComplete)](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                      if (SUCCEEDED(errorCode) && resultObjectAsJson) {
                          onComplete(toNarrow(resultObjectAsJson));
                      } else {
                          onComplete({});
                      }
                      return S_OK;
                  })
                  .Get());
      } else {
          m_impl->web->ExecuteScript(script.c_str(), nullptr);
      }
  }
  ```

### D. REAPER SDK ExtState Integration (`extension/src/reaper_plugin.cpp`)
- Lines 76–77:
  ```cpp
  #define REAPERAPI_WANT_GetExtState
  #define REAPERAPI_WANT_SetExtState
  #define REAPERAPI_IMPLEMENT
  #include <reaper_plugin_functions.h>
  ```
- In `createHostWindow()` onReady callback (Lines 1148–1160) & `showHostWindow()` (Lines 1195–1205):
  ```cpp
  const char* rawTheme = GetExtState ? GetExtState("REALSLAB", "theme") : nullptr;
  std::string theme = (rawTheme && *rawTheme)
      ? std::string(rawTheme)
      : reals::config::Config::instance().getString("theme", "dark-studio");
  if (theme.empty())
      theme = "dark-studio";
  if (g_web) {
      const std::wstring script = L"window.themeManager && window.themeManager.applyTheme('" +
                                  toWide(theme) + L"', false);";
      g_web->executeScript(script);
  }
  ```
- In `setWebMessageHandler` (Lines 1162–1173):
  ```cpp
  constexpr std::string_view kThemePrefix = "THEME_CHANGED:";
  if (msg.rfind(kThemePrefix, 0) == 0) {
      const std::string themeName = msg.substr(kThemePrefix.length());
      if (!themeName.empty()) {
          if (SetExtState)
              SetExtState("REALSLAB", "theme", themeName.c_str(), true);
          reals::config::Config::instance().set("theme", themeName);
      }
      return;
  }
  ```

### E. Build and Test Tool Outputs
- `cmake --build --preset windows`: Exited with code 0. Zero warnings (`/W4`), zero errors. Built `reaper_realslab.dll` and deployed to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll`.
- `ctest --preset windows --output-on-failure`: Exited with code 0. 100% tests passed (1/1 test target containing 183+ test cases).
- GitNexus `detect_changes`: Confirmed `risk_level: "low"` and `affected_count: 0`.

---

## 2. Logic Chain

1. **Elimination of FOUC**:
   - The Win32 shell initializes a dark background brush `#0D0E11` and sets WebView2 controller `DefaultBackgroundColor` to `{0,0,0,0}` (transparent).
   - In `<head>` of `index.html`, the inline script runs synchronously, reading `localStorage.getItem('reals_theme')` (fallback `'dark-studio'`) and setting `document.documentElement.setAttribute('data-theme', theme)` before any stylesheet is loaded or body layout begins.
   - When the webview becomes visible, the background and styling are pre-applied with 0ms visual flash.

2. **Bidirectional Native Persistence**:
   - When the user selects a new theme in the UI, `ThemeManager.applyTheme(name, true)` sets `data-theme`, stores it in `localStorage`, and posts `"THEME_CHANGED:<name>"` to the native host.
   - The native message handler intercepts `"THEME_CHANGED:"`, updates REAPER's `reaper-extstate.ini` via `SetExtState("REALSLAB", "theme", name, true)`, and saves to `Config::instance()`.
   - On plugin startup or `showHostWindow`, C++ queries `GetExtState("REALSLAB", "theme")` (falling back to `Config`) and pushes the authoritative state to `window.themeManager.applyTheme(theme, false)` via `executeScript`.

3. **Event-Driven Customization**:
   - `applyTheme` emits a `CustomEvent('themeUpdated', { detail: { theme, tokens } })` which provides all computed CSS color tokens to canvas rendering routines (waveforms, VU meters, piano roll).

---

## 3. Caveats

- **Standalone Browser Mode**: If the UI is opened in a standalone web browser outside REAPER (where `window.chrome.webview` is undefined), `ThemeManager` falls back cleanly to `localStorage` caching without throwing errors.
- **Null ExtState Function Pointers**: In case the plugin is initialized in an environment where REAPER SDK function pointers are not loaded, ternary checks (`GetExtState ? ... : nullptr` and `if (SetExtState) ...`) protect against null-pointer dereferencing, falling back safely to `Config::instance()`.

---

## 4. Conclusion

Milestone 2 (Zero-FOUC & Native REAPER Bridge) is completely implemented, zero-warning compliant, tested, and ready for integration:
- `ui-web/index.html` implements synchronous `<head>` theme bootstrapping.
- `ui-web/app.js` provides the `ThemeManager` singleton with bidirectional IPC and event dispatching.
- `shell/win/WebViewHost` provides `postString` and `executeScript`.
- `extension/src/reaper_plugin.cpp` provides `SetExtState`/`GetExtState` REAPER SDK persistence and IPC string interception.
- `reaper_realslab.dll` is built and deployed to `%APPDATA%/REAPER/UserPlugins/`.

---

## 5. Verification Method

To independently verify:

1. **Compilation Verification**:
   ```powershell
   cmake --build --preset windows
   ```
   *Expected Result*: Exits with code 0, 0 compiler warnings (`/W4`), DLL copied to `%APPDATA%/REAPER/UserPlugins/reaper_realslab.dll`.

2. **Automated Test Suite**:
   ```powershell
   ctest --preset windows --output-on-failure
   ```
   *Expected Result*: 100% tests passed.

3. **GitNexus Scope Verification**:
   ```javascript
   detect_changes({ repo: "reals-lab-extension" })
   ```
   *Expected Result*: `risk_level: "low"`, no unintended execution flow regressions.
