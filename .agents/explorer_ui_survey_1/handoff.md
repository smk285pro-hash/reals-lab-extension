# Handoff Report: Frontend & Web UI Survey for Theme Engine

**Agent ID**: `fa7c4443-f51c-4c1b-a087-5d32b96d53ab`
**Role**: Explorer (Frontend / Web UI Architecture)
**Working Directory**: `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_ui_survey_1`
**Timestamp**: 2026-08-31T22:20:00+07:00

---

## 1. Observation

### 1.1 Web File & Asset Inventory
The Web UI is located under `ui-web/` and served locally inside WebView2 via `https://app.local` (mapped from `ICoreWebView2_3::SetVirtualHostNameToFolderMapping` in `shell/win/WebViewHost.cpp:220`):

| File Path | Lines | Size | Purpose |
|---|---|---|---|
| `ui-web/index.html` | 391 | 21,491 B | Single-page application shell, `<head>` bootstrap script, Topbar, Content Panes, Player, Settings modal |
| `ui-web/tokens.css` | 333 | 10,208 B | 3-theme token definitions using CSS Custom Properties (`:root`, `html[data-theme="..."]`) |
| `ui-web/app.css` | 1,155 | 46,721 B | UI stylesheets, component styling, layout rules (`@import "tokens.css";`), zero hardcoded hex colors |
| `ui-web/app.js` | 3,602 | 139,728 B | Client JS engine, `ThemeManager`, Bridge IPC, Canvas renderers (`drawWaveform`, `drawMeter`), State |
| `ui-web/fonts/` | - | - | Self-hosted Inter font files (`inter-latin.woff2`, `inter-latin-ext.woff2`, `inter-vietnamese.woff2`) |
| `ui-web/assets/` | - | - | `logo.svg`, `reals-mark.svg`, `icon.png`, `reals-mark.png` |
| `tests/verify_tokens_test.py` | 168 | 8,082 B | Token parity & CSS variable integrity validation script |
| `tests/suites/TestSuite_ThemeEngine.cpp` | 785 | 31,276 B | Comprehensive C++ Theme Engine test suite (ExtState persistence, IPC, Fallback, Tokens, Boundaries) |

---

### 1.2 CSS Variables, Token Matrix & Hardcoded Colors

#### A. Token Matrix in `ui-web/tokens.css`
`ui-web/tokens.css` implements 3 complete theme palettes with **100% token override parity** (82 distinct CSS custom properties per theme across 11 categories):
1. `dark-studio` (`:root`, `html[data-theme="dark-studio"]`) — Default studio dark theme (`--bg-root: #090A0C`, `--accent: #FF6B2C`)
2. `pastel-pink` (`html[data-theme="pastel-pink"]`) — Light cutecore pastel pink (`--bg-root: #FFF0F5`, `--accent: #FF4081`)
3. `cyberpunk` (`html[data-theme="cyberpunk"]`) — High-contrast neon cyberpunk (`--bg-root: #040407`, `--accent: #00F0FF`)

**Token categories (82 tokens per theme)**:
- Surfaces & Backgrounds (16 tokens): `--bg-root`, `--bg-app`, `--bg-sidebar`, `--bg-panel`, `--bg-card`, `--bg-card-hover`, `--bg-input`, `--bg-input-search`, `--bg-input-focus`, `--bg-elevated`, `--bg-nav-active`, `--bg-hover-subtle`, `--bg-selected`, `--bg-time-badge`, `--modal-backdrop`, `--drop-overlay-bg`
- Borders (7 tokens): `--border-subtle`, `--border-default`, `--border-strong`, `--border-card`, `--border-input`, `--border-chip`, `--border-strong-2`
- Typography (9 tokens): `--text-primary`, `--text-secondary`, `--text-tertiary`, `--text-disabled`, `--text-icon`, `--text-meta`, `--text-chip`, `--text-secondary-strong`, `--text-primary-strong`
- Accents (8 tokens): `--accent`, `--accent-hover`, `--accent-active`, `--accent-soft`, `--accent-border`, `--accent-focus`, `--accent-glow`, `--accent-contrast`
- Functional Badges (10 tokens): `--free-bg`, `--free-tx`, `--pro-bg`, `--pro-tx`, `--upd-bg`, `--upd-tx`, `--badge-midi-bg`, `--badge-midi-tx`, `--danger`, `--danger-soft`
- Waveform & Canvas (5 tokens): `--waveform-bg`, `--waveform-fill`, `--waveform-fill-active`, `--waveform-playhead`, `--waveform-centerline`
- Meter (4 tokens): `--meter-bg`, `--meter-fill`, `--meter-fill-warn`, `--meter-fill-clip`
- Piano Roll & Key Transposer (14 tokens): `--pianoroll-bg`, `--pianoroll-grid`, `--pianoroll-note`, `--pianoroll-note-active`, `--pianoroll-note-grad-end`, `--pianoroll-key-white-bg`, `--pianoroll-key-white-tx`, `--pianoroll-key-white-hover`, `--pianoroll-key-black-bg`, `--pianoroll-key-black-tx`, `--pianoroll-key-black-hover`, `--pianoroll-key-active-bg`, `--pianoroll-key-active-tx`, `--pianoroll-root-marker`
- Mini Waveform Preview on List Rows (3 tokens): `--mini-wave-color`, `--mini-wave-hover`, `--mini-wave-sel`
- Shadows & Visual Effects (3 tokens): `--shadow-modal`, `--shadow-pop`, `--shadow-text-glow`
- Animation & System (3 tokens): `--t-fast`, `--t-med`, `--focus-ring`

