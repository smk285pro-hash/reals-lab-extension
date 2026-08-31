# BRIEFING — 2026-09-01T01:23:25+07:00

## Mission
Explore and analyze the UI frontend and IPC bridge of Reals Lab REAPER Extension (Webview/UI structure, Favorites UI, Search UI & filters, 60 FPS list rendering & audio waveform probing, and IPC message flow).

## 🔒 My Identity
- Archetype: explorer
- Roles: UI & Frontend/IPC Specialist (Explorer 2)
- Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_ui\
- Original parent: 2ece16d4-00dc-45ee-99bd-9c7cc471f29b
- Milestone: Explorer Survey

## 🔒 Key Constraints
- Read-only investigation — do NOT implement or modify project source code
- Always use GitNexus MCP tools for code intelligence
- Follow AGENTS.md, PLAN.md, SPEC.md, DESIGN.md specifications
- All UI text must not be hardcoded (i18n)

## Current Parent
- Conversation ID: 2ece16d4-00dc-45ee-99bd-9c7cc471f29b
- Updated: 2026-09-01T01:23:25+07:00

## Investigation State
- **Explored paths**: `ui-web/index.html`, `ui-web/app.js`, `ui-web/app.css`, `ui-web/tokens.css`, `bridge/include/reals/bridge/Bridge.h`, `bridge/src/Bridge.cpp`, `shell/win/WebViewHost.h`, `shell/win/WebViewHost.cpp`, `extension/src/reaper_plugin.cpp`, `core/include/reals/browser/BrowserModel.h`, `core/src/browser/BrowserModel.cpp`, `core/include/reals/search/QueryParser.h`, `core/src/search/QueryParser.cpp`, `core/include/reals/search/SearchEngine.h`, `core/src/search/SearchEngine.cpp`.
- **Key findings**:
  1. UI is 100% offline HTML/JS/CSS rendered via WebView2 virtual host mapping.
  2. Favorites `#favOnly` currently filters locally loaded folder files; requires global `FileEntry` resolution for R1.
  3. Search `#search` uses 200ms debouncing, `/syntax` parsing, generation token cancellation, and scroll position restoration; requires empty `base` for global multi-root search (R2).
  4. 60 FPS rendering is achieved via DOM virtual scrolling (`VIRT_OVERSCAN = 8`), dynamic row height, pointerdown event delegation, 100ms debounced probing (max 16 files), and background worker envelope extraction.
  5. IPC bridge is a JSON-RPC protocol over `postMessage` with event queues drained every 30ms on REAPER `timerHook`.
  6. Hardcoded default roots (`Music`, `Desktop`, `Downloads`) are in `BrowserModel.cpp:155-166` and can be cleanly removed for R3.
- **Unexplored areas**: None for UI and IPC scope.

## Key Decisions Made
- Completed deep code survey across all 5 assigned areas.
- Documented detailed findings in `analysis.md` and synthesized summary in `handoff.md`.

## Artifact Index
- `DISPATCH.md` — Task dispatch log
- `BRIEFING.md` — Persistent working memory
- `progress.md` — Heartbeat progress
- `analysis.md` — Comprehensive survey analysis report
- `handoff.md` — 5-component handoff summary
