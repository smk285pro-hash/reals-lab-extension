# Reals Lab — Comprehensive UI Frontend & IPC Bridge Survey Report

**Explorer 2 (UI & Frontend/IPC Specialist)**  
**Date:** 2026-09-01  
**Target Repository:** `reals-lab-extension`  
**Working Directory:** `.agents/explorer_survey_ui/`

---

## Executive Summary

Reals Lab utilizes a modern **hybrid architecture** combining a native C++20 backend core and a high-performance **WebView2 (HTML/CSS/JS)** user interface. The UI is designed to look and feel like a high-end native audio workstation plugin (such as FL Studio Browser / FL Cloud) with instant 0ms response times, 60 FPS smooth animations, virtual scrolling, real-time waveform visualization, and seamless OLE drag-and-drop into REAPER.

This survey provides an exhaustive analysis of:
1. Webview asset structure, styling, theme synchronization, and DOM hierarchy.
2. Favorites mechanism (`#favOnly`, `★`), its persistence, and the architecture required for global multi-root display.
3. Search engine input debouncing, syntax filter parsing (`/tag`, `/bpm:range`, `/key:note`), asynchronous search cancellation, and scroll position restoration.
4. 60 FPS file listing virtualization, event delegation, and debounced background waveform probing.
5. IPC communication flow between WebView2 JavaScript and the C++ Win32/REAPER shell.

---

## 1. Webview & UI Structure

### 1.1 Directory & Asset Layout

All UI assets reside in the `ui-web/` directory and are mapped to `https://app.local/` via WebView2 Virtual Host Mapping (`SetVirtualHostNameToFolderMapping`).

| Asset Path | Responsibility |
|---|---|
| `ui-web/index.html` | Root HTML structure (TopBar, 5 Navigation Panes, Player, Settings modal, Modals, Overlays). |
| `ui-web/app.js` | Main UI controller (state machine, IPC bridge, virtual scroll, canvas renderers, event delegation, Web Audio MIDI synth). |
| `ui-web/app.css` | Primary styles, CSS variables, responsive breakpoints, Studio Dark/Kawaii styling, layout splitters. |
| `ui-web/tokens.css` | Design system tokens for themes: `--bg-root`, `--bg-app`, `--border-subtle`, `--accent`, etc. |
| `ui-web/baloo2.css` | Offline font styling for headers and numbers. |
| `ui-web/fonts/` | WOFF2 font files (`inter-*`, `baloo2-*`) ensuring zero external network dependency during DAW operation. |
| `ui-web/assets/` | Scalable vector graphics (`logo.svg`, `kawaii/*.svg`, `reals-mark.svg`). |

### 1.2 DOM Hierarchy & Component Architecture

The interface follows a clean, nested structure:
- **`#app`**: Main container supporting 4 navigation orientations (`.nav-top`, `.nav-bottom`, `.nav-left`, `.nav-right`), custom frameless window styling (`border-radius: 10px`), and 8-directional resize handles (`.resize-handle.top`, `.resize-handle.top-left`, etc.).
- **`#topbar` (Height 44px)**:
  - `#topNavSlot` -> `#sidebar`: 5 Navigation tabs (`Market`, `Audio Lab`, `Agent`, `Browser`, `Account`).
  - `#winDragRegion`: Window caption drag handle wired to Win32 `WM_NCLBUTTONDOWN` (`HTCAPTION`).
  - `.topbar-actions`: Action controls (`#btnDock` for REAPER Docker docking, `#btnSettings` for modal configuration, `#btnWinMin`, `#btnWinMax`, `#btnWinClose`).
