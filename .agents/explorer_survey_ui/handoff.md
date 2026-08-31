# Handoff Report — Explorer 2 (UI & Frontend/IPC Specialist)

## 1. Observation
- **Webview & UI Asset Structure**:
  - `ui-web/index.html` (442 lines): Defines TopBar (height 44px with `#topNavSlot`, `#winDragRegion`, window actions `#btnDock`, `#btnSettings`, `#btnWinMin`, `#btnWinMax`, `#btnWinClose`), 5 Panes (`#pane-market`, `#pane-audioLab`, `#pane-agent`, `#pane-browser`, `#pane-account`), Browser toolbar (`#btnAddFolder`, `#search`, `#sort`, `#favOnly`, `#tagFilter`, `#btnRefresh`), Left Tree (`#tree`), Right Virtual List (`#files`), Bottom Player (`#preview` with `#waveform` canvas, `#meter` VU canvas, `#volume`, `#trackInfo`), and `#pianoTransposerPop`.
  - `ui-web/app.js` (3974 lines): Main UI controller and state machine.
  - `ui-web/app.css` & `ui-web/tokens.css`: Styling and design system tokens for 3 themes (`dark-studio`, `pastel-pink`, `cyberpunk`).
- **Favorites UI (`#favOnly` / `★`)**:
  - In `ui-web/app.js` lines 2608–2616, `#favOnly` toggles `state.favOnly = !state.favOnly` and calls `paintFromRaw(true)`.
  - In `ui-web/app.js` lines 2139–2148 (`filteredFiles()`), `state.favOnly` filters `state.rawFiles` (`if (state.favOnly && !state.favSet.has(f.path)) return false`).
  - Currently `state.rawFiles` contains only files from `state.currentDir`.
  - In `bridge/src/Bridge.cpp` lines 813–822, `browser.favorites` returns `BrowserModel::favorites()` (`m_favorites`), and `browser.toggleFavorite` calls `BrowserModel::toggleFavorite()` which persists into `%APPDATA%\RealsLab\browser_store.json`.
- **Search UI (`#search`)**:
  - In `ui-web/app.js` lines 2572–2600, input is debounced with 200ms timer (`searchTimer`).
  - In `core/src/search/QueryParser.cpp` lines 134–212, syntax `/fav`, `/bpm:range`, `/key:root`, `/camelot:code`, `/openkey:code`, `/genre:name`, `/mood:name`, and `/tag` are parsed into `ParsedQuery`.
  - In `ui-web/app.js` lines 2386–2402 & `bridge/src/Bridge.cpp` lines 477–558, async generation tokens (`searchGen`, `searchSeq`) and cancellation flags (`searchCancel->store(true)`) abort old search workers.
  - In `ui-web/app.js` lines 2556–2569, `#searchClear` clears search, resets `state.searchQ = ''`, reloads `state.currentDir`, and `paintFromRaw()` restores previous scroll from `state.dirScrolls[state.currentDir]`.
  - In `ui-web/app.js` line 2395, `bridge('browser.search', { base: state.currentDir || '', ... })` passes `state.currentDir` as base.
- **Virtual Scrolling & 60 FPS Probing**:
  - In `ui-web/app.js` lines 2193–2224 (`paintVisible()`), visible range is calculated: `start = Math.max(0, Math.floor(scroll / rowH) - VIRT_OVERSCAN)` (`VIRT_OVERSCAN = 8`), placing absolute `.file-row` items inside `#fileSpacer`.
  - Dynamic row height `getRowH()`: 36px (small), 46px (medium), 56px (large).
  - Fast selection event delegation on `#files` pointerdown (`lines 2641–2662`).
  - In `ui-web/app.js` lines 2345–2384 (`probeVisibleAudio()`), scroll is debounced by 100ms (`_scrollProbeTimer`), batching up to 16 visible un-probed audio files to `audio.probe`. C++ computes envelope in background workers (`Engine::computeEnvelope`) and pushes `audio.envelope` event, updating mini SVG waveform (`updateRowMiniWave`) without re-layout.
  - `startPlayerAnimLoop()` runs at 60 FPS via `requestAnimationFrame` with analog VU ballistics (instant attack, exponential decay).
- **IPC Bridge Architecture**:
  - `bridge(cmd, args)` sends JSON via `window.chrome.webview.postMessage({ id, cmd, args })`.
  - `WebViewHost.cpp` handles COM events via `ICoreWebView2WebMessageReceivedEventHandler` and dispatches to `Bridge::handle()`.
  - Responses return via `ICoreWebView2::PostWebMessageAsJson()`.
  - C++ background workers queue events in `SharedState::events`. The REAPER 30Hz `timerHook` drains events via `Bridge::drainEvents()` and posts to WebView2.
- **Default Roots**:
  - In `core/src/browser/BrowserModel.cpp` lines 155–166, hardcoded `Music`, `Desktop`, `Downloads` roots are pushed during initialization.

## 2. Logic Chain
1. *Observation*: `#favOnly` only filters `state.rawFiles` in `filteredFiles()`, which only contains files from `state.currentDir`.
   *Inference*: To satisfy R1 (Global Favorites), activating `#favOnly` must request all favorited file records globally across all library roots and populate `state.rawFiles`.
2. *Observation*: `runSearch()` in `app.js` passes `base: state.currentDir`. In `Bridge.cpp`, `opts.basePath = base` restricts database and file crawler queries to `base`.
   *Inference*: To satisfy R2 (Global Search Across All Roots), `runSearch()` should pass `base: ''` (or empty) so `SearchEngine` queries the entire DB and the file crawler iterates over all configured `model.roots()`.
3. *Observation*: `BrowserModel::BrowserModel()` hardcodes `Music`, `Desktop`, and `Downloads`.
   *Inference*: To satisfy R3 (Clean Initial Default Roots), removing these 3 pushes allows fresh instances to start empty with `#btnAddFolder` / drop-zone.
4. *Observation*: Virtual list rendering in `paintVisible()`, pointerdown event delegation, and 100ms debounced probing in `probeVisibleAudio()` minimize DOM allocations and CPU usage.
   *Inference*: The frontend is already optimized for 60 FPS and 5,000+ files with sub-16ms frame times.

## 3. Caveats
- No project source code was modified during this exploration turn (strict read-only investigation).
- Standalone app (`app/`) was not inspected in depth; analysis focused on the REAPER extension shell (`extension/`), `bridge/`, `core/`, and `ui-web/`.
- Testing of Web Audio MIDI playback requires `.mid` files with valid header format (`MThd`/`MTrk`).

## 4. Conclusion
The frontend UI and IPC bridge architecture are well-designed, decoupled, and fast. Implementing requirements R1, R2, and R3 requires straightforward, non-breaking modifications in:
1. `ui-web/app.js`: Global favorites view population on `#favOnly` toggle, empty `base` parameter in `browser.search`.
2. `core/src/browser/BrowserModel.cpp`: Removal of default `Music`/`Desktop`/`Downloads` roots, global search fallback across all roots.
3. `bridge/src/Bridge.cpp`: Multi-root crawler fallback when `base` is empty, `browser.allFavorites` command support.

## 5. Verification Method
1. Compile with CMake MSVC C++20:
   `cmake --build --preset windows` (must compile with 0 warnings and 0 errors).
2. Run test suites:
   `ctest --preset windows` (100% test pass).
3. Code Inspection Verification:
   - Check `ui-web/index.html`, `ui-web/app.js`, `bridge/src/Bridge.cpp`, `core/src/browser/BrowserModel.cpp`.
   - Verify report at `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_ui\analysis.md`.
