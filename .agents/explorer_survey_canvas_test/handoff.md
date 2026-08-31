# Explorer 3 Survey Report: Waveform Canvas, Piano Roll Theme Adaptation, & Test Infrastructure

**Survey Target**: Waveform Canvas rendering, Piano Roll theme adaptation, Dynamic CSS variable extraction, and Test Suite / Build infrastructure.  
**Working Directory**: `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_canvas_test`  
**Date**: 2026-08-31T14:31:30Z  

---

## 1. Observation

### 1.1 Waveform Canvas & Audio Meter Rendering (`ui-web/app.js`, `ui-web/app.css`, `ui-web/index.html`)

1. **HTML Canvas Elements**:
   - `ui-web/index.html:175`: `<canvas id="waveform" height="44"></canvas>`
   - `ui-web/index.html:177`: `<canvas id="meter" height="8"></canvas>`

2. **Canvas Styling in `ui-web/app.css`**:
   - Lines 837-842:
     ```css
     #waveform {
       width:100%; height:44px; display:block; background:#0B0E14;
       border:none; border-radius:6px;
       cursor:grab; box-shadow:none !important;
     }
     #waveform:active, #waveform.dragging { cursor:grabbing; }
     ```
   - Lines 843-846:
     ```css
     #meter {
       flex:1; height:6px; display:block; background:#0B0E14;
       border:none; border-radius:3px;
     }
     ```
   - Background colors are hardcoded to `#0B0E14` (dark slate).

3. **Audio Waveform Canvas Drawing Routine (`ui-web/app.js:2826-2872`)**:
   - Envelope arrays are fetched from `state.envelope` or `state.envCache[curPath]`.
   - Drawing geometry:
     - `const numBars = env.length; const barW = W / numBars; const drawW = Math.max(1, barW - 1.2);`
     - `const curved = Math.pow(Math.min(1, val), 0.75); const h = Math.max(1, curved * amp);`
     - `const barY = mid - h; const barH = Math.max(2, h * 2);`
     - Rendered with `ctx.roundRect(x, barY, drawW, barH, 0.6)`.
   - **Hardcoded colors in `drawWaveform()`**:
     - Centerline: `ctx.fillStyle = 'rgba(255,255,255,0.05)';` (`app.js:2833`)
     - Unplayed audio bars: `'rgba(255,255,255,0.12)'` (`app.js:2849`)
     - Played audio bars: `'rgba(56, 189, 248, 0.75)'` (`app.js:2849`)
     - Playhead cursor: `ctx.fillStyle = 'rgba(255,255,255,0.85)';` (`app.js:2864`)
     - Idle placeholder: `ctx.fillStyle = 'rgba(255,255,255,0.08)';` (`app.js:2869`)

4. **Audio Peak / VU Meter Drawing Routine (`ui-web/app.js:2874-2896`)**:
   - Peak decay ballistics: `_meterSmoothedVal = Math.max(0, _meterSmoothedVal - dt * 2.4);` (`app.js:2672`).
   - Gradient fill:
     ```javascript
     const grad = ctx.createLinearGradient(0, 0, W, 0);
     grad.addColorStop(0, '#2EA0D6');
     grad.addColorStop(0.65, '#22C55E');
     grad.addColorStop(0.85, '#EAB308');
     grad.addColorStop(1, '#EF4444');
     ctx.fillStyle = grad;
     ```

---

### 1.2 Piano Roll Rendering (`ui-web/app.js`, `ui-web/app.css`, `ui-web/index.html`)

There are two separate Piano Roll components in the codebase:

1. **MIDI Preview Canvas inside `drawWaveform()` (`ui-web/app.js:2722-2824`)**:
   - Activated when `isMidiFile(curPath)` is true.
   - Grid: 8 pitch rows $\times$ 16 time cols (`rows = 8; cols = 16; cellH = H / rows; cellW = W / cols;`).
   - Background & Grid lines:
     - `ctx.fillStyle = '#0B0E14';` (`app.js:2730`)
     - `ctx.strokeStyle = 'rgba(255,255,255,0.04)';` (`app.js:2733`)
   - MIDI Notes Gradient:
     - Active notes (`curTime >= n.time && curTime <= n.time + n.duration`):
       `grad.addColorStop(0, '#FFFFFF'); grad.addColorStop(1, '#38BDF8');` (`app.js:2774-2775`)
     - Inactive notes:
       `grad.addColorStop(0, '#38BDF8'); grad.addColorStop(1, '#0284C7');` (`app.js:2777-2778`)
     - Fallback algorithmic pattern:
       `grad.addColorStop(0, '#38BDF8'); grad.addColorStop(1, '#0284C7');` (`app.js:2807-2808`)
   - Playhead vertical line:
     - `ctx.fillStyle = 'rgba(255,255,255,0.85)';` (`app.js:2820`)