- **`#bodyRow` -> `#content`**:
  - `#pane-market`: Marketplace browser with category chips and plugin cards.
  - `#pane-audioLab`: AI stem separation, denoise, key/chord detection studio interface.
  - `#pane-agent`: Natural language REAPER automation assistant interface.
  - **`#pane-browser` (Active Core)**:
    - `.browser-toolbar`:
      - Row 1 (Primary): `#btnAddFolder` (with hidden `#folderInput`), `#search` (with `#searchClear` and `#searchSuggest`), `#sort` dropdown (Name / Size / Date).
      - Row 2 (Filters): `#favOnly` toggle button (`★`), `#tagFilter` color palette selector (0-7), `#btnRefresh` button.
    - `#scannerBar`: Real-time scan progress bar, file label, CPU throttle selector (`#scannerCpuMode`: 30%/50%/85%), cancel button (`#btnScannerCancel`).
    - `.browser-body`:
      - `#tree`: Left folder tree (`#treeNodes`), auto-collapsing accordion.
      - `#treeSplitter`: Draggable vertical splitter (`.splitter-v`) adjusting `#tree` width.
      - `#files`: Right virtual scrolling file list (`#fileSpacer` + absolute `.file-row`).
    - `#previewSplitter`: Draggable horizontal splitter (`.splitter-h`) adjusting player height.
    - `#preview`: Bottom audio player:
      - Control buttons (`#btnPlay`, `#btnLoop`, `#btnSyncBpm`, `#btnKeyTransposer` + `#pitchShiftBadge`, `#btnLab`, `#timeLabel`).
      - `#waveform`: 180-bar dual-tone audio waveform canvas or multi-track MIDI piano roll canvas.
      - `#meter`: Smoothed hardware VU peak meter canvas with scrubber indicator.
      - `#volume`: Audio output volume slider.
      - `#trackInfo`: Sample format and BPM/Key metadata badge (OLE drag-enabled).
    - `#pianoTransposerPop`: 12-key interactive Mini Piano Keyboard transposer popup.
- **`#ctxMenu`**: Floating context menu with edge-boundary auto-flip.
- **`#dropOverlay`**: Visual overlay shown when folders are dragged over the window from Windows Explorer.

### 1.3 Theme Engine & Dynamic Color Synchronization

The UI supports 3 built-in themes:
1. `dark-studio` (Default): Dark charcoal background (`#0D0E11`), subtle borders (`#24262B`), Reals Orange accent (`#FF6B2C`).
2. `pastel-pink`: Cream background (`#FDF0E9`), soft rose accents (`#FF8DA6`).
3. `cyberpunk`: Deep navy background (`#0B0E14`), neon cyan accent (`#38BDF8`).

When themes change, `ThemeManager` in `app.js` updates `data-theme` on `<html>`, extracts computed CSS variables, and notifies canvas renderers via `themeUpdated` CustomEvent so the `#waveform` and `#meter` canvases immediately redraw with matching theme palette colors.

---

## 2. Favorites UI (`#favOnly` / `★`)

### 2.1 UI Implementation & State Management

- **Toggle Button**: `#favOnly` in `ui-web/index.html` (line 141) and `ui-web/app.js` (lines 2608-2615).
- **State Properties**:
  - `state.favOnly` (Boolean): Current toggle status.
  - `state.favSet` (Set<string>): Set of normalized full paths of favorited files.
- **Favorites Storage & Backend IPC**:
  - On startup (`renderTree()`, line 1792), `bridge('browser.favorites')` queries C++ `BrowserModel::favorites()`.
  - Stored in `state.favSet = new Set(favs || [])`.
  - Toggling a file's favorite status:
    - User clicks `'★ / ☆'` in the context menu (`app.js` line 3519).
    - Calls `bridge('browser.toggleFavorite', { path: f.path })`.
    - In C++ (`Bridge.cpp` lines 818-822 & `BrowserModel.cpp` lines 341-349): `BrowserModel::toggleFavorite()` updates `m_favorites`, immediately saves to `%APPDATA%\RealsLab\browser_store.json` using atomic temporary file rename (`saveStore()`), and returns the new boolean state.
    - Frontend receives the boolean, updates `state.favSet`, updates star icon, and triggers `renderTree()` and `paintFromRaw(true)`.

### 2.2 Current Behavior vs. Global Favorites View Requirement (R1)

#### Current Implementation:
In `ui-web/app.js` (lines 2139-2148):
```javascript
function filteredFiles() {
  return state.rawFiles.filter((f) => {
    if (f.isDir) return false;
    if (state.favOnly && !state.favSet.has(f.path)) return false;
    if (state.tagFilter > 0 && (state.tagCache[f.path] || 0) !== state.tagFilter) return false;
    return true;
  });
}
```
Currently, `state.rawFiles` contains only the files from the **currently selected directory** (`state.currentDir`). When the user activates `#favOnly`, it only filters files that exist inside the currently open folder. If the user is in `D:\Samples\Kicks`, clicking `★` only shows favorited kicks in that folder.

#### Architectural Requirement for R1 (Global Favorites):
When `#favOnly` is activated, the file list must immediately display **ALL** favorited audio samples and MIDI files across the entire library (all roots and subfolders), regardless of the currently open folder.

