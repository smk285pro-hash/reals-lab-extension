# Specification Report: Reals Lab Theme Engine

**Author**: Specification Miner (`spec_miner_survey_1`)  
**Target Project**: Reals Lab REAPER Extension (C++20 DLL + WebView2 UI)  
**Date**: 2026-08-31  
**Status**: Authoritative Specification Complete  

---

## 1. Observation

Direct examination of authoritative documentation and codebase files was conducted:
- `ORIGINAL_REQUEST.md` (Lines 1–47): Defines requirements R1 (CSS Custom Properties & 3 Theme Palettes), R2 (Bidirectional Native Bridge & REAPER ExtState Persistence), R3 (Canvas Waveform & Piano Roll Synchronization), R4 (Build, Deployment, Parallel Verification), and Acceptance Criteria.
- `DESIGN.md` (Lines 1–120): Authoritative Design System covering Visual Direction, Color Tokens, Frameless Window App Shell, Spacing Tokens, Typography Hierarchy, Border Philosophy, Shadows, and 8 Golden Rules.
- `PLAN.md` (Lines 1–397): Documents architectural decisions (WebView2 pivot, C++20 pure, WinHTTP transport, OLE Drag & Drop Mechanism A/B, DSP SoundTouch, REAPER Docker docking, SWS-standard Audio Hook, sub-frame latency alignment, atomic post-build deployment).
- `SPEC.md` (Lines 1–195): Architectural specifications, bridge command definitions, interface contracts, module specifications, and coding standards.
- `AGENTS.md`: Mandatory engineering rules (zero-warning C++20, smart pointers, thread safety, i18n via `tr()`, UTF-8 wide-safe I/O, architectural layer boundaries).
- `tests/suites/TestSuite_ThemeEngine.cpp` (Lines 1–785): Defines the comprehensive 4-Tier test suite (20 test cases) covering ExtState persistence, sanitization/fallbacks, design tokens, IPC string parsing, concurrency, adversarial injection attacks, and session lifecycle.
- `ui-web/tokens.css` (Lines 1–333): Authoritative implementation of 82 CSS custom properties across 3 palettes (`dark-studio`, `pastel-pink`, `cyberpunk`).
- `ui-web/app.css` & `ui-web/index.html`: Inline head script bootstrap, frameless window styling, SVG icon color inheritance (`currentColor`, `var(--accent)`).
- `ui-web/app.js`: `ThemeManager` implementation, IPC handling, `themeUpdated` CustomEvent dispatch, canvas waveform and piano roll rendering.
- `shell/win/WebViewHost.cpp` (Lines 1–372): Win32 WebView2 controller hosting, `put_DefaultBackgroundColor` transparent configuration, `put_IsVisible(FALSE)` pre-warming, virtual host folder mapping (`app.local`).
- `extension/src/reaper_plugin.cpp` (Lines 1–1431): `REAPERAPI_LoadAPI(rec->GetFunc)`, `GetExtState` and `SetExtState` integration, `THEME_CHANGED:` IPC handler, pre-warm on REAPER startup.
- `extension/CMakeLists.txt` & `CMakePresets.json`: Build targets, MSVC compiler options, and atomic post-build deployment script copying `reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins/`.

---

## 2. Logic Chain

1. **Tokens & Theme Consistency**: The design system requires 3 distinct themes (`dark-studio`, `pastel-pink`, `cyberpunk`) with 100% token override parity (82 tokens per theme, 246 definitions total). Each palette must supply valid CSS color representations for surfaces, borders, text, accents, functional badges, canvas waveforms, volume meters, and the piano roll keyboard transposer.
2. **State Hierarchy & Zero-FOUC**: 
   - `GetExtState("REALSLAB", "theme")` is the authoritative persistent store across REAPER sessions (stored in `reaper-extstate.ini`).
   - `localStorage.getItem('reals_theme')` acts as a synchronous, local fast-cache in the `<head>` tag of `index.html` to eliminate white flash before external resources load.
   - `ICoreWebView2Controller2::put_DefaultBackgroundColor({0,0,0,0})` and `put_IsVisible(FALSE)` ensure transparent rendering during background pre-warming.