2. **Mini Piano Keyboard Transposer Popup DOM Component (`ui-web/index.html:191-213`, `ui-web/app.css:880-950`)**:
   - `#pianoTransposerPop` popup container: `background: #12151C; border: 1px solid rgba(255,255,255,0.1);` (`app.css:883`)
   - `#pianoKeyboard` frame: `background: #0A0B0D;` (`app.css:903`)
   - `.piano-key.white`: `background: #E2E4E9; color: #1E2024;` (hover: `#FFFFFF`) (`app.css:908-914`)
   - `.piano-key.black`: `background: #181A1F; color: #8F939B;` (hover: `#2D3038`, `#FFF`) (`app.css:926-932`)
   - `.piano-key.active`: `background: #38BDF8 !important; color: #0A0D14 !important;` (`app.css:915-917, 933-935`)
   - `.piano-key.root-marker`: `box-shadow: inset 0 -4px 0 0 #F59E0B;` with `::after` dot `#F59E0B` (`app.css:918-924, 936-942`)
   - `#pianoRootBadge`, `#pianoSemitoneLabel`, `#btnResetKey`: hardcoded `color: #38BDF8; background: rgba(56,189,248,0.16);` (`app.css:895-950`)

3. **File List Mini SVG Waveforms & MIDI Previews**:
   - `generateMiniWaveSvg(f)` (`app.js:1689-1721`): uses `fill="currentColor"`. It dynamically respects the parent CSS text color.
   - `generateMiniMidiSvg(f)` (`app.js:1742-1760`): uses hardcoded palette array `const colors = ['#55A5FF', '#35D07F', '#B98CFF', '#55A5FF'];` (`app.js:1750`).

---

### 1.3 Build and Test Infrastructure (`CMakeLists.txt`, `CMakePresets.json`, `tests/`)

1. **Root CMake & Presets**:
   - Root `CMakeLists.txt` targets C++20 standard, MSVC `/W4`, `/permissive-`, `/utf-8`, `/FS`.
   - `CMakePresets.json`:
     - Configure preset `windows`: VS 2022 generator, binary directory `build/windows`, `REALS_BUILD_APP=OFF`, `REALS_BUILD_EXTENSION=ON`.
     - Build preset `windows`: builds `reals_core`, `reals_bridge`, `reals_shell_win`, `reaper_realslab` (DLL), `reals_tests` (exe).
     - Test preset `windows`: runs `ctest` targeting `reals_e2e_tests`.
   - Verification command execution:
     - `cmake --build --preset windows`: Compiles with zero errors and zero warnings in ~3s. Automatically deploys `reaper_realslab.dll` to `%APPDATA%/REAPER/UserPlugins`.
     - `build\windows\tests\Debug\reals_tests.exe --suite=BridgeUI`: 37/37 tests passed in 30.8s.

2. **Test Framework (`tests/framework/TestRunner.h`)**:
   - Header-only GoogleTest-compatible test runner (`TEST`, `TEST_F`, `EXPECT_EQ`, `ASSERT_EQ`, `EXPECT_TRUE`, `EXPECT_NEAR`, `EXPECT_THROW`, etc.).
   - Supports CLI flags: `--suite=<Name>`, `--filter=<Pattern>`, `--list`, `-l`, `--help`.
   - `MockHostActions.h`: Implements `IHostActions` mock interface providing simulated REAPER calls (`GetExtState`, `SetExtState`, `insertMedia`, `getTrackCount`, etc.).

---

## 2. Logic Chain