**Recommended Solution**:
1. When `#favOnly` is toggled ON:
   - If `state.favOnly === true`, dispatch `bridge('browser.allFavorites')` or `bridge('db.search', { query: '/fav' })`.
   - In backend C++ (`Bridge.cpp` / `BrowserModel.cpp`), resolve full file metadata (`FileEntry` with name, size, duration, bpm, key) for all paths in `m_favorites`.
   - Populate `state.rawFiles` with these global favorite entries, set `state.listDir = null`, and render the unified list.
2. When `#favOnly` is toggled OFF:
   - Restore the previous folder view by reloading `state.currentDir` via `loadDir(state.currentDir, false)` and restoring scroll position.

---

## 3. Search UI & Filter Syntax

### 3.1 Input Handling & Debouncing

- **Search Bar Element**: `#search` in `ui-web/index.html` (line 130).
- **Clear Button**: `#searchClear` (line 131) with visual `✕` glyph.
- **Autocomplete Popup**: `#searchSuggest` (line 132).
- **Debounce Mechanism** (`app.js` lines 2572-2589):
  ```javascript
  searchInput.addEventListener('input', (e) => {
    clearTimeout(searchTimer);
    const val = e.target.value;
    if (searchClear) searchClear.classList.toggle('hidden', !val);
    updateSuggestions(val);
    searchTimer = setTimeout(() => {
      state.searchQ = val;
      const q = (state.searchQ || '').trim();
      if (q) runSearch(q);
      else if (state.currentDir) {
        state.searchPending = false;
        state.searchGen = ++state.searchSeq;
        loadDir(state.currentDir, false);
      }
    }, 200); // 200ms debounce
  });
  ```

### 3.2 Filter Syntax & Autocomplete Suggestions

The search bar supports advanced token syntax parsed by both frontend autocomplete suggestions (`browser.suggestTags`) and C++ `reals::search::QueryParser`:

| Syntax Pattern | Parsed Behavior | Example |
|---|---|---|
| `/fav` or `/favorite` | Restricts search to favorited samples | `/fav kick` |
| `/bpm:min-max` | Filters samples within BPM range `[min, max]` | `/bpm:120-130` |
| `/bpm:val` | Filters samples within `[val - 2, val + 2]` | `/bpm:140` |
| `/key:root[mode]` | Matches musical key (auto-converted to Camelot) | `/key:Am`, `/key:F#` |
| `/camelot:code` | Filters by exact Camelot wheel code | `/camelot:11A`, `/camelot:8B` |
| `/openkey:code` | Filters by OpenKey notation | `/openkey:4m` |
| `/genre:style` | Matches specific genre or sub-genre | `/genre:Trap-EDM` |
| `/mood:feeling` | Matches acoustic mood tag | `/mood:dark` |
| `/[tag]` | Matches instrument or custom tag | `/trap`, `/kick`, `/808`, `/vocal` |
| Free text keywords | SIMD Cosine similarity CLAP semantic search + filename match | `punchy acoustic snare` |

When a user types `/`, `updateSuggestions` checks the current token and queries `bridge('browser.suggestTags')`. A chip popup appears; clicking any chip autocompletes the token into the search box.

### 3.3 Asynchronous Search & Race Cancellation

- In JS (`app.js` lines 2386-2402):
  - Every search increments `state.searchSeq` -> `state.searchGen = ++state.searchSeq`.
  - Sets `state.searchPending = true`, clears `state.rawFiles`, and calls `bridge('browser.search', { base, query: q, gen })`.
- In C++ (`Bridge.cpp` lines 477-558):
  - Cancels any running search thread via atomic flag `searchCancel->store(true)`.
  - Spawns background worker running `SearchEngine::search()` (DB index + CLAP embedding cosine similarity) and filesystem crawler fallback.
  - If `cancel->load()` or generation mismatch occurs, the worker aborts instantly without allocating results.
  - On completion, pushes `browser.searchResult` event with matching `gen`.
- In JS event handler (`app.js` lines 776-785):
  - Discards results if `data.gen !== state.searchGen`.
  - Updates `state.rawFiles = data.results`, resets `state.searchPending = false`, and paints the virtual list.

### 3.4 Clearing Search & Scroll Position Restoration

- When `#searchClear` is clicked (`app.js` lines 2556-2569):
  1. Input is emptied, `#searchClear` and `#searchSuggest` are hidden.
  2. `state.searchQ = ''`, `state.searchPending = false`, `state.searchGen = ++state.searchSeq`.
  3. `loadDir(state.currentDir, false)` reloads the current directory.
  4. In `paintFromRaw()` (line 2162):
     `savedScroll = preserveScroll ? box.scrollTop : (state.searchQ ? 0 : (state.dirScrolls[state.currentDir] || 0))`
     The scroll position is restored to `state.dirScrolls[state.currentDir]`, ensuring the user returns to their exact prior browsing location without jumping to the top.

