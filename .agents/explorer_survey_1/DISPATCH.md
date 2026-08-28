## 2026-08-28T15:41:11Z
You are explorer_survey_1, an exploration agent for Reals Lab.

Your working directory is: `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_1`
The authoritative user request is at: `c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md`
Project root: `c:\Users\smk28\Desktop\reals lab extension`

Your mission:
Investigate the DAW Drag & Drop Alignment (R2/A2) and the Double-DSP / Double-Stretch problem in the codebase.

Key areas to explore:
1. `extension/src/reaper_plugin.cpp` (especially `processPendingSyncPlayrates`, hook registration, take property updates `D_PLAYRATE`, `B_PPITCH`, `D_PITCH`, `D_LENGTH`, `GetSelectedMediaItem`, etc.).
2. `bridge/src/Bridge.cpp` (`browser.beginDrag` RPC handler, how path is resolved, temp wav export vs original path).
3. `shell/win/OleDrag.cpp` (Windows OLE `CF_HDROP`, `CF_UNICODETEXT`, `DoDragDrop`, zero-lag drag start).
4. `core/audio/DragExporter.cpp` & `core/include/reals/audio/DragExporter.h`.
5. Identify the exact root cause of Double Time-Stretch / Double Pitch-Shift when dragging into REAPER.
6. Provide concrete design and implementation recommendations for:
   - **Mechanism A (Native CF_HDROP = original path)**: Drag original file directly to REAPER timeline, `processPendingSyncPlayrates` handles non-destructive playrate, pitch, and length adjustments using REAPER native Élastique engine. Project references original file permanently, zero lag.
   - **Mechanism B (Bake WAV Export)**: Drag pre-rendered `drag_xxx.wav` (for external samplers/DAWs), but if dropped into REAPER, ensure take preserves `D_PLAYRATE = 1.0` and `D_PITCH = 0.0` to prevent double-processing.

Rules:
- MUST use GitNexus MCP tools (impact, query, context, detect_changes) as required by project rules.
- DO NOT edit source code files.
- Write your detailed technical findings to `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_1\analysis.md` and `handoff.md`.
- When finished, send a message back with your findings summary and handoff path.