3. **IPC Protocol Decoupling**: Bidirectional communication uses the plain string format `THEME_CHANGED:<name>`. JS notifies C++ via `window.chrome.webview.postMessage("THEME_CHANGED:<name>")`, and C++ executes `window.themeManager.applyTheme('<name>', false)` or broadcasts via WebMessage. Input sanitization prevents malformed strings, command injections, or oversized payloads from compromising the system.
4. **Canvas Real-Time Synchronization**: `ThemeManager` dispatches a `themeUpdated` event carrying computed CSS color tokens (`--waveform-fill`, `--waveform-fill-active`, `--waveform-bg`). The waveform canvas and piano roll listen for this event and trigger a synchronous re-draw without page reload, DOM disruption, or audio glitching.
5. **Quality & Deployment Automation**: C++20 zero-warning compilation (`/W4 /permissive- /utf-8 /FS`) and automated post-build deployment to `%APPDATA%/REAPER/UserPlugins/` with atomic `.old` swap enable continuous development and parallel testing.

---

## 3. Caveats

- **Web Browser Mock Fallback**: In standalone browser mode (outside REAPER/WebView2), `window.chrome.webview` is undefined. The `ThemeManager` and `Bridge` gracefully fall back to local mock storage (`localStorage` and in-memory stores).
- **Custom Theme Extensions**: While the specification currently freezes 3 official themes, the token structure is completely modular, allowing future themes or user custom themes to be added by defining matching CSS custom properties.
- **High-DPI Canvas Scaling**: Canvas elements (`#waveform`, `#meter`) must check `devicePixelRatio` and element client bounding rects during redraws to preserve pixel sharpness across 100%, 125%, 150%, and 200% Windows display scaling.

---

## 4. Conclusion

The Reals Lab Theme Engine specification is completely defined, validated, and aligned across documentation, C++ test infrastructure, native shell controllers, and frontend CSS/JS. All 5 core areas have exact interface contracts, token schemas, IPC string formats, and lifecycle rules ready for development and verification.

---

## 5. Verification Method

- **Automated C++ Test Suite**:
  ```powershell
  cmake --preset windows
  cmake --build --preset windows --target reals_tests
  ctest --preset windows --output-on-failure
  ```
- **Token Parity Validation**:
  ```powershell
  python tests/verify_tokens_test.py
  ```
- **Live REAPER Deployment & Inspection**:
  - Build `reaper_realslab` target.
  - Verify `reaper_realslab.dll` is deployed to `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`.
  - Open REAPER, trigger action `Reals Lab: Show Window`, switch themes via Settings, and verify persistence in `%APPDATA%\REAPER\reaper-extstate.ini`.

---

# Detailed Technical Specification

```
================================================================================
REALS LAB THEME ENGINE — COMPLETE SYSTEM SPECIFICATION
================================================================================
```

## Section 1: Design Tokens & Theme Palettes (R1)

### 1.1 Token Architecture & Semantic Conventions
All UI styles are declared via CSS Custom Properties. The base tokens are defined in `:root, html[data-theme="dark-studio"]` and fully overridden in `html[data-theme="pastel-pink"]` and `html[data-theme="cyberpunk"]`.

**Naming Convention Matrix:**
- Surfaces & Backgrounds: `--bg-*`
- Borders & Dividers: `--border-*`
- Typography: `--text-*`
- Accent & Interaction: `--accent-*`
- Functional Status Badges: `--free-*`, `--pro-*`, `--upd-*`, `--badge-*`, `--danger-*`
- Waveform & Audio Display: `--waveform-*`
- Audio Level Meter: `--meter-*`
- Piano Roll & Transposer: `--pianoroll-*`
- Mini List Waveform: `--mini-wave-*`
- Shadows & Effects: `--shadow-*`
- Transitions & Focus: `--t-*`, `--focus-ring`

---

### 1.2 The 82 Design Tokens Master Matrix (All 3 Palettes)