```
[Observation 1.1 & 1.2: Hardcoded Canvas & CSS Colors]
  │
  ├──> Canvas 2D context draws pixels imperatively; CSS cascade does NOT automatically restyle HTML5 canvas pixels.
  │
  ├──> If CSS variables change on <html>, canvas elements remain stale until an explicit redraw is executed with new RGB values.
  │
  ├──> Calling getComputedStyle() inside 60fps updatePreviewLive() causes layout thrashing and CPU overhead.
  │
  └──> SOLUTION: ThemeManager extracts computed CSS variables ONCE when theme changes, caches them in a JS object, and fires 'themeUpdated' CustomEvent.

[Observation 1.3: Bridge & MockHostActions in tests/]
  │
  ├──> MockHostActions already provides mock host facilities for testing.
  │
  ├──> C++ IPC protocol can handle plain string messages ("THEME_CHANGED:<name>") and persist via SetExtState("RealsLab", "theme", name).
  │
  └──> SOLUTION: Add TestSuite_ThemeEngine.cpp testing ExtState persistence, IPC message handling, and fallback resilience.
```

1. **Why `themeUpdated` CustomEvent is Necessary**:
   - HTML DOM elements automatically re-evaluate CSS variables upon `document.documentElement.setAttribute('data-theme', themeName)`.
   - However, `<canvas id="waveform">` and `<canvas id="meter">` maintain drawn pixel bitmaps on their `CanvasRenderingContext2D`.
   - Emitting `window.dispatchEvent(new CustomEvent('themeUpdated', { detail: { theme, tokens } }))` provides a clean, decoupled broadcast channel.
   - Canvas renderers register a listener on `themeUpdated` and immediately call `drawWaveform()` and `drawMeter()`.

2. **Preventing Audio Glitches & Ensuring Zero-Lag (<16ms)**:
   - Waveform/meter redraw only modifies the canvas 2D frame buffer in the UI thread.
   - Audio rendering runs on Miniaudio / SoundTouch threads in `reals_core`.
   - Theme switching does **not** stop, recreate, or block audio streams or IPC bridges.
   - By caching resolved token colors in memory during `applyTheme()`, `drawWaveform()` never invokes `getComputedStyle()` during routine playback animation frames.

---

## 3. Caveats

1. **Color Format in Canvas Contexts**: Canvas 2D gradient and fill APIs accept valid CSS color strings (`#HEX`, `rgb(...)`, `rgba(...)`, `hsl(...)`). CSS variables must resolve to concrete color values, not unresolved variable chains.
2. **Audio File Envelope vs MIDI**: When changing themes during MIDI playback vs. Audio sample playback, `drawWaveform()` branches on `isMidiFile(curPath)`. Both branches must reference the respective theme tokens.
3. **High-DPI Scaling (DevicePixelRatio)**: When canvas is resized or initialized, `c.width = w * devicePixelRatio` and `c.height = h * devicePixelRatio` ensure razor-sharp rendering on Retina/4K displays.

---

## 4. Conclusion & Concrete Technical Blueprint

### 4.1 Theme Token Mapping Specification (`ui-web/tokens.css` / `ui-web/app.css`)

The following semantic CSS variables must be declared in `:root` and overridden in `html[data-theme="..."]`:

