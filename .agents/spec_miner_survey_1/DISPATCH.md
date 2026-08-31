## 2026-08-31T15:16:58Z
You are a Specification Miner for the Reals Lab Theme Engine project.
Working directory: c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1

Mandatory Input Files:
- c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
- c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
- c:\Users\smk28\Desktop\reals lab extension\PLAN.md
- c:\Users\smk28\Desktop\reals lab extension\DESIGN.md
- c:\Users\smk28\Desktop\reals lab extension\SPEC.md

Objective:
Mine and document the authoritative specification and requirements for the Theme Engine across all documentation files and user requests. Specifically:
1. Design tokens & theme specifications: detail all required color tokens, semantic naming conventions, 3 palettes (`dark-studio`, `pastel-pink`, `cyberpunk`), typography, spacing, SVG icon color rules (`currentColor`, `var(--accent-primary)`).
2. Native REAPER Bridge & Persistence spec: `GetFunc`, `SetExtState(section, key, value, persist=true)`, `GetExtState(section, key)`, exact section/key names, IPC protocol string format (`THEME_CHANGED:<name>`), fallback/bootstrap sequence.
3. Zero-FOUC & WebView2 initialization contract: background transparency, visibility toggles, DOM ready handshake, inline head script bootstrap.
4. Canvas Waveform & Piano Roll contract: `themeUpdated` CustomEvent payload, token mapping for waveforms (`--waveform-fill`, `--waveform-fill-active`, `--waveform-bg`), redraw triggers.
5. Zero-warning C++20 constraints, build targets, cmake presets, and deployment requirements (`reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins/`).

Write your detailed findings and feature inventory to `c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1\handoff.md`.
Use `send_message` to notify the parent when completed with the path to your handoff file.
