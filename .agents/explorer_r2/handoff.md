# R2 Audit Report: WebView2 Shell & C++ ↔ JS Bridge Protocol

**Date:** 2026-08-25  
**Auditor:** Explorer R2 (WebView2 Shell & Bridge Specialist)  
**Target Subsystems:**
- `shell/win/WebViewHost.h`, `shell/win/WebViewHost.cpp`
- `extension/src/reaper_plugin.cpp`
- `bridge/include/reals/bridge/Bridge.h`, `bridge/src/Bridge.cpp`
- `ui-web/app.js`, `ui-web/index.html`, `ui-web/app.css`
- `core/src/lab/LabApi.cpp`, `core/src/browser/BrowserModel.cpp`, `core/src/platform/Path.cpp`
- `assets/i18n/strings_vi.json`, `assets/i18n/strings_en.json`

---

## 1. Executive Summary

A comprehensive, line-by-line audit of the WebView2 hosting infrastructure, REAPER extension lifecycle, JSON-RPC Bridge dispatcher, and Web UI frontend communication was conducted.

### Issue Breakdown by Severity:
- 🔴 **CRITICAL (4)**: Unhandled JSON parse exception crashing REAPER process on malformed request; `WebViewHost` memory leak & missing `ICoreWebView2Controller::Close()` leaving orphaned `msedgewebview2.exe` processes; Dangling `this` pointer use-after-free in async COM callbacks; Detached background worker threads crashing upon DLL unloading / extension exit.
- 🟠 **HIGH (5)**: Non-atomic REAPER Undo transaction for batch stem imports (`reaper.insertMany`); Missing Bridge command handlers for `lab.tempo` and `lab.midi`; Ignored return code from REAPER `InsertMedia`; Fixed 600-byte stack buffer truncation in `json_event` creating invalid JSON; Infinite polling loop in `runLabJob` without timeout cap.
- 🟡 **MEDIUM (6)**: Missing `put_IsVisible(FALSE)` on window hide leading to unnecessary background GPU/CPU rendering; Missing `WM_DPICHANGED` handling for Per-Monitor DPI scaling; Broken file list tag dots (`tagCache`) and favorite stars (`favSet`) in frontend; Broken sort dropdown in Browser UI; i18n key mismatch between C++ backend and frontend toast strings; Missing `fs.addRoot` / `fs.removeRoot` bridge APIs.
- 🔵 **LOW / REFACTOR (3)**: Unclosed COM apartment & uncleaned HWND/window class on plugin unload; ANSI vs UTF-8 path conversions in `std::filesystem::path` on Windows; Missing keyboard accelerator coordination (`add_AcceleratorKeyPressed`).

---

## 2. Component 1: Observation & Detailed Audit Findings

### Section A: WebView2 Shell & COM Lifecycle (Scope 1)

#### 🔴 CRITICAL-1: `WebViewHost` Leaks `m_impl` Heap Allocation & Fails to Explicitly Close WebView2 Controller
- **File & Line:** `shell/win/WebViewHost.h:33`, `shell/win/WebViewHost.cpp:80`
- **Severity:** 🔴 CRITICAL
- **Observation:**
  In `WebViewHost.h`:
  ```cpp
  struct Impl;
  Impl* m_impl = nullptr;
  ```
  In `WebViewHost.cpp`:
  ```cpp
  WebViewHost::~WebViewHost() = default;
  ```
  `m_impl` is allocated with `new Impl()` in `create()` and `setWebMessageHandler()`. Because `m_impl` is a raw pointer, `delete m_impl;` is never invoked by the default destructor.
  Furthermore, `ICoreWebView2Controller::Close()` is never called. According to Microsoft WebView2 SDK specifications, failing to call `controller->Close()` leaves `msedgewebview2.exe` subprocesses running as orphaned background processes and allows COM callbacks to fire into dead HWNDs.
- **Reproduction Scenario:**
  1. Open the Reals Lab window in REAPER.
  2. Close the window and unload/reload the plugin.
  3. Open Windows Task Manager -> Notice `msedgewebview2.exe` processes lingering and leaking memory.