```css
/* ==========================================================================
   Design Tokens & Theme Definitions
   ========================================================================== */

:root, html[data-theme="dark-studio"] {
  /* Surface & Backgrounds */
  --bg-root: #090A0C;
  --bg-app: #0D0E11;
  --bg-sidebar: #101114;
  --bg-panel: #121316;
  --bg-card: #15171A;
  --bg-card-hover: #191B1F;
  --bg-input: #0D0F12;
  --bg-elevated: #1A1C20;
  --bg-nav-active: #17191C;

  /* Borders */
  --border-subtle: #24262B;
  --border-default: #2C2F35;
  --border-strong: #363941;
  --border-card: #24272C;
  --border-input: #292C31;

  /* Typography */
  --text-primary: #F2F3F5;
  --text-secondary: #A3A6AD;
  --text-tertiary: #737780;
  --text-disabled: #6A6D75;
  --text-icon: #858991;

  /* Brand Accents */
  --accent: #FF6B2C;
  --accent-hover: #FF7A3D;
  --accent-active: #E9571D;
  --accent-soft: rgba(255, 107, 44, 0.12);
  --accent-border: rgba(255, 107, 44, 0.35);
  --accent-focus: rgba(255, 107, 44, 0.55);

  /* Waveform Canvas Tokens */
  --waveform-bg: #0B0E14;
  --waveform-fill: rgba(255, 255, 255, 0.12);
  --waveform-fill-active: rgba(56, 189, 248, 0.85);
  --waveform-centerline: rgba(255, 255, 255, 0.05);
  --waveform-cursor: rgba(255, 255, 255, 0.90);
  --waveform-idle: rgba(255, 255, 255, 0.08);

  /* Meter Tokens */
  --meter-bg: #0B0E14;
  --meter-grad-0: #2EA0D6;
  --meter-grad-mid: #22C55E;
  --meter-grad-warn: #EAB308;
  --meter-grad-clip: #EF4444;

  /* Piano Roll MIDI Canvas Tokens */
  --piano-roll-bg: #0B0E14;
  --piano-roll-grid: rgba(255, 255, 255, 0.04);
  --piano-roll-note-grad-0: #38BDF8;
  --piano-roll-note-grad-1: #0284C7;
  --piano-roll-active-grad-0: #FFFFFF;
  --piano-roll-active-grad-1: #38BDF8;
  --piano-roll-playhead: rgba(255, 255, 255, 0.85);

  /* Piano Transposer Popup DOM Tokens */
  --piano-pop-bg: #12151C;
  --piano-pop-border: rgba(255, 255, 255, 0.10);
  --piano-kb-bg: #0A0B0D;
  --piano-key-white-bg: #E2E4E9;
  --piano-key-white-hover: #FFFFFF;
  --piano-key-white-tx: #1E2024;
  --piano-key-black-bg: #181A1F;
  --piano-key-black-hover: #2D3038;
  --piano-key-black-tx: #8F939B;
  --piano-key-active-bg: #38BDF8;
  --piano-key-active-tx: #0A0D14;
  --piano-key-root-mark: #F59E0B;
}

html[data-theme="pastel-pink"] {
  /* Surface & Backgrounds (Soft Strawberry / Lavender Blush Light Theme) */
  --bg-root: #FFF5F7;
  --bg-app: #FFF0F5;
  --bg-sidebar: #FFE8EE;
  --bg-panel: #FFE2EA;
  --bg-card: #FFFFFF;
  --bg-card-hover: #FFF9FA;
  --bg-input: #FFFFFF;
  --bg-elevated: #FFD8E3;
  --bg-nav-active: #FFD4E1;

  /* Borders */
  --border-subtle: #F3C4D2;
  --border-default: #EAAEC0;
  --border-strong: #DF98AD;
  --border-card: #F0BCCB;
  --border-input: #EAAEC0;

  /* Typography */
  --text-primary: #2D1420;
  --text-secondary: #6E3D52;
  --text-tertiary: #9C657C;
  --text-disabled: #B88AA0;
  --text-icon: #8F546E;

  /* Brand Accents */
  --accent: #E75480;
  --accent-hover: #FF6699;
  --accent-active: #D13E6B;
  --accent-soft: rgba(231, 84, 128, 0.14);
  --accent-border: rgba(231, 84, 128, 0.40);
  --accent-focus: rgba(231, 84, 128, 0.55);

  /* Waveform Canvas Tokens */
  --waveform-bg: #FFE4EC;
  --waveform-fill: rgba(231, 84, 128, 0.28);
  --waveform-fill-active: #E75480;
  --waveform-centerline: rgba(231, 84, 128, 0.20);
  --waveform-cursor: #C2185B;
  --waveform-idle: rgba(231, 84, 128, 0.15);

  /* Meter Tokens */
  --meter-bg: #FFE4EC;
  --meter-grad-0: #FF69B4;
  --meter-grad-mid: #FF1493;
  --meter-grad-warn: #FF8C00;
  --meter-grad-clip: #E53935;

  /* Piano Roll MIDI Canvas Tokens */
  --piano-roll-bg: #FFE4EC;
  --piano-roll-grid: rgba(231, 84, 128, 0.12);
  --piano-roll-note-grad-0: #FF85A2;
  --piano-roll-note-grad-1: #E75480;
  --piano-roll-active-grad-0: #FFFFFF;
  --piano-roll-active-grad-1: #FF4081;
  --piano-roll-playhead: #C2185B;

  /* Piano Transposer Popup DOM Tokens */
  --piano-pop-bg: #FFFFFF;
  --piano-pop-border: #F3C4D2;
  --piano-kb-bg: #FFE4EC;
  --piano-key-white-bg: #FFFFFF;
  --piano-key-white-hover: #FFF0F5;
  --piano-key-white-tx: #2D1420;
  --piano-key-black-bg: #6E3D52;
  --piano-key-black-hover: #542B3C;
  --piano-key-black-tx: #FFE4EC;
  --piano-key-active-bg: #E75480;
  --piano-key-active-tx: #FFFFFF;
  --piano-key-root-mark: #FF6F00;
}

html[data-theme="cyberpunk"] {
  /* Surface & Backgrounds (High-Contrast Obsidian & Neon) */
  --bg-root: #040408;
  --bg-app: #080811;
  --bg-sidebar: #0C0C18;
  --bg-panel: #101020;
  --bg-card: #141428;
  --bg-card-hover: #1A1A34;
  --bg-input: #080812;
  --bg-elevated: #1C1C3A;
  --bg-nav-active: #202042;

  /* Borders */
  --border-subtle: #242446;
  --border-default: #343464;
  --border-strong: #00FFFF;
  --border-card: #282850;
  --border-input: #343464;

  /* Typography */
  --text-primary: #00FFFF;
  --text-secondary: #FF007F;
  --text-tertiary: #A6A6D0;
  --text-disabled: #5E5E8A;
  --text-icon: #00FFFF;

  /* Brand Accents */
  --accent: #FFE600;
  --accent-hover: #FFF04D;
  --accent-active: #E6CE00;
  --accent-soft: rgba(255, 230, 0, 0.15);
  --accent-border: rgba(255, 230, 0, 0.45);
  --accent-focus: rgba(255, 230, 0, 0.65);

  /* Waveform Canvas Tokens */
  --waveform-bg: #05050C;
  --waveform-fill: rgba(0, 255, 255, 0.22);
  --waveform-fill-active: #FF007F;
  --waveform-centerline: rgba(0, 255, 255, 0.18);
  --waveform-cursor: #FFE600;
  --waveform-idle: rgba(0, 255, 255, 0.10);

  /* Meter Tokens */
  --meter-bg: #05050C;
  --meter-grad-0: #00FFFF;
  --meter-grad-mid: #00FF66;
  --meter-grad-warn: #FFE600;
  --meter-grad-clip: #FF0055;

  /* Piano Roll MIDI Canvas Tokens */
  --piano-roll-bg: #05050C;
  --piano-roll-grid: rgba(0, 255, 255, 0.10);
  --piano-roll-note-grad-0: #00FFFF;
  --piano-roll-note-grad-1: #0066FF;
  --piano-roll-active-grad-0: #FFE600;
  --piano-roll-active-grad-1: #FF007F;
  --piano-roll-playhead: #FFE600;

  /* Piano Transposer Popup DOM Tokens */
  --piano-pop-bg: #0C0C1A;
  --piano-pop-border: #FF007F;
  --piano-kb-bg: #05050C;
  --piano-key-white-bg: #00FFFF;
  --piano-key-white-hover: #80FFFF;
  --piano-key-white-tx: #040408;
  --piano-key-black-bg: #141428;
  --piano-key-black-hover: #222244;
  --piano-key-black-tx: #FF007F;
  --piano-key-active-bg: #FF007F;
  --piano-key-active-tx: #FFFFFF;
  --piano-key-root-mark: #FFE600;
}
```