---

## 4. File Tree / List Rendering & 60 FPS Performance

### 4.1 Folder Tree Rendering (`renderTree`)

The tree view (`#tree`) employs a 2-pass rendering architecture:
1. **Pass 1 (0ms Synchronous)**: Builds DOM using cached directory children in `state.subCache` into a `DocumentFragment` and replaces children synchronously via `container.replaceChildren(frag)`.
2. **Pass 2 (Parallel Background)**: If any expanded folders are missing from cache, fetches subdirectories in parallel using `Promise.all(uncached.map(p => bridge('fs.subdirs', { path: p })))`. Once loaded, re-renders the tree without hitching.
3. **Auto-Collapse (`state.autoCollapseTree`)**: `tidyExpandedFolders()` automatically collapses sibling branches when a new directory is opened, keeping the tree clean.

### 4.2 High-Performance Virtual Scrolling (`#files`)

Handling directories with 5,000+ files requires sub-millisecond layout times. `ui-web` implements a DOM virtualization pipeline:
- **Spacer Height**: `#fileSpacer.style.height = total * rowH + 'px'`.
- **Dynamic Density (`getRowH()`)**:
  - Small (Compact): 36px
  - Medium (Standard - Default): 46px
  - Large (Spacious): 56px
- **Window Calculation (`paintVisible()`, lines 2193-2224)**:
  ```javascript
  const scroll = box.scrollTop - headerH;
  const viewH = box.clientHeight || 300;
  let start = Math.max(0, Math.floor(scroll / rowH) - VIRT_OVERSCAN); // OVERSCAN = 8
  let end = Math.min(total, Math.ceil((scroll + viewH) / rowH) + VIRT_OVERSCAN);
  ```
- **Absolute Positioning**: Only rows between `start` and `end` (~20-30 DOM nodes) are created. Each `.file-row` is placed with `position: absolute; top: (i * rowH + 2)px; height: (rowH - 4)px`.
- **Zero Memory Allocation on Selection**: Selection changes (`selectEntry()`, lines 2294-2340) update class `.sel` via `querySelector` rather than re-rendering the entire virtual list, eliminating GC pauses.
- **Fast Event Delegation**: Pointer interactions on `#files` capture `pointerdown` at container level (`lines 2641-2662`), mapping Y-coordinates directly to the index `idx = Math.floor(clickY / rowH)` for instant click preview.

### 4.3 Waveform Probing & Debounced Background Extraction

- **Probing Scheduler (`probeVisibleAudio()`, lines 2345-2384)**:
  - Scrolling `#files` fires `_scrollProbeTimer` (100ms debounce).
  - When idle, slices only visible files that lack cached duration or envelope (`!state.probeCache[f.path] || !state.envCache[f.path]`).
  - Limits concurrent probing to a maximum of 16 files per batch to prevent thread saturation.
  - Adds active paths to `state.probeInflight` Set to avoid duplicate requests.
- **C++ Backend Probing (`Bridge.cpp` & `Engine.cpp`)**:
  - Fast probe: `audio.probe` reads audio file headers (duration, sample rate, channels) in <0.2ms.
  - Asynchronous Envelope computation: Spawns a background thread running `Engine::computeEnvelope(path)` (processes 64k chunks in float32, extracting 80-180 peak points).
  - Emits `audio.envelope` event back to WebView2.
- **Live SVG Mini-Wave Update**:
  - When `audio.envelope` arrives, `updateRowMiniWave(path)` generates an SVG `<svg class="mini-preview-svg">` and injects it into `.mini-preview-bg` of the specific row without triggering list re-layouts.

### 4.4 60 FPS Player Animation & Canvas Rendering

- `startPlayerAnimLoop()` runs via `requestAnimationFrame` (`app.js` lines 3016-3083).
- **Position Extrapolation**: Smoothly advances `state.position += dt / state.duration` at 60 FPS.
- **Analog Ballistics VU Meter**:
  - Instant attack: `_meterSmoothedVal = targetPeak`
  - Exponential decay: `_meterSmoothedVal = Math.max(0, _meterSmoothedVal - dt * 2.4)`