- **Proposed Fix:**
  Use `std::unique_ptr<Impl>` and implement a proper destructor that explicitly removes event tokens, calls `Close()`, and resets COM pointers:
  ```cpp
  // shell/win/WebViewHost.h
  #include <memory>
  ...
  std::unique_ptr<Impl> m_impl;
  ```
  ```cpp
  // shell/win/WebViewHost.cpp
  WebViewHost::~WebViewHost() {
      if (m_impl) {
          if (m_impl->web) {
              if (m_impl->messageToken.value != 0)
                  m_impl->web->remove_WebMessageReceived(m_impl->messageToken);
              if (m_impl->navToken.value != 0)
                  m_impl->web->remove_NavigationCompleted(m_impl->navToken);
          }
          if (m_impl->controller) {
              m_impl->controller->Close();
              m_impl->controller.Reset();
          }
          m_impl->web.Reset();
          m_impl->environment.Reset();
      }
  }
  ```

---

#### 🔴 CRITICAL-2: Dangling `this` Pointer & Use-After-Free in Async COM Completion Handlers
- **File & Line:** `shell/win/WebViewHost.cpp:103, 120, 163, 62`
- **Severity:** 🔴 CRITICAL
- **Observation:**
  `CreateCoreWebView2EnvironmentWithOptions` and `CreateCoreWebView2Controller` are asynchronous COM operations that complete via REAPER's message loop. The completion callbacks capture raw `[this]` (`WebViewHost*` or `Impl*`):
  ```cpp
  Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [this, sourceFolder, onReady](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
          ...
          m_impl->environment = env;
  ```
  If the user quickly opens and closes the Reals Lab window or unloads the plugin while initialization is pending, `g_web.reset()` destroys `WebViewHost`. When the COM callback executes on the message pump, `this` points to deallocated heap memory -> Instant Access Violation (0xC0000005).
- **Reproduction Scenario:**
  1. Trigger `REALSLAB_SHOW_WINDOW` to open the window.
  2. Immediately close the window / press shortcut within 50ms before WebView2 finishes loading.
  3. REAPER crashes with `EXCEPTION_ACCESS_VIOLATION`.
- **Proposed Fix:**
  Introduce `std::shared_ptr<Impl>` and capture `std::weak_ptr<Impl>` in all async COM callbacks:
  ```cpp
  std::weak_ptr<Impl> weakImpl = m_impl;
  Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
      [weakImpl, sourceFolder, onReady](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
          auto impl = weakImpl.lock();
          if (!impl)
              return S_OK; // Host was destroyed, abort cleanly
          ...
      });
  ```

---

#### 🟡 MEDIUM-1: Missing `ICoreWebView2Controller::put_IsVisible` on Window Hide/Show
- **File & Line:** `shell/win/WebViewHost.cpp:206-211`, `extension/src/reaper_plugin.cpp:116-120, 274-279`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  When the host window is hidden (`ShowWindow(g_hwnd, SW_HIDE)` or `g_actions.hideWindow()`), `put_IsVisible(FALSE)` is never called on `controller`. WebView2 continues rendering and composition in the background, needlessly consuming GPU, CPU, and laptop battery while invisible.
- **Reproduction Scenario:**
  Hide the Reals Lab window in REAPER. Check GPU/CPU utilization of `msedgewebview2.exe` -> It continues running animation and composition timers.
- **Proposed Fix:**
  Add `setVisible(bool visible)` to `WebViewHost`:
  ```cpp
  void WebViewHost::setVisible(bool visible) {
      if (m_impl && m_impl->controller) {
          m_impl->controller->put_IsVisible(visible ? TRUE : FALSE);
      }
  }
  ```
  Invoke `g_web->setVisible(false)` in `hideWindow()` and `g_web->setVisible(true)` in `toggleWindow()`.

---

#### 🟡 MEDIUM-2: Missing `WM_DPICHANGED` Handling for Per-Monitor DPI v2
- **File & Line:** `extension/src/reaper_plugin.cpp:168-183`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  `hostWndProc` only handles `WM_SIZE` and `WM_CLOSE`. When moving the window between monitors with different DPI scaling (e.g. 100% 1080p to 175% 4K), Windows sends `WM_DPICHANGED` with a suggested `RECT*` in `lParam`. Currently, this falls through to `DefWindowProcW`, causing blurred/misaligned rendering.
- **Reproduction Scenario:**
  Drag the Reals Lab window from a 1080p standard DPI display to a 4K high DPI display -> Window elements appear blurred or with incorrect bounds until manually resized.