| # | Token Name | Category | `dark-studio` (Default) | `pastel-pink` (Cutecore) | `cyberpunk` (Neon) |
|---|---|---|---|---|---|
| 1 | `--bg-root` | Surface | `#090A0C` | `#FFF0F5` | `#040407` |
| 2 | `--bg-app` | Surface | `#0D0E11` | `#FFF5F8` | `#08080E` |
| 3 | `--bg-sidebar` | Surface | `#101114` | `#FFEBF2` | `#0A0A12` |
| 4 | `--bg-panel` | Surface | `#121316` | `#FFF8FA` | `#0B0B14` |
| 5 | `--bg-card` | Surface | `#15171A` | `#FFFFFF` | `#0F101A` |
| 6 | `--bg-card-hover` | Surface | `#191B1F` | `#FFF0F6` | `#161726` |
| 7 | `--bg-input` | Surface | `#0D0F12` | `#FFFFFF` | `#07070D` |
| 8 | `--bg-input-search` | Surface | `rgba(0, 0, 0, 0.30)` | `rgba(255, 255, 255, 0.90)` | `rgba(7, 7, 13, 0.85)` |
| 9 | `--bg-input-focus` | Surface | `rgba(0, 0, 0, 0.50)` | `#FFFFFF` | `#0A0B16` |
| 10 | `--bg-elevated` | Surface | `#1A1C20` | `#FFE4EE` | `#151624` |
| 11 | `--bg-nav-active` | Surface | `#17191C` | `#FFD9E8` | `#1A1C30` |
| 12 | `--bg-hover-subtle` | Surface | `rgba(255, 255, 255, 0.05)` | `rgba(255, 64, 129, 0.06)` | `rgba(0, 240, 255, 0.08)` |
| 13 | `--bg-selected` | Surface | `rgba(255, 255, 255, 0.09)` | `rgba(255, 64, 129, 0.14)` | `rgba(0, 240, 255, 0.18)` |
| 14 | `--bg-time-badge` | Surface | `rgba(0, 0, 0, 0.30)` | `rgba(255, 228, 238, 0.90)` | `rgba(15, 16, 26, 0.85)` |
| 15 | `--modal-backdrop` | Surface | `rgba(0, 0, 0, 0.65)` | `rgba(46, 24, 36, 0.45)` | `rgba(4, 4, 7, 0.80)` |
| 16 | `--drop-overlay-bg` | Surface | `rgba(9, 10, 12, 0.85)` | `rgba(255, 240, 245, 0.88)` | `rgba(4, 4, 7, 0.90)` |
| 17 | `--border-subtle` | Border | `#24262B` | `#F3D0DF` | `#1E2038` |
| 18 | `--border-default` | Border | `#2C2F35` | `#ECC4D5` | `#282B4D` |
| 19 | `--border-strong` | Border | `#363941` | `#DFB0C5` | `#383C6E` |
| 20 | `--border-card` | Border | `#24272C` | `#F0D5E2` | `#20233D` |
| 21 | `--border-input` | Border | `#292C31` | `#E8C2D3` | `#2A2D52` |
| 22 | `--border-chip` | Border | `#272A30` | `#ECC8D8` | `#242745` |
| 23 | `--border-strong-2` | Border | `#30333A` | `#D8A2BA` | `#3E437A` |
| 24 | `--text-primary` | Typography | `#F2F3F5` | `#2E1824` | `#F0F4FF` |
| 25 | `--text-secondary` | Typography | `#A3A6AD` | `#6B4C5D` | `#8A95C7` |
| 26 | `--text-tertiary` | Typography | `#737780` | `#947184` | `#5B6699` |
| 27 | `--text-disabled` | Typography | `#6A6D75` | `#B59AA8` | `#414A73` |
| 28 | `--text-icon` | Typography | `#858991` | `#8C657A` | `#00F0FF` |
| 29 | `--text-meta` | Typography | `#777B84` | `#805B6F` | `#7480B8` |
| 30 | `--text-chip` | Typography | `#8F939B` | `#5E3F50` | `#A0ABDE` |
| 31 | `--text-secondary-strong` | Typography | `#D7D9DD` | `#402234` | `#CBD3F7` |
| 32 | `--text-primary-strong` | Typography | `#F1F2F4` | `#1F0E17` | `#FFFFFF` |
| 33 | `--accent` | Accent | `#FF6B2C` | `#FF4081` | `#00F0FF` |
| 34 | `--accent-hover` | Accent | `#FF7A3D` | `#FF6097` | `#33F3FF` |
| 35 | `--accent-active` | Accent | `#E9571D` | `#E02868` | `#00C4D1` |
| 36 | `--accent-soft` | Accent | `rgba(255, 107, 44, 0.12)` | `rgba(255, 64, 129, 0.12)` | `rgba(0, 240, 255, 0.15)` |
| 37 | `--accent-border` | Accent | `rgba(255, 107, 44, 0.35)` | `rgba(255, 64, 129, 0.35)` | `rgba(0, 240, 255, 0.45)` |
| 38 | `--accent-focus` | Accent | `rgba(255, 107, 44, 0.55)` | `rgba(255, 64, 129, 0.55)` | `rgba(0, 240, 255, 0.70)` |
| 39 | `--accent-glow` | Accent | `rgba(255, 107, 44, 0.08)` | `rgba(255, 64, 129, 0.08)` | `rgba(0, 240, 255, 0.20)` |
| 40 | `--accent-contrast` | Accent | `#FFFFFF` | `#FFFFFF` | `#040407` |
| 41 | `--free-bg` | Badge | `rgba(34, 197, 94, 0.12)` | `rgba(16, 185, 129, 0.12)` | `rgba(0, 255, 102, 0.15)` |
| 42 | `--free-tx` | Badge | `#35D07F` | `#059669` | `#00FF66` |
| 43 | `--pro-bg` | Badge | `rgba(255, 107, 44, 0.12)` | `rgba(255, 64, 129, 0.12)` | `rgba(255, 0, 85, 0.15)` |
| 44 | `--pro-tx` | Badge | `#FF7A3D` | `#E02868` | `#FF0055` |
| 45 | `--upd-bg` | Badge | `rgba(59, 130, 246, 0.12)` | `rgba(139, 92, 246, 0.12)` | `rgba(0, 240, 255, 0.15)` |
| 46 | `--upd-tx` | Badge | `#55A5FF` | `#7C3AED` | `#00F0FF` |
| 47 | `--badge-midi-bg` | Badge | `rgba(167, 139, 250, 0.14)` | `rgba(139, 92, 246, 0.14)` | `rgba(255, 0, 85, 0.15)` |
| 48 | `--badge-midi-tx` | Badge | `#A78BFA` | `#7C3AED` | `#FF0055` |
| 49 | `--danger` | Badge | `#FF5C66` | `#E11D48` | `#FF0055` |
| 50 | `--danger-soft` | Badge | `rgba(255, 92, 102, 0.15)` | `rgba(225, 29, 72, 0.15)` | `rgba(255, 0, 85, 0.20)` |
| 51 | `--waveform-bg` | Waveform | `#0B0E14` | `#FFEBF2` | `#05050C` |
| 52 | `--waveform-fill` | Waveform | `rgba(255, 255, 255, 0.12)` | `rgba(148, 113, 132, 0.35)` | `rgba(0, 240, 255, 0.18)` |
| 53 | `--waveform-fill-active`| Waveform | `rgba(56, 189, 248, 0.75)` | `#FF4081` | `#00F0FF` |
| 54 | `--waveform-playhead` | Waveform | `rgba(255, 255, 255, 0.85)` | `#2E1824` | `#FF0055` |
| 55 | `--waveform-centerline`| Waveform | `rgba(255, 255, 255, 0.05)` | `rgba(223, 176, 197, 0.60)` | `rgba(0, 240, 255, 0.25)` |
| 56 | `--meter-bg` | Meter | `#0B0E14` | `#FFEBF2` | `#05050C` |
| 57 | `--meter-fill` | Meter | `#35D07F` | `#059669` | `#00FF66` |
| 58 | `--meter-fill-warn` | Meter | `#F59E0B` | `#D97706` | `#FFE600` |
| 59 | `--meter-fill-clip` | Meter | `#FF5C66` | `#E11D48` | `#FF0055` |
| 60 | `--pianoroll-bg` | Piano Roll | `#0B0E14` | `#FFEBF2` | `#05050C` |
| 61 | `--pianoroll-grid` | Piano Roll | `rgba(255, 255, 255, 0.04)` | `rgba(223, 176, 197, 0.35)` | `rgba(0, 240, 255, 0.08)` |
| 62 | `--pianoroll-note` | Piano Roll | `#38BDF8` | `#FF4081` | `#00F0FF` |
| 63 | `--pianoroll-note-active`| Piano Roll | `#FFFFFF` | `#FFFFFF` | `#FFFFFF` |
| 64 | `--pianoroll-note-grad-end`| Piano Roll | `#0284C7` | `#F43F5E` | `#FF0055` |
| 65 | `--pianoroll-key-white-bg`| Piano Roll | `#E2E4E9` | `#FFFFFF` | `#1B1E33` |
| 66 | `--pianoroll-key-white-tx`| Piano Roll | `#1E2024` | `#2E1824` | `#00F0FF` |
| 67 | `--pianoroll-key-white-hover`| Piano Roll| `#FFFFFF` | `#FFF0F5` | `#262B47` |
| 68 | `--pianoroll-key-black-bg`| Piano Roll | `#181A1F` | `#805B6F` | `#080911` |
| 69 | `--pianoroll-key-black-tx`| Piano Roll | `#8F939B` | `#FFFFFF` | `#8A95C7` |
| 70 | `--pianoroll-key-black-hover`| Piano Roll| `#2D3038` | `#634053` | `#121422` |
| 71 | `--pianoroll-key-active-bg`| Piano Roll | `#38BDF8` | `#FF4081` | `#00F0FF` |
| 72 | `--pianoroll-key-active-tx`| Piano Roll | `#0A0D14` | `#FFFFFF` | `#040407` |
| 73 | `--pianoroll-root-marker`| Piano Roll | `#F59E0B` | `#D97706` | `#FFE600` |
| 74 | `--mini-wave-color` | Mini Wave | `rgba(56, 189, 248, 0.40)` | `rgba(255, 64, 129, 0.45)` | `rgba(0, 240, 255, 0.45)` |
| 75 | `--mini-wave-hover` | Mini Wave | `rgba(56, 189, 248, 0.70)` | `rgba(255, 64, 129, 0.75)` | `rgba(0, 240, 255, 0.80)` |
| 76 | `--mini-wave-sel` | Mini Wave | `#38BDF8` | `#FF4081` | `#00F0FF` |
| 77 | `--shadow-modal` | Shadow | `0 16px 48px rgba(0, 0, 0, 0.70)` | `0 16px 48px rgba(148, 113, 132, 0.25)` | `0 16px 48px rgba(0, 0, 0, 0.90)` |
| 78 | `--shadow-pop` | Shadow | `0 12px 32px rgba(0, 0, 0, 0.60)` | `0 12px 32px rgba(148, 113, 132, 0.20)` | `0 12px 32px rgba(0, 0, 0, 0.80)` |
| 79 | `--shadow-text-glow` | Shadow | `0 1px 4px rgba(0,0,0,0.95), 0 0 12px rgba(18,21,27,0.90)` | `none` | `0 1px 4px rgba(0,0,0,0.95), 0 0 12px rgba(0,240,255,0.35)` |
| 80 | `--t-fast` | System | `120ms ease` | `120ms ease` | `120ms ease` |
| 81 | `--t-med` | System | `160ms ease` | `160ms ease` | `160ms ease` |
| 82 | `--focus-ring` | System | `0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus)` | `0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus)` | `0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus)` |