- **Waveform Canvas (`#waveform`)**: Dual-tone rendering (Played = active cyan/accent, Unplayed = muted white/translucent) with 1.2px playhead cursor and centerline.
- **MIDI Piano Roll Canvas**: When playing `.mid`/`.midi` files, the waveform canvas dynamically switches to a Piano Roll view, rendering pitch-colored note blocks and 60 FPS playhead.

---

## 5. IPC Communication Bridge

### 5.1 Architecture & Message Format

The IPC bridge connects WebView2 (Chromium JavaScript) and C++20 Win32/REAPER using JSON-RPC style messages over `window.chrome.webview.postMessage`.

```
[ JavaScript (ui-web) ]
       │  bridge(cmd, args)
       ▼  window.chrome.webview.postMessage({ id, cmd, args })
[ Win32 Shell (WebViewHost.cpp) ]
       │  ICoreWebView2WebMessageReceivedEventHandler
       ▼  Bridge::handle(requestJson)
[ C++ Bridge Dispatcher (Bridge.cpp) ]
       │  Calls Engine / BrowserModel / Database / SearchEngine / REAPER SDK
       ▼  Returns response JSON: { id, ok: true, data: ... }
[ Win32 Shell (WebViewHost.cpp) ]
       │  PostWebMessageAsJson()
       ▼
[ JavaScript (ui-web) ]
       Resolves Promise via Map `_pending.get(id)`
```

### 5.2 Bridge Command Reference

| Group | Command | Parameters | Description |
|---|---|---|---|
| **app** | `app.info` | `{}` | Returns version (`0.2.0`) and platform (`windows`). |
| **config** | `config.getAll`, `config.set` | `{ key, value }` | Reads/writes persistent JSON configuration. |
| **fs** | `fs.roots` | `{}` | Returns list of configured root folders. |
| | `fs.addRoot` | `{ name, path }` | Adds a directory to root list (persisted in `browser_store.json`). |
| | `fs.removeRoot` | `{ path }` | Removes a directory from roots. |
| | `fs.dropPaths` | `{ paths: [] }` | Handles OS folders dropped from Explorer into the window. |
| | `fs.subdirs` | `{ path }` | Lists immediate subdirectories (for tree expansion). |
| | `fs.list` | `{ path, sort }` | Returns recursive file list with sorting (0: Name, 1: Size, 2: Date). |
| | `fs.invalidate` | `{ path }` | Clears directory cache. |
| | `fs.watch` | `{ path }` | Starts IOCP Directory Watcher, emitting `fs.changed`. |
| **browser** | `browser.favorites` | `{}` | Returns array of favorited file paths. |
| | `browser.toggleFavorite` | `{ path }` | Toggles favorite status in store. |
| | `browser.search` | `{ base, query, audioOnly, gen }` | Initiates async hybrid search, emitting `browser.searchResult`. |
| | `browser.suggestTags` | `{ query }` | Returns autocompletion tokens for `/` queries. |
| | `browser.beginDrag` | `{ path, syncBpm, sampleBpm, pitchSemitones }` | Starts OLE drag into REAPER with Take stretch queue. |
| | `browser.rename`, `browser.delete` | `{ from, to }` / `{ path }` | File management with tag/favorite path rewriting. |
| **audio** | `audio.play` | `{ path, loop, syncBpm, sampleBpm }` | Starts sample playback with late PhaseAnchor alignment. |
| | `audio.stop` | `{}` | Stops playback. |
| | `audio.setVolume` | `{ value }` | Sets output volume (0.0 - 1.0). |
| | `audio.setLoop` | `{ value }` | Toggles loop mode. |
| | `audio.setPitchShift` | `{ semitones }` | Sets realtime pitch shifting (±12 semitones). |
| | `audio.setSyncBpm` | `{ enabled, bpm, sampleBpm, path }` | Toggles DAW BPM tempo stretch. |
| | `audio.probe` | `{ path }` | Queries sample duration, format, and initiates background envelope scan. |
| | `audio.readMidi` | `{ path }` | Reads binary `.mid` file as base64 for Web Audio synthesizer. |
| **reaper** | `reaper.insert` | `{ path, playrate, pitchSemitones }` | Inserts media into new track in REAPER timeline wrapped in Undo block. |
| | `reaper.tempo` | `{}` | Queries REAPER project tempo (`Master_GetTempo`). |
| | `reaper.reveal` | `{ path }` | Opens Explorer with file selected (`explorer.exe /select,"path"`). |
| **window** | `window.minimize`, `window.close`, `window.toggleMaximize`, `window.startDrag`, `window.startResize`, `window.toggleDock` | `{ edge }` | Custom frameless window controls & REAPER docking integration. |