- **Proposed Fix:**
  Handle `WM_DPICHANGED` in `hostWndProc`:
  ```cpp
  case WM_DPICHANGED: {
      const RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam);
      SetWindowPos(h, nullptr,
                   prcNewWindow->left, prcNewWindow->top,
                   prcNewWindow->right - prcNewWindow->left,
                   prcNewWindow->bottom - prcNewWindow->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
      return 0;
  }
  ```

---

### Section B: REAPER API Safety & Threading (Scope 2)

#### 🟠 HIGH-1: Non-Atomic REAPER Undo History on Batch Stem Import (`reaper.insertMany`)
- **File & Line:** `bridge/src/Bridge.cpp:325-329`, `extension/src/reaper_plugin.cpp:85-101`
- **Severity:** 🟠 HIGH
- **Observation:**
  In `ui-web/app.js`, clicking "Chèn tất cả vào project" calls `bridge('reaper.insertMany', { paths })`.
  In `Bridge.cpp`:
  ```cpp
  } else if (cmd == "reaper.insertMany") {
      if (args.contains("paths") && args["paths"].is_array() && m_actions)
          for (const auto& p : args["paths"])
              m_actions->insertMedia(narrowPath(p.get<std::string>()));
      res["ok"] = true;
  }
  ```
  In `reaper_plugin.cpp`, `ExtHostActions::insertMedia` opens and closes `Undo_BeginBlock()` / `Undo_EndBlock()` on *every single file*.
  Inserting 4 stem tracks results in 4 distinct undo entries. If the user presses `Ctrl+Z` in REAPER, only 1 stem track is undone, leaving 3 stems on the timeline in an inconsistent state.
- **Reproduction Scenario:**
  1. Separate a song into 4 stems in Audio Lab.
  2. Click "Chèn tất cả vào project".
  3. Press `Ctrl+Z` in REAPER -> Only the "other" stem is undone; "vocal", "drum", "bass" remain on tracks.
- **Proposed Fix:**
  Add `insertMediaMany` to `IHostActions` and wrap the entire loop in a single Undo block:
  ```cpp
  // bridge/include/reals/bridge/Bridge.h
  virtual void insertMediaMany(const std::vector<std::string>& paths) = 0;
  ```
  ```cpp
  // extension/src/reaper_plugin.cpp
  void insertMediaMany(const std::vector<std::string>& paths) override {
      if (paths.empty()) return;
      Undo_BeginBlock();
      int insertedCount = 0;
      for (const auto& path : paths) {
          if (isMediaFile(path) && InsertMedia(path.c_str(), 3) > 0) {
              insertedCount++;
          }
      }
      Undo_EndBlock(insertedCount > 0 ? "Reals Lab: Insert media (batch)" : nullptr, 0);
      if (insertedCount > 0)
          json_event("toast", reals::i18n::tr("browser.toast.inserted"));
  }
  ```

---

#### 🟠 HIGH-2: Silent Failure & Ignored Return Value from REAPER `InsertMedia`
- **File & Line:** `extension/src/reaper_plugin.cpp:95-97`
- **Severity:** 🟠 HIGH
- **Observation:**
  ```cpp
  Undo_BeginBlock();
  InsertMedia(path.c_str(), 3); // Return value (1 on success, 0 on failure) is completely ignored!
  Undo_EndBlock("Reals Lab: Insert media", 0);
  json_event("toast", reals::i18n::tr("browser.toast.inserted"));
  ```
  If `InsertMedia` fails (due to locked file, missing codec, corrupt header, or invalid path), it still records a useless undo block and tells the user "Đã chèn vào project" (toast inserted).
- **Reproduction Scenario:**
  Attempt to double-click or insert a 0-byte or corrupted `.wav` file -> UI says "Đã chèn vào project" even though REAPER did nothing.
- **Proposed Fix:**
  ```cpp
  Undo_BeginBlock();
  const int ret = InsertMedia(path.c_str(), 3);
  if (ret > 0) {
      Undo_EndBlock("Reals Lab: Insert media", 0);
      json_event("toast", reals::i18n::tr("browser.toast.inserted"));
  } else {
      Undo_EndBlock(nullptr, 0); // Cancel undo block
      json_event("toast", reals::i18n::tr("browser.toast.insertFail"));
      LOG_ERROR(kTag, "InsertMedia failed for path: %s", path.c_str());
  }
  ```