---

### 1.3 Typography, Spacing, and Vector Icon Rules
- **Typography Scale**:
  - Font Family: `'Inter', 'Segoe UI', system-ui, sans-serif` (Embedded WOFF2 with Latin, Vietnamese, Latin-Ext support).
  - App Header Title: 16–18px / 700
  - Section Headings: 12px / 600, `letter-spacing: 1.5px`, uppercase
  - Plugin & Item Titles: 15–16px / 600
  - Body Text: 13–14px / 400
  - Metadata & Badges: 11–12px / 400–600
- **Spacing System**: Base 4px grid (`4 / 8 / 12 / 16 / 20 / 24 / 32 / 40 / 48px`).
  - Card Padding: 14–18px; Card Gap: 10–12px; Main Content Margin: 24px.
- **Vector SVG Icons**:
  - Zero hardcoded colors in SVG files.
  - Icons inherit parent text color via `fill="none"` and `stroke="currentColor"` or `fill="currentColor"`.
  - Action/Active states apply `color: var(--accent)` or `color: var(--text-icon)`.

---

## Section 2: Native REAPER Bridge & Persistence Spec (R2)

### 2.1 REAPER SDK Integration (`GetFunc`)
- Extension dynamically imports REAPER API pointers via `REAPERAPI_LoadAPI(rec->GetFunc)` during `reaper_plugin_entrypoint`.
- Key symbols required:
  - `GetExtState`: `const char* GetExtState(const char* section, const char* key)`
  - `SetExtState`: `void SetExtState(const char* section, const char* key, const char* val, bool persist)`