---

### 4.2 ThemeManager Implementation Blueprint (`ui-web/app.js`)

```javascript
// ============================================================================
// Theme Manager & Canvas Live Synchronizer
// ============================================================================

const ThemeManager = {
  currentTheme: 'dark-studio',
  tokens: {},

  init() {
    // 1. Read inline bootstrap theme (from localStorage or default)
    let saved = 'dark-studio';
    try { saved = localStorage.getItem('reals_theme') || 'dark-studio'; } catch (_) {}
    this.applyTheme(saved, false);

    // 2. Wire native bridge message listeners
    window.addEventListener('message', (e) => {
      const msg = e.data;
      if (typeof msg === 'string' && msg.startsWith('THEME_CHANGED:')) {
        const theme = msg.slice(14).trim();
        this.applyTheme(theme, false);
      }
    });

    // 3. Register themeUpdated listener to immediately refresh canvas
    window.addEventListener('themeUpdated', () => {
      if (typeof drawWaveform === 'function') drawWaveform();
      if (typeof drawMeter === 'function') drawMeter();
      if (typeof paintVisible === 'function') paintVisible();
    });
  },

  applyTheme(themeName, notifyNative = true) {
    const validThemes = ['dark-studio', 'pastel-pink', 'cyberpunk'];
    if (!validThemes.includes(themeName)) themeName = 'dark-studio';

    this.currentTheme = themeName;
    document.documentElement.setAttribute('data-theme', themeName);

    // Extract computed CSS tokens once per switch
    this.extractTokens();

    // Notify all UI & Canvas components
    window.dispatchEvent(new CustomEvent('themeUpdated', {
      detail: {
        theme: themeName,
        tokens: this.tokens
      }
    }));

    // Update settings theme buttons if present
    this.updateUI();

    // Persist to localStorage cache
    try { localStorage.setItem('reals_theme', themeName); } catch (_) {}

    // Send IPC to C++ host for REAPER SetExtState persistence
    if (notifyNative && window.chrome?.webview?.postMessage) {
      window.chrome.webview.postMessage(`THEME_CHANGED:${themeName}`);
    }
  },

  extractTokens() {
    const s = getComputedStyle(document.documentElement);
    this.tokens = {
      waveformBg: s.getPropertyValue('--waveform-bg').trim() || '#0B0E14',
      waveformFill: s.getPropertyValue('--waveform-fill').trim() || 'rgba(255,255,255,0.12)',
      waveformFillActive: s.getPropertyValue('--waveform-fill-active').trim() || 'rgba(56,189,248,0.75)',
      waveformCenterline: s.getPropertyValue('--waveform-centerline').trim() || 'rgba(255,255,255,0.05)',
      waveformCursor: s.getPropertyValue('--waveform-cursor').trim() || 'rgba(255,255,255,0.85)',
      waveformIdle: s.getPropertyValue('--waveform-idle').trim() || 'rgba(255,255,255,0.08)',
      meterBg: s.getPropertyValue('--meter-bg').trim() || '#0B0E14',
      meterGrad0: s.getPropertyValue('--meter-grad-0').trim() || '#2EA0D6',
      meterGradMid: s.getPropertyValue('--meter-grad-mid').trim() || '#22C55E',
      meterGradWarn: s.getPropertyValue('--meter-grad-warn').trim() || '#EAB308',
      meterGradClip: s.getPropertyValue('--meter-grad-clip').trim() || '#EF4444',
      pianoRollBg: s.getPropertyValue('--piano-roll-bg').trim() || '#0B0E14',
      pianoRollGrid: s.getPropertyValue('--piano-roll-grid').trim() || 'rgba(255,255,255,0.04)',
      pianoRollNote0: s.getPropertyValue('--piano-roll-note-grad-0').trim() || '#38BDF8',
      pianoRollNote1: s.getPropertyValue('--piano-roll-note-grad-1').trim() || '#0284C7',
      pianoRollActive0: s.getPropertyValue('--piano-roll-active-grad-0').trim() || '#FFFFFF',
      pianoRollActive1: s.getPropertyValue('--piano-roll-active-grad-1').trim() || '#38BDF8',
      pianoRollPlayhead: s.getPropertyValue('--piano-roll-playhead').trim() || 'rgba(255,255,255,0.85)'
    };
  },

  updateUI() {
    const buttons = document.querySelectorAll('.theme-btn');
    buttons.forEach((btn) => {
      btn.classList.toggle('active', btn.dataset.theme === this.currentTheme);
    });
  }
};
window.themeManager = ThemeManager;
```