---

#### 🟠 HIGH-3: Fixed 600-Byte Stack Buffer Truncation in `ExtHostActions::json_event` Creating Malformed JSON
- **File & Line:** `extension/src/reaper_plugin.cpp:145-148`
- **Severity:** 🟠 HIGH
- **Observation:**
  ```cpp
  static void json_event(const char* event, const std::string& text) {
      if (!g_web)
          return;
      char buf[600];
      std::snprintf(buf, sizeof(buf), "{\"event\":\"%s\",\"data\":{\"text\":\"%s\"}}", event,
                    escapeJson(text).c_str());
      g_web->postJson(buf);
  }
  ```
  If `text` is a long file path or multi-byte UTF-8 string exceeding ~500 bytes, `std::snprintf` truncates the buffer, producing invalid JSON syntax (e.g. `{"event":"toast","data":{"text":"C:\Long...`). When passed to `postJson`, WebView2 drops the malformed message.
- **Reproduction Scenario:**
  Insert a file situated in a deeply nested path (>450 chars) or with Unicode Vietnamese characters -> The toast event is never received by JS because JSON parsing fails in WebView2.
- **Proposed Fix:**
  Use `nlohmann::json` to serialize events safely without any fixed buffer limit:
  ```cpp
  static void json_event(const char* event, const std::string& text) {
      if (!g_web) return;
      nlohmann::json j;
      j["event"] = event;
      j["data"] = {{"text", text}};
      g_web->postJson(j.dump());
  }
  ```

---

### Section C: Bridge Protocol & Command Mapping (Scope 3)

#### 🔴 CRITICAL-3: Unhandled JSON Parse Exception Outside `try/catch` Crashing REAPER on Malformed Bridge Message
- **File & Line:** `bridge/src/Bridge.cpp:175-179`
- **Severity:** 🔴 CRITICAL
- **Observation:**
  ```cpp
  std::string Bridge::handle(const std::string& requestJson) {
      auto& eng = audio::Engine::instance();
      auto& cfg = config::Config::instance();
      auto& model = m_impl->model;
      json req = json::parse(requestJson, nullptr, false);
      json res;
      res["id"] = req.contains("id") ? req["id"] : json(0); // <--- THROWS IF DISCARDED!

      if (req.is_discarded() || !req.contains("cmd")) {
          res["ok"] = false;
          res["error"] = "bad request";
          return res.dump();
      }
      const std::string cmd = req.value("cmd", "");
      const json& args = req.contains("args") ? req["args"] : json::object();

      try {
          if (cmd == "app.info") {
          ...
  ```
  When `requestJson` is invalid (e.g. incomplete payload, syntax error), `json::parse(..., false)` returns a discarded JSON value.
  Evaluating `req.contains("id")` on a discarded value throws `nlohmann::json::type_error: [json.exception.type_error.304] cannot use contains() with discarded`.
  Because lines 177-179 are *outside* the `try { ... }` block (which only starts at line 187), this unhandled exception leads to immediate `std::terminate()`, taking down the entire REAPER DAW.
- **Reproduction Scenario:**
  From JS console / devtools: `window.chrome.webview.postMessage("invalid json {[[");` -> Immediate crash of REAPER.
- **Proposed Fix:**
  Wrap the entire function body inside `try / catch` and validate `req.is_discarded()` first:
  ```cpp
  std::string Bridge::handle(const std::string& requestJson) {
      try {
          json req = json::parse(requestJson, nullptr, false);
          if (req.is_discarded() || !req.is_object() || !req.contains("cmd")) {
              json res;
              res["id"] = 0;
              res["ok"] = false;
              res["error"] = "bad request";
              return res.dump();
          }
          json res;
          res["id"] = req.value("id", 0);
          const std::string cmd = req.value("cmd", "");
          const json args = req.value("args", json::object());
          ...
      } catch (const std::exception& e) {
          json res;
          res["id"] = 0;
          res["ok"] = false;
          res["error"] = e.what();
          LOG_ERROR(kTag, "Bridge exception: %s", e.what());
          return res.dump();
      }
  }
  ```

---