### 2.2 Section & Key Names
- **Section**: `"REALSLAB"`
- **Key**: `"theme"`
- **Persist Flag**: `true` (forces REAPER to serialize value to `%APPDATA%/REAPER/reaper-extstate.ini` under `[REALSLAB] theme=<name>`).

### 2.3 IPC Protocol Specification
1. **Frontend to Native (JS -> C++)**:
   - Web message posted: `window.chrome.webview.postMessage("THEME_CHANGED:<name>")`
   - C++ WebMessage handler in `reaper_plugin.cpp`:
     ```cpp
     constexpr std::string_view kThemePrefix = "THEME_CHANGED:";
     if (msg.rfind(kThemePrefix, 0) == 0) {
         std::string themeName = msg.substr(kThemePrefix.length());
         themeName = sanitizeTheme(themeName);
         if (SetExtState)
             SetExtState("REALSLAB", "theme", themeName.c_str(), true);
         reals::config::Config::instance().set("theme", themeName);
     }
     ```
2. **Native to Frontend (C++ -> JS)**:
   - Initial push and window activations execute:
     `ExecuteScript(L"window.themeManager && window.themeManager.applyTheme('" + toWide(theme) + L"', false);")`
   - Secondary push: WebMessage string `THEME_CHANGED:<name>`.