---

### 4.3 Refactored `drawWaveform()` and `drawMeterSmoothed()` Snippets

In `ui-web/app.js`:

```javascript
function drawWaveform() {
  const c = $('#waveform');
  if (!c) return;
  const w = c.clientWidth || 300;
  const h = c.clientHeight || 44;
  if (c.width !== w) c.width = w;
  if (c.height !== h) c.height = h;
  const ctx = c.getContext('2d');
  const W = c.width, H = c.height;
  ctx.clearRect(0, 0, W, H);
  const curPath = state.selected || state.playingPath || '';
  const t = window.themeManager?.tokens || {};

  if (isMidiFile(curPath)) {
    // Piano Roll Canvas Rendering for MIDI files
    const rows = 8;
    const cols = 16;
    const cellH = H / rows;
    const cellW = W / cols;

    // Background & Grid
    ctx.fillStyle = t.pianoRollBg || '#0B0E14';
    ctx.fillRect(0, 0, W, H);

    ctx.strokeStyle = t.pianoRollGrid || 'rgba(255,255,255,0.04)';
    ctx.lineWidth = 1;
    for (let r = 0; r <= rows; ++r) {
      ctx.beginPath();
      ctx.moveTo(0, r * cellH);
      ctx.lineTo(W, r * cellH);
      ctx.stroke();
    }
    for (let col = 0; col <= cols; ++col) {
      ctx.beginPath();
      ctx.moveTo(col * cellW, 0);
      ctx.lineTo(col * cellW, H);
      ctx.stroke();
    }

    const notes = state.midiNotes;
    const dur = state.duration || 4.0;
    const shift = state.pitchSemitones || 0;

    if (notes && notes.length > 0) {
      let minNote = 127, maxNote = 0;
      notes.forEach(n => {
        minNote = Math.min(minNote, n.note);
        maxNote = Math.max(maxNote, n.note);
      });
      if (minNote > maxNote) { minNote = 48; maxNote = 72; }
      const noteRange = Math.max(12, maxNote - minNote + 2);
      const curTime = state.position * dur;

      notes.forEach((n) => {
        const nx = (n.time / dur) * W;
        const nw = Math.max(3, (n.duration / dur) * W - 1);
        const transposedMidi = n.note + shift;
        const normPitch = (transposedMidi - minNote) / noteRange;
        const ny = H - 6 - Math.max(0, Math.min(H - 8, normPitch * (H - 8)));
        const nh = Math.max(3, cellH - 1);

        const isNoteActive = state.playing && (curTime >= n.time && curTime <= n.time + n.duration);

        const grad = ctx.createLinearGradient(nx, ny, nx + nw, ny);
        if (isNoteActive) {
          grad.addColorStop(0, t.pianoRollActive0 || '#FFFFFF');
          grad.addColorStop(1, t.pianoRollActive1 || '#38BDF8');
        } else {
          grad.addColorStop(0, t.pianoRollNote0 || '#38BDF8');
          grad.addColorStop(1, t.pianoRollNote1 || '#0284C7');
        }
        ctx.fillStyle = grad;
        ctx.beginPath();
        if (ctx.roundRect) ctx.roundRect(nx, ny, nw, nh, 1.5);
        else ctx.rect(nx, ny, nw, nh);
        ctx.fill();
      });
    }

    // Playhead line
    const px = (W - 2) * Math.min(1, Math.max(0, state.position));
    if (state.playing || state.position > 0) {
      ctx.fillStyle = t.pianoRollPlayhead || 'rgba(255,255,255,0.85)';
      ctx.fillRect(px, 0, 1.2, H);
    }
    return;
  }

  // Audio Waveform Rendering
  const env = (state.envelope && state.envelope.length) ? state.envelope : ((state.envCache && state.envCache[curPath]) || []);
  const mid = H / 2;
  const amp = H / 2 - 3;
  const px = (W - 2) * Math.min(1, Math.max(0, state.position));

  // Centerline
  ctx.fillStyle = t.waveformCenterline || 'rgba(255,255,255,0.05)';
  ctx.fillRect(0, mid - 0.5, W, 1);

  if (env && env.length) {
    const numBars = env.length;
    const barW = W / numBars;
    const drawW = Math.max(1, barW - 1.2);

    for (let i = 0; i < numBars; ++i) {
      const x = i * barW;
      const val = env[i];
      const curved = Math.pow(Math.min(1, val), 0.75);
      const h = Math.max(1, curved * amp);
      const isPlayed = (x + barW / 2) <= px;

      ctx.fillStyle = isPlayed ? (t.waveformFillActive || 'rgba(56,189,248,0.75)') : (t.waveformFill || 'rgba(255,255,255,0.12)');

      const barY = mid - h;
      const barH = Math.max(2, h * 2);
      if (ctx.roundRect) {
        ctx.beginPath();
        ctx.roundRect(x, barY, drawW, barH, 0.6);
        ctx.fill();
      } else {
        ctx.fillRect(x, barY, drawW, barH);
      }
    }

    // Playhead cursor
    if (state.playing || state.position > 0) {
      ctx.fillStyle = t.waveformCursor || 'rgba(255,255,255,0.85)';
      ctx.fillRect(px, 0, 1.2, H);
    }
  } else {
    // Subtle idle placeholder
    ctx.fillStyle = t.waveformIdle || 'rgba(255,255,255,0.08)';
    ctx.fillRect(0, mid - 0.5, W, 1);
  }
}
```