#### 🟠 HIGH-4: Missing Bridge Handlers for `lab.tempo` and `lab.midi` from Web UI
- **File & Line:** `bridge/src/Bridge.cpp:330-344`, `ui-web/app.js:583, 709-713`, `ui-web/index.html:66`
- **Severity:** 🟠 HIGH
- **Observation:**
  In `ui-web/app.js` line 583:
  `['stem', 'denoise', 'keychord', 'tempo', 'midi'].forEach((job) => ...)`
  And in `initLab()` line 713:
  `bridge('lab.' + job, args)`
  When user triggers `tempo` or `midi`, `Bridge.cpp` has no branch for `cmd == "lab.tempo"` or `cmd == "lab.midi"`.
  `Bridge.cpp` falls through to line 362: `res["ok"] = false; res["error"] = "unknown cmd: lab.tempo"`.
- **Reproduction Scenario:**
  Select a file in Browser, right-click -> "Detect Tempo" -> Toast displays `Lab error: unknown cmd: lab.tempo`.
- **Proposed Fix:**
  Handle `lab.tempo` (routes to `"analyze"` job) and `lab.midi` in `Bridge.cpp`:
  ```cpp
  } else if (cmd == "lab.tempo" || cmd == "lab.analyze") {
      m_impl->runLabJob("analyze", narrowPath(args.value("path", "")), 0);
      res["ok"] = true;
  } else if (cmd == "lab.midi") {
      m_impl->runLabJob("keychord", narrowPath(args.value("path", "")), 0);
      res["ok"] = true;
  }
  ```

---

#### 🟡 MEDIUM-3: Missing `tagCache` & `favSet` Loading in Frontend Web UI
- **File & Line:** `ui-web/app.js:229-233, 437, 441`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  In `ui-web/app.js`:
  ```js
  const tag = state.tagCache && state.tagCache[f.path] || 0;
  ...
  if (state.favSet && state.favSet.has(f.path)) row.appendChild(el('span', 'star', '★'));
  ```
  `state.tagCache` and `state.favSet` are never declared or populated in `state`, `initBrowser()`, or `renderFiles()`. As a result, color tag dots and favorite stars '★' are never displayed on files in the browser.
- **Reproduction Scenario:**
  Add a file to favorites or assign a color tag -> The file list does not display the star or color dot.
- **Proposed Fix:**
  In `app.js`:
  ```js
  // Inside renderTree():
  const favs = await bridge('browser.favorites');
  state.favSet = new Set(favs);
  ```
  And when listing files, batch-fetch or cache tags so `fileRowEl` can render tag dots.

---

#### 🟡 MEDIUM-4: Broken Sort Dropdown in Browser UI
- **File & Line:** `ui-web/app.js:418-434, 466`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  Changing `#sort` dropdown triggers `$('#sort').onchange = (e) => { state.sort = +e.target.value; renderFiles(); };`.
  However, `renderFiles()` takes `files` returned from `bridge('fs.list')` and immediately iterates over them without sorting according to `state.sort` (0: Name, 1: Size, 2: Date).
- **Reproduction Scenario:**
  Change Sort dropdown to "Dung lượng" (Size) or "Ngày sửa" (Date) -> File list order remains unchanged.
- **Proposed Fix:**
  In `ui-web/app.js`:
  ```js
  files.sort((a, b) => {
    if (state.sort === 1) return (b.size || 0) - (a.size || 0); // Size desc
    if (state.sort === 2) return (b.modified || 0) - (a.modified || 0); // Date desc
    return a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' });
  });
  ```

---

#### 🟡 MEDIUM-5: i18n Key Mismatch Between C++ Backend and Frontend Toast Strings
- **File & Line:** `assets/i18n/strings_vi.json:69-75`, `extension/src/reaper_plugin.cpp:90, 97`, `ui-web/app.js:46-50`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  In `reaper_plugin.cpp`:
  - `reals::i18n::tr("browser.toast.notMedia")`
  - `reals::i18n::tr("browser.toast.inserted")`
  In `assets/i18n/strings_vi.json`:
  These keys do not exist (they are missing from the JSON file).
  In `ui-web/app.js`:
  They are defined as `'toast.notMedia'` and `'toast.inserted'`.
  When C++ sends `i18n::tr("browser.toast.notMedia")`, because the key is not in `strings_vi.json`, it returns the raw key string `"browser.toast.notMedia"`, which is then shown to the user on screen.