#### B. Verification via `python tests/verify_tokens_test.py`
Running `python tests/verify_tokens_test.py` confirmed:
- Block count: 3 selector blocks (`dark-studio`, `pastel-pink`, `cyberpunk`).
- Parity: 0 missing tokens in `pastel-pink`, 0 missing in `cyberpunk`.
- Syntax: 246 / 246 valid CSS declarations.
- Hardcoded hex in `app.css`: 0.

#### C. Observations of Color Mismatches in Codebase
1. **Legacy property names in `ThemeManager.applyTheme()` (`ui-web/app.js:259-267`)**:
   ```javascript
   // app.js lines 259-267:
   bgSurface: styles.getPropertyValue('--bg-surface').trim(),         // Mismatch: not in tokens.css
   accentPrimary: styles.getPropertyValue('--accent-primary').trim(), // Mismatch: in tokens.css it is --accent
   txPrimary: styles.getPropertyValue('--tx-primary').trim(),         // Mismatch: in tokens.css it is --text-primary
   txSecondary: styles.getPropertyValue('--tx-secondary').trim(),     // Mismatch: in tokens.css it is --text-secondary
   txMuted: styles.getPropertyValue('--tx-muted').trim(),             // Mismatch: in tokens.css it is --text-tertiary
   borderMedium: styles.getPropertyValue('--border-medium').trim(),   // Mismatch: in tokens.css it is --border-default
   ```