### 5.3 Asynchronous C++ Push Events

The backend queues events in `SharedState::events` (`state->pushEvent(json)`). The REAPER main thread `timerHook` drains events every 30ms via `Bridge::drainEvents()` and posts them to WebView2:
- `audio.state`: Playhead position fraction, peak, RMS, playing boolean (sent at 30Hz during playback).
- `audio.envelope`: Precomputed audio envelope array for a given file.
- `audio.syncState`: Synchronized BPM ratio and pitch semitones state.
- `browser.searchResult`: Asynchronous search results payload tagged with generation ID `gen`.
- `fs.changed`: Directory change notification from IOCP `DirWatch`.
- `fs.rootsChanged`: Notifies frontend when roots are added via drag-and-drop.
- `fs.dropHover`: Shows/hides `#dropOverlay` when drag enters/leaves window.
- `scanner.progress`: Live scanning statistics (total, processed, added, current file, percent).
- `window.dockState`: Synchronizes UI when window is docked/undocked in REAPER.

---

## 6. Gap Analysis & Concrete Recommendations

Based on `ORIGINAL_REQUEST.md`, `PLAN.md`, and `SPEC.md`, the following gaps and implementation paths were identified:

### 6.1 Clean Initial Default Roots (Requirement R3)
- **Current Observation**: `BrowserModel::BrowserModel()` (lines 155-166 of `BrowserModel.cpp`) hardcodes default OS directories:
  ```cpp
  m_roots.push_back({"Music", platform::defaultMusicDir()});
  m_roots.push_back({"Desktop", platform::joinPath(up, "Desktop")});
  m_roots.push_back({"Downloads", platform::joinPath(up, "Downloads")});
  ```
- **Fix Recommendation**: Remove these default pushes. Allow `BrowserModel` to start with an empty `m_roots` vector when `browser_store.json` does not exist. The UI already gracefully handles empty roots by displaying `#btnAddFolder` (`+📁`) and the drag-and-drop root drop-zone.

### 6.2 Global Favorites View (Requirement R1)
- **Current Observation**: Clicking `#favOnly` only filters files loaded in the current folder (`filteredFiles()`).
- **Fix Recommendation**:
  1. Add a dedicated bridge command `browser.allFavorites` (or extend `fs.list` / `db.search`) that returns all favorited `FileEntry` records from `m_favorites` across all roots.
  2. In `app.js`, when `#favOnly` is activated, populate `state.rawFiles` with these global entries. When deactivated, restore `state.currentDir`.

### 6.3 Global Search Across All Root Folders (Requirement R2)
- **Current Observation**: In `app.js` line 2395, `bridge('browser.search', { base: state.currentDir || '', ... })` passes `state.currentDir` as the `base` path. In `Bridge.cpp` line 501, `opts.basePath = base` restricts database and file crawler queries to `state.currentDir`.
- **Fix Recommendation**:
  1. In `app.js`, when running search, pass `base: ''` (empty string) to search across all indexed files in the database and across all configured root folders in `state.roots`.
  2. In `Bridge.cpp` (`runSearch`), when `base` is empty, query `SearchEngine` globally and iterate over all `model.roots()` for the filesystem crawler fallback.

### 6.4 Zero-Lag File Browsing Performance (Requirement R4)
- **Current Observation**: Virtual scrolling is already well implemented in `paintVisible()`. Probing is debounced (100ms).
- **Optimization Recommendation**:
  - In `BrowserModel::buildListing()`, ensure directories with 5,000+ files avoid expensive per-file I/O by utilizing `platform::scanDirectoryRecursive` and maintaining caching in `m_cache`.
  - Maintain `VIRT_OVERSCAN = 8` and ensure mini waveform SVGs are injected asynchronously via `updateRowMiniWave` without triggering layout reflows.

---

## 7. Conclusion

The Reals Lab UI and IPC subsystem is well-structured, modular, and built to high standards:
- **Zero external web dependencies**: All fonts and assets are embedded locally.
- **60 FPS Performance**: Virtualized scrolling, late phase anchoring, and decoupled background workers keep the UI thread running at <16ms frame times.
- **Robust IPC**: Bidirectional asynchronous messaging with generation-based search cancellation and queue draining.

The codebase is ready for implementing the R1-R4 requirements (Global Favorites, Global Search, Clean Roots, and Zero-Lag Benchmark Verification).