- **Proposed Fix:**
  Add missing keys in `assets/i18n/strings_vi.json` and `assets/i18n/strings_en.json`:
  ```json
  "browser.toast.notMedia": "File này không phải media",
  "browser.toast.inserted": "Đã chèn vào project",
  "browser.toast.insertFail": "Chèn file thất bại"
  ```

---

#### 🟡 MEDIUM-6: Missing `fs.addRoot` and `fs.removeRoot` Bridge Endpoints
- **File & Line:** `bridge/src/Bridge.cpp:205-234`, `core/include/reals/browser/BrowserModel.h:38-39`
- **Severity:** 🟡 MEDIUM
- **Observation:**
  `BrowserModel` implements `addRoot()` and `removeRoot()`, but `Bridge::handle()` does not expose commands `fs.addRoot` or `fs.removeRoot`. Right-clicking a folder to "Đặt làm thư mục gốc" only shows a toast saying "(P1.6)".
- **Proposed Fix:**
  Add `fs.addRoot` and `fs.removeRoot` to `Bridge::handle()`:
  ```cpp
  } else if (cmd == "fs.addRoot") {
      model.addRoot(args.value("name", ""), narrowPath(args.value("path", "")));
      res["ok"] = true;
  } else if (cmd == "fs.removeRoot") {
      model.removeRoot(args.value("index", 0u));
      res["ok"] = true;
  }
  ```

---

### Section D: Async Event Queue & Background Lab Jobs (Scope 4)

#### 🔴 CRITICAL-4: Detached Background Threads Crashing Process on DLL Unload / Extension Exit
- **File & Line:** `bridge/src/Bridge.cpp:45, 135`
- **Severity:** 🔴 CRITICAL
- **Observation:**
  In `Bridge::Impl::runLabJob`:
  ```cpp
  void runLabJob(const std::string& job, const std::string& path, const int modeOrStrength) {
      std::thread([this, job, path, modeOrStrength]() {
          try {
              ...
              for (;;) {
                  std::this_thread::sleep_for(std::chrono::seconds(2));
                  last = lab::LabApi::pollJob(taskId);
                  ...
                  pushEvent(prog);
              }
              ...
          } catch (...) { ... }
      }).detach(); // <--- CRITICAL DETACHED THREAD IN SHARED DLL
  }
  ```
  `runLabJob` spawns detached threads (`.detach()`) that loop for several minutes during audio processing.
  If the user closes REAPER or disables the extension (`FreeLibrary`), the operating system unmaps `reaper_realslab.dll` from memory.
  When the sleeping detached thread wakes up and attempts to execute instructions in unmapped code memory or calls `this->pushEvent()`, an immediate fatal memory crash (`0xC0000005 Access Violation`) occurs.
- **Reproduction Scenario:**
  1. Start a stem separation or chord analysis job on a large audio file.
  2. Immediately close REAPER while the job is still polling.
  3. REAPER encounters an unhandled crash during shutdown.
- **Proposed Fix:**
  Replace detached threads with managed worker threads with a cancellation flag (`std::atomic<bool> m_stopping`), and join all active threads in `Bridge::~Bridge()` / `Impl::~Impl()` before DLL unload:
  ```cpp
  struct Bridge::Impl {
      std::atomic<bool> stopping{false};
      std::vector<std::thread> workers;
      std::mutex workersMutex;

      ~Impl() {
          stopping = true;
          std::lock_guard lock(workersMutex);
          for (auto& t : workers) {
              if (t.joinable())
                  t.join();
          }
      }
      ...
  ```

---

#### 🟠 HIGH-5: Infinite Polling Loop in `runLabJob` Without Timeout
- **File & Line:** `bridge/src/Bridge.cpp:59-75`
- **Severity:** 🟠 HIGH
- **Observation:**
  In `runLabJob`, `for (;;)` polls `pollJob(taskId)` every 2 seconds indefinitely until `status == "COMPLETE"` or `status == "FAILED"`.
  Per `SPEC.md` Section 4.3:
  "Client poll mỗi 2s, timeout 10 phút, hỗ trợ offline queue."
  If the server gets stuck in `"QUEUED"` or `"RUNNING"` or hangs, the worker thread polls forever, leaking memory and thread resources.