### 2.4 Sanitization & Fallback Sequence
```
               ┌──────────────────────────────┐
               │ REAPER Start / Plugin Load   │
               └──────────────┬───────────────┘
                              │
               ┌──────────────▼───────────────┐
               │ Query GetExtState("REALSLAB",│
               │                   "theme")   │
               └──────────────┬───────────────┘
                              │
                    Exists? ──┴── No ──┐
                   Yes                 │
                    │                  ▼
                    │      ┌─────────────────────────┐
                    │      │ Config::getString()     │
                    │      └───────────┬─────────────┘
                    │                  │
                    ▼                  ▼
               ┌──────────────────────────────┐
               │ sanitizeTheme(rawTheme)      │
               │ 1. Trim whitespace & nulls   │
               │ 2. Convert to lowercase      │
               │ 3. Check in kValidThemes:    │
               │    - dark-studio             │
               │    - pastel-pink             │
               │    - cyberpunk               │
               │ (Reject SQLi, XSS, garbage)  │
               └──────────────┬───────────────┘
                              │
                  Invalid? ───┴─── Valid?
                     │                │
                     ▼                ▼
             ┌──────────────┐ ┌───────────────┐
             │ Fallback to: │ │ Keep Verified │
             │"dark-studio" │ │  Theme Name   │
             └───────┬──────┘ └───────┬───────┘
                     │                │
                     └────────┬───────┘
                              ▼
               ┌──────────────────────────────┐
               │ Push to WebView2 via Script: │
               │ applyTheme(theme, false)     │
               └──────────────────────────────┘
```

---

## Section 3: Zero-FOUC & WebView2 Initialization Contract (R3)

To ensure an instant, flicker-free desktop application experience without any white flash or layout shifts:

1. **Win32 Window Frame**:
   - Host window background brush is set to `CreateSolidBrush(RGB(13, 14, 17))` (`#0D0E11`).
   - Frameless setup via `DwmExtendFrameIntoClientArea` and dark title bar attribute `DWMWA_USE_IMMERSIVE_DARK_MODE`.
2. **WebView2 Controller Transparency**:
   - `ICoreWebView2Controller2::put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{0, 0, 0, 0})` is called immediately upon controller creation to enforce 100% alpha transparency.
3. **Visibility Gating**:
   - Controller is initialized with `put_IsVisible(FALSE)` during pre-warming.
   - Visibility is set to `TRUE` only after the host window displays and navigation completes.
4. **Synchronous Inline `<head>` Bootstrap**:
   - An inline script at line 7 of `index.html` executes synchronously before CSS parsing:
     ```html
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
     ```
   - Prevents CSS re-computation and visual flash on page load.
5. **Background Pre-Warming**:
   - `createHostWindow(false)` runs in the background on REAPER load (`reaper_plugin_entrypoint`), warming the Edge WebView2 runtime process before the user ever clicks "Show Window".

---

## Section 4: Canvas Waveform & Piano Roll Contract (R4)

### 4.1 `themeUpdated` CustomEvent Contract
When `ThemeManager.applyTheme(name)` runs:
1. `document.documentElement.setAttribute('data-theme', name)` updates active CSS tokens.
2. `ThemeManager` extracts computed styles from `document.documentElement`.
3. Dispatches a window-level `CustomEvent('themeUpdated', { detail: { theme: name, tokens } })`.

### 4.2 Dynamic Color Token Mapping for Canvases
- **Audio Waveform Canvas (`#waveform`)**:
  - Background: `var(--waveform-bg)`
  - Centerline: `var(--waveform-centerline)`
  - Unplayed Bars: `var(--waveform-fill)`
  - Played Progress Bars: `var(--waveform-fill-active)`
  - Playhead Cursor: `var(--waveform-playhead)`
- **Piano Roll MIDI Canvas (`#waveform` in MIDI mode)**:
  - Canvas Background: `var(--pianoroll-bg)`
  - Grid Gridlines: `var(--pianoroll-grid)`
  - Note Gradients: `linear-gradient(var(--pianoroll-note), var(--pianoroll-note-grad-end))`
  - Active Sounding Note: `linear-gradient(var(--pianoroll-note-active), var(--pianoroll-note))`
  - Playhead Line: `var(--waveform-playhead)`