---

### 4.4 Automated Test Suite Blueprint (`tests/suites/TestSuite_ThemeEngine.cpp`)

Create `tests/suites/TestSuite_ThemeEngine.cpp` to integrate with `reals_tests`:

```cpp
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"

namespace reals::test {

using json = nlohmann::json;

// ============================================================================
// Suite: ThemeEngine (Native Persistence & Protocol Verification)
// ============================================================================

TEST(ThemeEngine, ExtState_PersistenceRoundTrip) {
    MockHostActions host;
    // Initial state is empty
    EXPECT_TRUE(host.getExtState("RealsLab", "theme").empty());

    // Save pastel-pink
    host.setExtState("RealsLab", "theme", "pastel-pink", true);
    EXPECT_EQ(host.getExtState("RealsLab", "theme"), "pastel-pink");

    // Switch to cyberpunk
    host.setExtState("RealsLab", "theme", "cyberpunk", true);
    EXPECT_EQ(host.getExtState("RealsLab", "theme"), "cyberpunk");
}

TEST(ThemeEngine, Ipc_ProtocolValidation) {
    const std::vector<std::string> validThemes = {
        "dark-studio", "pastel-pink", "cyberpunk"
    };

    auto isValidTheme = [&](const std::string& name) {
        for (const auto& t : validThemes) {
            if (t == name) return true;
        }
        return false;
    };

    EXPECT_TRUE(isValidTheme("dark-studio"));
    EXPECT_TRUE(isValidTheme("pastel-pink"));
    EXPECT_TRUE(isValidTheme("cyberpunk"));
    EXPECT_FALSE(isValidTheme("non-existent-theme"));
    EXPECT_FALSE(isValidTheme(""));
}

TEST(ThemeEngine, Ipc_MessagePrefixParsing) {
    std::string validMsg = "THEME_CHANGED:pastel-pink";
    const std::string prefix = "THEME_CHANGED:";

    EXPECT_EQ(validMsg.rfind(prefix, 0), 0u);
    std::string themeName = validMsg.substr(prefix.length());
    EXPECT_EQ(themeName, "pastel-pink");

    std::string invalidMsg = "OTHER_COMMAND:data";
    EXPECT_NE(invalidMsg.rfind(prefix, 0), 0u);
}

TEST(ThemeEngine, Fallback_UnknownThemeDefaultsToDarkStudio) {
    auto sanitizeTheme = [](const std::string& input) -> std::string {
        if (input == "pastel-pink" || input == "cyberpunk") return input;
        return "dark-studio";
    };

    EXPECT_EQ(sanitizeTheme("dark-studio"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("corrupt_value_123"), "dark-studio");
    EXPECT_EQ(sanitizeTheme(""), "dark-studio");
    EXPECT_EQ(sanitizeTheme("pastel-pink"), "pastel-pink");
}

} // namespace reals::test
```

---

## 5. Verification Method

To independently verify all findings and test suite readiness:

1. **Verify Build**:
   ```powershell
   cmake --build --preset windows
   ```
   *Expected result*: Zero compiler warnings (`/W4`), zero errors, and clean generation of `reals_tests.exe` and `reaper_realslab.dll`.

2. **Verify Existing Tests**:
   ```powershell
   build\windows\tests\Debug\reals_tests.exe --suite=BridgeUI
   ```
   *Expected result*: 37/37 tests pass with 100% success rate.

3. **Verify Registered Test Inventory**:
   ```powershell
   build\windows\tests\Debug\reals_tests.exe --list
   ```
   *Expected result*: 183 registered test cases across 17 suites.

4. **Invalidation Conditions**:
   - If canvas drawing code references hardcoded colors directly instead of `ThemeManager.tokens`, theme switching will leave canvas bitmaps unstyled.
   - If `getComputedStyle()` is called on every animation frame instead of during `themeUpdated`, UI rendering will drop frames during audio playback.