- **Proposed Fix:**
  Add a maximum iteration counter (e.g. 300 iterations = 10 minutes):
  ```cpp
  int pollCount = 0;
  constexpr int kMaxPolls = 300; // 10 minutes at 2s interval
  for (;;) {
      if (stopping || ++pollCount > kMaxPolls)
          throw std::runtime_error("Lab job timed out after 10 minutes");
      std::this_thread::sleep_for(std::chrono::seconds(2));
      ...
  ```

---

### Section E: Code Style, Lifecycle & Architecture (Low / Refactor)

#### 🔵 LOW-1: Unbalanced COM Apartment & Missing Window Cleanup on Extension Unload
- **File & Line:** `extension/src/reaper_plugin.cpp:228, 321-328`
- **Severity:** 🔵 LOW / REFACTOR
- **Observation:**
  In `createHostWindow()`: `CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)` is called.
  When plugin is unloaded (`!rec`), `CoUninitialize()` is never called, `DestroyWindow(g_hwnd)` is not called, and `UnregisterClassW` is not called.
- **Proposed Fix:**
  Perform full cleanup on `!rec`:
  ```cpp
  if (!rec) {
      plugin_register("-timer", reinterpret_cast<void*>(timerHook));
      if (g_web) g_web.reset();
      if (g_bridge) g_bridge.reset();
      if (g_hwnd) {
          DestroyWindow(g_hwnd);
          g_hwnd = nullptr;
      }
      UnregisterClassW(kWndClass, g_hInstance);
      CoUninitialize();
      LOG_INFO(kTag, "entry: unloaded cleanly");
      return 0;
  }
  ```

---

#### 🔵 LOW-2: Non-UTF-8 `std::filesystem::path` Conversions on Windows
- **File & Line:** `core/src/platform/Path.cpp:74, 81, 90`, `core/src/browser/BrowserModel.cpp:153, 229`
- **Severity:** 🔵 LOW / REFACTOR
- **Observation:**
  On Windows MSVC, passing `std::string` (UTF-8) directly into `std::filesystem::path` constructor assumes ANSI code page (`CP_ACP`), which corrupts paths containing Vietnamese characters.
- **Proposed Fix:**
  Use UTF-16 wide paths `toWide(path)` when constructing `fs::path` on Windows, or use `fs::u8path`.

---

## 3. Component 2: Logic Chain

1. **JSON Handling Logic Chain:**
   `json::parse(requestJson, nullptr, false)` on malformed input $\rightarrow$ returns discarded JSON $\rightarrow$ line 177 calls `req.contains("id")` $\rightarrow$ throws `json::type_error` $\rightarrow$ line 177 is outside `try/catch` $\rightarrow$ uncaught exception in C++ plugin $\rightarrow$ `std::terminate()` called $\rightarrow$ REAPER DAW crashes instantly.

2. **COM Lifetime Logic Chain:**
   `WebViewHost` allocates `Impl` on heap via raw pointer $\rightarrow$ default destructor does nothing $\rightarrow$ `ICoreWebView2Controller::Close()` is never called $\rightarrow$ `ComPtr` ref counts stay non-zero $\rightarrow$ `msedgewebview2.exe` child process remains orphaned in background upon window close $\rightarrow$ pending COM callbacks execute into freed `this` pointer $\rightarrow$ Use-After-Free crash.

3. **Background Threading Logic Chain:**
   `runLabJob` calls `.detach()` on `std::thread` running a multi-minute polling loop $\rightarrow$ User closes REAPER or removes extension $\rightarrow$ `FreeLibrary` unmaps `.text` code section of `reaper_realslab.dll` $\rightarrow$ background thread wakes up from `sleep_for` and jumps to unmapped address or accesses destroyed mutex $\rightarrow$ Fatal Access Violation `0xC0000005`.

4. **DAW Undo Integrity Logic Chain:**
   User clicks "Insert All Stems" $\rightarrow$ `reaper.insertMany` loops over 4 stem paths $\rightarrow$ each file calls `m_actions->insertMedia` $\rightarrow$ opens and closes `Undo_BeginBlock` / `Undo_EndBlock` 4 separate times $\rightarrow$ REAPER undo history gets 4 distinct undo entries instead of 1 atomic transaction $\rightarrow$ pressing `Ctrl+Z` leaves project in half-imported state.

---

## 4. Component 3: Caveats