- **Audio Level Meter Canvas (`#meter`)**:
  - Background: `var(--meter-bg)`
  - Normal Level (0..-12dB): `var(--meter-fill)`
  - Warning Level (-12..-3dB): `var(--meter-fill-warn)`
  - Clipping Peak (>-3dB..0dB): `var(--meter-fill-clip)`

### 4.3 Redraw Lifecycle
- Event listener: `window.addEventListener('themeUpdated', () => { drawWaveform(); drawMeterSmoothed(0); paintVisible(); });`
- Execution latency: < 16ms (single animation frame).
- Zero audio interruption: SoundTouch DSP and miniaudio threads continue unaffected without buffer reset or playback hitching.

---

## Section 5: C++20, Build Targets, Presets, and Deployment Pipeline (R5)

### 5.1 Compilation Constraints
- Standard: C++20 (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`).
- Compiler Flags:
  - MSVC: `/W4 /permissive- /utf-8 /FS` (zero warning policy).
  - GCC/Clang: `-Wall -Wextra -Wpedantic`.

### 5.2 Build Targets
1. `reals_core` (STATIC): Business logic, DSP engines, database scanner, AI models, HTTP client.
2. `reals_bridge` (STATIC): JSON dispatcher between JS and C++.
3. `reals_shell_win` (STATIC): `WebViewHost.cpp`, `OleDrag.cpp`.
4. `reaper_realslab` (SHARED / DLL): REAPER plugin entrypoint, audio hook, docking, native window manager.
5. `reals_tests` (EXECUTABLE): Consolidated test runner covering all suites.

### 5.3 CMake Presets & Deployment Pipeline
- Configuration Preset: `windows` (VS 2022 x64, `REALS_BUILD_APP=OFF`, `REALS_BUILD_EXTENSION=ON`).
- Post-Build Automated Deployment (`extension/CMakeLists.txt`):
  ```powershell
  # Creates directory if needed
  make_directory "$ENV{APPDATA}/REAPER/UserPlugins"
  # Safely handles locked DLLs via atomic .old rename
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Get-ChildItem '$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll*.old' -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue; if (Test-Path '$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll') { Move-Item -Force '$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll' ('$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll.' + (Get-Random) + '.old') -ErrorAction SilentlyContinue }; Copy-Item -Force '$<TARGET_FILE:reaper_realslab>' '$ENV{APPDATA}/REAPER/UserPlugins/reaper_realslab.dll'"
  ```

---

# Features Discovered & Edge Cases Tables

## Features Discovered

| # | Category | Feature | Description | Inputs | Outputs | Error Behavior | Discovered Via |
|---|---|---|---|---|---|---|---|
| 1 | Tokens | 82 Design Tokens Matrix | Complete CSS custom properties covering surfaces, borders, text, accents, badges, waveforms, meters, and piano roll | CSS Variables | Dynamic UI Styling | Fallback to `:root` defaults if undefined | `ui-web/tokens.css`, `DESIGN.md` |
| 2 | Tokens | 3 Official Palettes | `dark-studio`, `pastel-pink`, `cyberpunk` with 100% token override parity | `data-theme` attribute | Theme-specific palette | Reverts to `dark-studio` | `tokens.css`, `verify_tokens_test.py` |
| 3 | Tokens | Dynamic SVG Colors | Vector SVGs adapt color via `currentColor` and accent tokens | CSS text color / vars | Vector stroke/fill | Falls back to inherited text color | `index.html`, `app.css` |
| 4 | Persistence | `SetExtState` Persistence | Persists selected theme to `reaper-extstate.ini` | `("REALSLAB", "theme", name, true)` | INI state saved | Ignored if `SetExtState` is null | `reaper_plugin.cpp`, `TestSuite_ThemeEngine.cpp` |
| 5 | Persistence | `GetExtState` Recovery | Reads saved theme on REAPER startup across sessions | `("REALSLAB", "theme")` | Saved theme string | Falls back to `Config` or `dark-studio` | `reaper_plugin.cpp`, `TestSuite_ThemeEngine.cpp` |
| 6 | Persistence | Config JSON Secondary Store | Local config backup in `%APPDATA%/RealsLab/config.json` | Key `"theme"`, string | JSON persistence | Returns default if file missing | `core/config/Config.h` |
| 7 | IPC | Bidirectional Protocol | Plain string IPC protocol `THEME_CHANGED:<name>` | `THEME_CHANGED:<name>` | IPC acknowledgment & state sync | Non-matching prefix ignored | `app.js`, `reaper_plugin.cpp` |
| 8 | IPC | Direct Script Push | Native C++ executes theme change script in WebView2 | Theme name string | JavaScript function call | No-op if `window.themeManager` missing | `reaper_plugin.cpp` |
| 9 | FOUC | Head Inline Bootstrap | Synchronous `localStorage` check in `<head>` | `localStorage.reals_theme` | Immediate `data-theme` set | Defaults to `dark-studio` on exception | `index.html` |
| 10 | FOUC | WebView2 Transparency | Sets transparent background color on controller | `COREWEBVIEW2_COLOR{0,0,0,0}` | Transparent initial canvas | Handled via COM HRESULT | `WebViewHost.cpp` |
| 11 | FOUC | Visibility Gating | Keeps controller hidden during pre-warming | `put_IsVisible(FALSE)` | Zero flash on startup | Logged if controller fails | `WebViewHost.cpp` |
| 12 | Canvas | Waveform Live Sync | Immediate canvas redraw with active theme colors | `themeUpdated` CustomEvent | Re-rendered waveform | Redraws on next RAF if busy | `app.js` |
| 13 | Canvas | Piano Roll MIDI Sync | MIDI canvas note gradients adapt to theme | `themeUpdated` CustomEvent | Re-rendered piano roll | Fallbacks to default notes | `app.js` |
| 14 | Canvas | Level Meter Sync | VU meter threshold colors adapt to theme | `themeUpdated` CustomEvent | Re-rendered VU meter | Default green/warn/clip | `app.js` |
| 15 | UI | Settings Theme Picker | User-facing UI chips in Settings modal to pick theme | Click on theme chip | Theme switched + persisted | Disallowed values rejected | `index.html`, `app.js` |
| 16 | Shell | Virtual Host Mapping | Maps `ui-web/` folder to `https://app.local` | Source folder path | Virtual HTTP origin | Returns HRESULT error on failure | `WebViewHost.cpp` |
| 17 | Shell | Cache Invalidation Stamp | Keeps disk cache between sessions, wipes only on UI change | File timestamps stamp | Preserved / cleared cache | Wipes on stamp mismatch | `WebViewHost.cpp` |
| 18 | Deploy | Atomic Post-Build Deploy | PowerShell deployment with `.old` rename for running REAPER | DLL build artifact | UserPlugins DLL replacement | Continues on permission error | `extension/CMakeLists.txt` |

---

## Edge Cases

| # | Feature | Input | Observed Behavior |
|---|---|---|---|
| 1 | Fallback | Empty string `""` | Sanitized to `"dark-studio"`, `SetExtState` and `Config` retain valid default |
| 2 | Fallback | Unknown theme name `"solarized-light"` | Sanitized to `"dark-studio"`, avoids missing CSS tokens |
| 3 | Fallback | Oversized string (4KB garbage payload) | Rejected by `sanitizeTheme`, safely clamped to `"dark-studio"` |
| 4 | Fallback | Control characters & null bytes (`"pastel-pink\0\r\n"`) | Whitespace/null trimmed, correctly sanitized to `"pastel-pink"` |
| 5 | Security | SQLi / XSS Injection (`"' OR '1'='1"`, `<script>`) | Rejected by theme validator, defaults to `"dark-studio"` |
| 6 | Normalization | Mixed case input (`"PaStEl-PiNk"`) | Converted to lowercase `"pastel-pink"` and applied normally |
| 7 | Normalization | Unicode / Emoji names (`"🌸pastel-pink🌸"`) | Non-ASCII matches rejected, safely falls back to `"dark-studio"` |
| 8 | Concurrency | 5 concurrent reader/writer threads | Zero data race or memory corruption; atomic state updates |
| 9 | Rapid Switching | 100 rapid oscillations in tight loop | UI updates synchronously, `reaper-extstate.ini` records final state |
| 10 | Standalone | Browser mock mode (No WebView2) | Uses `localStorage` gracefully, ignores missing `window.chrome.webview` |
| 11 | Crash Recovery | Corrupt `.ini` entry (`"theme=broken\n["`) | Sanitized during startup load, cleanly recovers to `"dark-studio"` |
| 12 | High-DPI | Window dragged to 150% DPI monitor | Canvas auto-resizes using `clientWidth * dpr`, preserving sharp waveform bars |