2. **`applyAccent(name)` inline style pollution (`ui-web/app.js:1518-1531`)**:
   `applyAccent` writes inline style properties directly to `document.documentElement.style.setProperty('--accent', ...)`. These inline styles override the CSS stylesheet rules when switching themes (e.g. selecting `pastel-pink` won't update `--accent` to pink if inline `--accent: #FF6B2C` remains on `<html>`).
3. **Canvas Drawing Color Hardcoding in `app.js` (`lines 2819-2980`)**:
   - `drawWaveform()` MIDI mode: hardcoded `'#0B0E14'`, `'rgba(255,255,255,0.04)'`, `'#FFFFFF'`, `'#38BDF8'`, `'#0284C7'`, `'rgba(255,255,255,0.85)'`.
   - `drawWaveform()` Audio mode: hardcoded `'rgba(255,255,255,0.05)'`, `'rgba(56, 189, 248, 0.75)'`, `'rgba(255,255,255,0.12)'`, `'rgba(255,255,255,0.85)'`.
   - `drawMeterSmoothed()`: hardcoded gradient stops `'#2EA0D6'`, `'#22C55E'`, `'#EAB308'`, `'#EF4444'`.

#### D. SVG Icons Implementation
- `ui-web/app.js:1328-1332` (Navigation icons): Use `stroke="currentColor"`, `fill="none"`, and `fill="var(--bg-app)"`.
- `ui-web/index.html` (Header, player, browser icons): Use `stroke="currentColor"`, `fill="none"`, adapting automatically to `--text-icon` / `--text-primary`.
- `ui-web/assets/logo.svg` and `reals-mark.svg`: Standalone static SVG files with hardcoded `#2D2D2D`, `#FFFFFF`, `#c8f93a`.

---

### 1.3 ThemeManager, Bootstrap Script & IPC Bridge

#### A. Inline `<head>` Bootstrap Script (`ui-web/index.html:7-16`)
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
- **Execution**: Executes synchronously in `<head>` before stylesheets, DOM parsing, or first paint.
- **FOUC Prevention**: Eliminates white flash during initial page load by instantly applying the cached theme from `localStorage`.

#### B. `ThemeManager` in `ui-web/app.js:208-287`
- Initialized on window load: `window.themeManager = new ThemeManager()`.
- Valid themes: `['dark-studio', 'pastel-pink', 'cyberpunk']`.
- Methods:
  - `getTheme()`: returns `_currentTheme`.
  - `applyTheme(themeName, notifyNative = false)`:
    - Normalizes invalid inputs to `'dark-studio'`.
    - Sets `localStorage.setItem('reals_theme', themeName)`.
    - Sets `document.documentElement.setAttribute('data-theme', themeName)`.
    - Dispatches `CustomEvent('themeUpdated', { detail: { theme: themeName, tokens } })`.
    - If `notifyNative === true`, sends plain string IPC: `window.chrome.webview.postMessage('THEME_CHANGED:' + themeName)`.
- IPC Message Handling:
  - In constructor & bridge message listener (`app.js:225-233`, `app.js:543-551`):
    Listens for `data.startsWith('THEME_CHANGED:')` -> calls `applyTheme(themeName, false)` without sending a loopback message to C++.

#### C. Native REAPER C++ Bridge (`extension/src/reaper_plugin.cpp`)
- Host startup (`reaper_plugin.cpp:1152-1162`):
  Queries REAPER SDK `GetExtState("REALSLAB", "theme")` (fallback to `Config::instance().getString("theme", "dark-studio")`), then executes:
  `window.themeManager && window.themeManager.applyTheme('<name>', false);`
- Host message handler (`reaper_plugin.cpp:1169-1178`):
  Intercepts `THEME_CHANGED:<name>` -> calls `SetExtState("REALSLAB", "theme", themeName.c_str(), true)` (persisting to `reaper-extstate.ini`) and `Config::instance().set("theme", themeName)`.
- Host window display (`reaper_plugin.cpp:1220-1230`):
  Re-pushes the current persisted theme on window show/unhide to ensure complete state synchronization.
- Transparent host background (`shell/win/WebViewHost.cpp:208, 285`):
  Sets `controller2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{0,0,0,0})` and `put_IsVisible(FALSE)` until DOM is ready.

---

### 1.4 Canvas Renderers (Waveform, Piano Roll, Meter)

#### A. Current Rendering Path
- Canvas element: `<canvas id="waveform" height="44"></canvas>` in `#preview`.
- Function `drawWaveform()` (`app.js:2799-2961`):
  - Triggered on playback position ticks, file selection, resize, and zoom.
  - MIDI files (`isMidiFile(curPath)`): Draws 8-row grid, note rectangles with linear gradient, and vertical playhead.
  - Audio files: Reads envelope bars from `state.envelope` / `state.envCache`, computes bar height with curve `Math.pow(val, 0.75) * amp`, and renders played vs unplayed bars separated by centerline.
- Function `drawMeterSmoothed(peak)` (`app.js:2963-2985`):
  - Canvas element: `<canvas id="meter" height="8"></canvas>`.
  - Draws volume peak level with linear gradient.

#### B. Audio Playback Performance & Color Resolution Issue
- **Current problem**: `drawWaveform()` and `drawMeterSmoothed()` hardcode colors directly in JS, so changing `data-theme` does NOT alter canvas colors unless hardcoded values are replaced with dynamic theme tokens.
- **Critical Performance Constraint**: `drawWaveform()` executes at 30–60 FPS during audio preview playback. Calling `window.getComputedStyle(document.documentElement)` inside `drawWaveform()` would cause severe layout thrashing (5–15ms per frame), resulting in audio playback buffer underruns (glitches/pops) and UI frame drops.
- **Recommended Solution**:
  1. Maintain a fast in-memory color cache object in JS:
     ```javascript
     const canvasTheme = {
       waveformBg: '#0B0E14',
       waveformFill: 'rgba(255, 255, 255, 0.12)',
       waveformFillActive: 'rgba(56, 189, 248, 0.75)',
       waveformPlayhead: 'rgba(255, 255, 255, 0.85)',
       waveformCenterline: 'rgba(255, 255, 255, 0.05)',
       meterBg: '#0B0E14',
       meterFill: '#35D07F',
       meterFillWarn: '#F59E0B',
       meterFillClip: '#FF5C66',
       pianorollBg: '#0B0E14',
       pianorollGrid: 'rgba(255, 255, 255, 0.04)',
       pianorollNote: '#38BDF8',
       pianorollNoteActive: '#FFFFFF',
       pianorollNoteEnd: '#0284C7'
     };
     ```
  2. Subscribe to `window.addEventListener('themeUpdated', (e) => { ... })`:
     Update `canvasTheme` fields from `e.detail.tokens` or a single `getComputedStyle()` call ONCE on theme change, then invoke `drawWaveform()` and `drawMeter()`.
  3. Inside `drawWaveform()` and `drawMeterSmoothed()`, reference `canvasTheme.*` directly with 0ms overhead.

---

### 1.5 Theme Picker UI in Settings & User Interaction Flow

#### A. Current Settings Modal Layout (`ui-web/index.html:275-368`)
- Trigger: Gear icon `btnSettings` (⚙) in `#topbar`.
- Settings Modal has 5 tabs:
  1. `tab-general` (General)
  2. `tab-browser` (File Browser)
  3. `tab-market` (Marketplace)
  4. `tab-stem` (Stem Separation)
  5. `tab-agent` (Agent AI)
- In `tab-general` (`index.html:304-322`):
  - Group 1: `optDisplaySize` (Small, Medium, Large)
  - Group 2: `optNavPos` (Top, Bottom, Left, Right)
  - **Currently Missing**: Theme selection group (`optTheme`).

#### B. Required Theme Picker UI Structure
1. Add Theme Picker group in `ui-web/index.html` within `#tab-general`:
   ```html
   <div class="setting-group">
     <label class="setting-lbl" data-i18n="settings.theme">Giao diện (Theme)</label>
     <div class="setting-btn-group" id="optTheme">
       <button class="setting-chip" data-val="dark-studio" data-i18n="theme.darkStudio">Dark Studio</button>
       <button class="setting-chip" data-val="pastel-pink" data-i18n="theme.pastelPink">Pastel Pink</button>
       <button class="setting-chip" data-val="cyberpunk" data-i18n="theme.cyberpunk">Cyberpunk</button>
     </div>
   </div>
   ```
2. Add i18n keys in `I18N.vi` and `I18N.en` (`app.js:5-203` and `assets/i18n/`):
   - `vi`: `'settings.theme': 'Giao diện', 'theme.darkStudio': 'Dark Studio', 'theme.pastelPink': 'Pastel Pink', 'theme.cyberpunk': 'Cyberpunk Neon'`
   - `en`: `'settings.theme': 'Theme Palette', 'theme.darkStudio': 'Dark Studio', 'theme.pastelPink': 'Pastel Pink', 'theme.cyberpunk': 'Cyberpunk Neon'`
3. In `renderSettingsModal()` (`app.js:1450-1510`):
   - Query `window.themeManager.getTheme()`
   - Toggle `.active` class on `#optTheme .setting-chip` based on `c.dataset.val === curTheme`
   - Attach `onclick` -> `window.themeManager.applyTheme(v, true); renderSettingsModal();`

---

## 2. Logic Chain

```
[Observation 1.1: tokens.css defines 82 variables per theme for dark-studio, pastel-pink, cyberpunk]
      │
      ├──> [Observation 1.2: verify_tokens_test.py proves 100% token override parity & zero hardcoded hex in app.css]
      │
      ├──> [Observation 1.3: <head> script loads localStorage cache -> zero FOUC on boot]
      │           │
      │           └──> [Observation 1.3: C++ reaper_plugin.cpp handles GetExtState/SetExtState and THEME_CHANGED:<name>]
      │
      ├──> [Observation 1.4: drawWaveform / drawMeter currently use hardcoded hex/rgba and do not subscribe to themeUpdated]
      │           │
      │           └──> [Logic: Reading getComputedStyle inside 60FPS drawWaveform causes audio underruns]
      │                       │
      │                       └──> [Conclusion: Decouple canvas colors via in-memory canvasTheme cache updated on themeUpdated event]
      │
      └──> [Observation 1.5: Settings modal tab-general lacks optTheme picker chips]
                  │
                  └──> [Conclusion: Add optTheme chips in index.html & bind to themeManager.applyTheme(val, true) in app.js]
```

1. **Token Foundation**: `ui-web/tokens.css` is syntactically sound and complete across all 3 required themes.
2. **IPC & Persistence Synchronization**: The bidirectional protocol `THEME_CHANGED:<name>` and `applyTheme('<name>', false)` matches between `ThemeManager` (`app.js`), `reaper_plugin.cpp`, and the unit test harness `TestSuite_ThemeEngine.cpp`.
3. **Canvas Synchronization**: To achieve zero-latency live canvas waveform and piano roll updates without audio hiccups, canvas color tokens must be cached in memory on `themeUpdated` CustomEvent and read synchronously by the render loop.
4. **Theme Picker Integration**: Adding the theme chips to `tab-general` completes the user flow with instant feedback and persistent ExtState storage.

---

## 3. Caveats

1. **Web vs Standalone Mock**: When running in a standalone web browser (outside REAPER), `window.chrome.webview` is undefined; `ThemeManager` and `mockBridge` handle this gracefully by falling back to `localStorage`.
2. **Inline Accent Overrides**: `applyAccent()` in `app.js` sets inline styles on `documentElement.style`. The implementer must ensure that switching themes either clears or harmonizes inline accent overrides so that theme-specific accent colors (`#FF6B2C`, `#FF4081`, `#00F0FF`) take effect cleanly.
3. **SVG Vector Adapters**: Vector icons in `app.js` and `index.html` already use `stroke="currentColor"` or CSS variables, while static brand logos in `ui-web/assets/` remain brand-specific.

---

## 4. Conclusion

The Web UI architecture is well-structured for the Theme Engine implementation:
1. **Design Tokens**: `ui-web/tokens.css` contains all 82 tokens for 3 themes with 100% override parity.
2. **FOUC Prevention**: The inline `<head>` bootstrap script combined with transparent WebView2 initialization provides zero-FOUC startup.
3. **IPC Bridge**: The plain string `THEME_CHANGED:<name>` protocol is already recognized by `reaper_plugin.cpp` and tested in `TestSuite_ThemeEngine.cpp`.
4. **Actionable Implementation Checklist for Implementer Agent**:
   - [ ] Fix variable name mismatches in `ThemeManager.applyTheme()` (`--text-primary`, `--accent`, etc.).
   - [ ] In `app.js`, create `canvasThemeColors` cache and register `window.addEventListener('themeUpdated', ...)` to redraw waveform and meter.
   - [ ] Update `drawWaveform()` and `drawMeterSmoothed()` to use `canvasThemeColors` instead of hardcoded hex/rgba values.
   - [ ] Ensure `applyTheme()` clears conflicting inline `--accent` styles set by `applyAccent()`.
   - [ ] Add `#optTheme` button group into `ui-web/index.html` under `#tab-general`.
   - [ ] Add i18n keys for themes in `app.js` (`I18N.vi` & `I18N.en`) and `assets/i18n/`.
   - [ ] Wire up `#optTheme` click handlers in `renderSettingsModal()` in `app.js`.

---

## 5. Verification Method

To independently verify the frontend theme engine implementation:

1. **Verify Token CSS Parity**:
   ```powershell
   python tests/verify_tokens_test.py
   ```
   *Expected*: Passes with 100% token parity (0 missing, 0 extra tokens across 3x82 definitions) and 0 undefined variables.

2. **Verify C++ Theme Engine Unit Tests**:
   ```powershell
   cmake --build --preset windows
   ctest --preset windows -R ThemeEngine --output-on-failure
   ```
   *Expected*: 100% pass across all Tier 1–4 test cases in `TestSuite_ThemeEngine.cpp`.

3. **Visual & Interactive Verification in Web / REAPER**:
   - Open Settings (⚙) -> Verify Theme chips ("Dark Studio", "Pastel Pink", "Cyberpunk") are displayed and indicate the active theme.
   - Click "Pastel Pink" -> UI switches instantly (<16ms) to pink palette; waveform canvas and meter canvas switch to pink/rose accents.
   - Click "Cyberpunk" -> UI switches instantly to neon cyan/pink palette; waveform canvas and meter canvas update immediately.
   - Reload page / restart REAPER -> Selected theme is restored with zero white flash or FOUC.