- **Network Mode:** The lab analysis API endpoints (`https://smk285pro--ai-audio-lab-fastapi-web.modal.run`) rely on remote server availability. Offline mocking fallback works for local development.
- **macOS / Linux Shells:** The current investigation audited the Windows shell (`shell/win/`). Non-Windows platforms (macOS WKWebView, Linux WebKitGTK) are scheduled for Phase 6 per `SPEC.md`.

---

## 5. Component 4: Conclusion & Remediation Plan

### Summary Table

| ID | Module / File | Line | Severity | Summary |
|---|---|---|---|---|
| **C-1** | `bridge/src/Bridge.cpp` | 175-179 | 🔴 CRITICAL | Unhandled JSON parse exception crashing REAPER on malformed message |
| **C-2** | `shell/win/WebViewHost.cpp` | 80 | 🔴 CRITICAL | `WebViewHost` memory leak & missing `controller->Close()` |
| **C-3** | `shell/win/WebViewHost.cpp` | 103, 120 | 🔴 CRITICAL | Dangling `this` pointer & UAF in async COM init handlers |
| **C-4** | `bridge/src/Bridge.cpp` | 45, 135 | 🔴 CRITICAL | Detached threads in DLL crashing process on exit |
| **H-1** | `extension/src/reaper_plugin.cpp` | 85-101 | 🟠 HIGH | Non-atomic Undo block on batch stem insert (`reaper.insertMany`) |
| **H-2** | `bridge/src/Bridge.cpp` | 330-344 | 🟠 HIGH | Missing Bridge handlers for `lab.tempo` and `lab.midi` |
| **H-3** | `extension/src/reaper_plugin.cpp` | 95-97 | 🟠 HIGH | Ignored return code from REAPER `InsertMedia` |
| **H-4** | `extension/src/reaper_plugin.cpp` | 145-148 | 🟠 HIGH | 600-byte buffer truncation in `json_event` creating invalid JSON |
| **H-5** | `bridge/src/Bridge.cpp` | 59-75 | 🟠 HIGH | Infinite polling loop without 10-min timeout cap |
| **M-1** | `shell/win/WebViewHost.cpp` | 206-211 | 🟡 MEDIUM | Missing `put_IsVisible(FALSE)` on window hide |
| **M-2** | `extension/src/reaper_plugin.cpp` | 168-183 | 🟡 MEDIUM | Missing `WM_DPICHANGED` handling |
| **M-3** | `ui-web/app.js` | 229, 437 | 🟡 MEDIUM | Missing `tagCache` and `favSet` loading in file list |
| **M-4** | `ui-web/app.js` | 418-434 | 🟡 MEDIUM | Broken Sort dropdown in Browser UI |
| **M-5** | `assets/i18n/strings_vi.json` | 69-75 | 🟡 MEDIUM | Missing toast i18n keys between backend and frontend |
| **M-6** | `bridge/src/Bridge.cpp` | 205-234 | 🟡 MEDIUM | Missing `fs.addRoot` / `fs.removeRoot` bridge APIs |
| **L-1** | `extension/src/reaper_plugin.cpp` | 228, 321 | 🔵 LOW | Unbalanced COM & missing HWND/class cleanup on unload |
| **L-2** | `core/src/platform/Path.cpp` | 74, 81 | 🔵 LOW | ANSI vs UTF-8 path conversions on Windows |
| **L-3** | `shell/win/WebViewHost.cpp` | 131 | 🔵 LOW | Missing `add_AcceleratorKeyPressed` coordination |

---

## 6. Component 5: Verification Method

### Step 1: Verification Commands
```powershell
# Configure and build on Windows
cmake --preset windows
cmake --build --preset windows --config Release
```

### Step 2: Test Reproduction Cases
1. **JSON Crash Test:** Send `window.chrome.webview.postMessage("{not valid json")` -> Verify Bridge returns `{ ok: false, error: "bad request" }` without crashing.
2. **Batch Undo Test:** Insert 4 stem tracks -> Press `Ctrl+Z` in REAPER -> Verify all 4 tracks are removed in a single undo step.
3. **Shutdown Safety Test:** Start a stem separation job -> Immediately close REAPER -> Verify clean shutdown without `0xC0000005` error.
4. **WebView2 Teardown Test:** Open and close Reals Lab window 10 times -> Check Task Manager -> Verify no orphaned `msedgewebview2.exe` processes remain.
5. **DPI Scaling Test:** Drag window between 100% and 175% DPI screens -> Verify crisp rendering and proper layout resizing.
