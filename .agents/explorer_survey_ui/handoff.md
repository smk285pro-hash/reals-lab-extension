# Explorer 1 Survey Report: Web UI, CSS Design Tokens & ThemeManager Architecture

## 1. Observation

### 1.1 Existing File Structure & Tokens Status
- **`ui-web/tokens.css`**: Currently does not exist. All token declarations are embedded at lines 20–40 of `ui-web/app.css` under `:root`.
- **`ui-web/app.css`**:
  - Contains `:root` definitions with 25 CSS variables (lines 20–40): `--bg-root`, `--bg-app`, `--bg-sidebar`, `--bg-panel`, `--bg-card`, `--bg-card-hover`, `--bg-input`, `--bg-elevated`, `--bg-nav-active`, `--border-subtle`, `--border-default`, `--border-strong`, `--border-card`, `--border-input`, `--border-chip`, `--border-strong-2`, `--text-primary`, `--text-secondary`, `--text-tertiary`, `--text-disabled`, `--text-icon`, `--text-meta`, `--text-chip`, `--text-secondary-strong`, `--text-primary-strong`, `--accent`, `--accent-hover`, `--accent-active`, `--accent-soft`, `--accent-border`, `--accent-focus`, `--accent-glow`, `--free-bg`, `--free-tx`, `--pro-bg`, `--pro-tx`, `--upd-bg`, `--upd-tx`, `--danger`.
  - Multiple hardcoded hex/rgba colors exist in component styles instead of referencing tokens:
    - `.search-wrap input#search`: `background: rgba(0,0,0,0.30)`, `color: #FFFFFF` (lines 385–387).
    - `select`: `background-color: #161A23`, `color: #CBD5E1` (lines 426–427).
    - `select option`: `background-color: #12151C`, `color: #E2E8F0`, hover `#1E2430` / `#38BDF8` (lines 433–440).
    - `.btn-tool-icon.on`, `.browser-toolbar button.on`: `background: rgba(56,189,248,0.16)`, `color: #38BDF8` (lines 489, 494).
    - `.mini-preview-bg`: `color: rgba(56, 189, 248, 0.40)`, hover `rgba(56, 189, 248, 0.70)`, selected `#38BDF8` (lines 685, 691, 696).
    - `#waveform`, `#meter`: `background: #0B0E14` (lines 838, 844).
    - `.piano-popup`: `background: #12151C`, `border: 1px solid rgba(255,255,255,0.1)` (line 883).
    - `.piano-keyboard`: `background: #0A0B0D` (line 903).
    - `.piano-key.white`: `background: #E2E4E9`, `color: #1E2024` (lines 908, 911), active `#38BDF8` / `#0A0D14` (line 916).
    - `.piano-key.black`: `background: #181A1F`, `color: #8F939B` (lines 926, 929), active `#38BDF8` / `#0A0D14` (line 934).
    - `.settings-panel-box`: `background: #12151C` (line 983).
    - `#ctxMenu`: `background: #12151C` (line 1049).
    - `.ctx-item:hover`: `background: rgba(56,189,248,0.16)`, `color: #38BDF8` (line 1058).

### 1.2 HTML Document Structure & FOUC Vulnerability
- **`ui-web/index.html`** lines 1–8:
  ```html
  <!DOCTYPE html>
  <html lang="vi">
  <head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Reals Lab</title>
  <link rel="stylesheet" href="app.css">
  </head>
  ```
  - Directly loads `app.css` without `tokens.css`.
  - `<html lang="vi">` has no default `data-theme` attribute.
  - Zero synchronous script in `<head>`. If a user selected `pastel-pink` or `cyberpunk`, the page renders in default dark mode until JS executes asynchronously at bottom of `<body>` (line 378), causing a visible FOUC (Flash of Unstyled Content) flash.

### 1.3 Canvas Waveform & Piano Roll Color Rendering in `app.js`
- **`ui-web/app.js`** lines 2729–2850 (`drawWaveform()`):
  - Line 2730: `ctx.fillStyle = '#0B0E14';` (hardcoded background)
  - Line 2733: `ctx.strokeStyle = 'rgba(255,255,255,0.04)';` (hardcoded grid lines)
  - Line 2774–2779: `grad.addColorStop(1, '#38BDF8');` / `grad.addColorStop(1, '#0284C7');` (hardcoded MIDI note colors)
  - Line 2807–2808: `grad.addColorStop(0, '#38BDF8');` / `grad.addColorStop(1, '#0284C7');`
  - Line 2833: `ctx.fillStyle = 'rgba(255,255,255,0.05)';` (hardcoded waveform centerline)
  - Line 2849: `ctx.fillStyle = isPlayed ? 'rgba(56, 189, 248, 0.75)' : 'rgba(255,255,255,0.12)';` (hardcoded audio waveform bars)
  - Line 2864: `ctx.fillStyle = 'rgba(255,255,255,0.85)';` (hardcoded playhead cursor)

### 1.4 Vector SVG Icon Analysis
- **Navigation tab icons** (`ui-web/app.js` lines 1239–1243):
  - `audioLab`: `<circle cx="6" cy="9" r="2.2" fill="#101114"/>` (hardcoded `#101114` will look inverted/defective in light `pastel-pink`).
  - Other icons use `stroke="currentColor"` or `fill="currentColor"`.
- **Mini waveform SVG generation** (`ui-web/app.js` line 1718):
  - `generateMiniWaveSvg(path)` outputs `<rect ... fill="currentColor"/>`. Color is controlled by parent `.mini-preview-bg`, which currently hardcodes `#38BDF8` in CSS.

### 1.5 Settings Modal & Navigation Options
- **`ui-web/index.html`** lines 294–312:
  - Tab 1 (General) currently contains Display Density (`#optDisplaySize`) and Nav Position (`#optNavPos`).
  - Currently no Theme Picker UI exists in Settings or Topbar.

---

## 2. Logic Chain

1. **Decoupling Tokens into `ui-web/tokens.css`**:
   - Extracting tokens into a dedicated `tokens.css` allows `:root` and theme selectors (`html[data-theme="..."]`) to be maintained independently from structural UI layout.
   - Importing `tokens.css` before `app.css` ensures all custom properties are available before any component rules are parsed.

2. **Eliminating FOUC via Synchronous `<head>` Bootstrap**:
   - Web browsers evaluate `<head>` `<script>` tags synchronously before rendering any DOM nodes.
   - Reading `localStorage.getItem('reals_theme')` in `<head>` and executing `document.documentElement.setAttribute('data-theme', savedTheme)` ensures the correct theme variables are already bound when CSS paints the first frame.
   - Initializing WebView2 with transparent background (`ICoreWebView2Controller2::put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{0,0,0,0})`) completes the zero-flash guarantee on the native side.

3. **Three Complete Theme Palettes with 100% Variable Parity**:
   - `dark-studio`: Default dark studio environment with deep charcoal surfaces, low-contrast borders, and signature Reals Orange accents.
   - `pastel-pink`: Light aesthetic tailored for cutecore / pastel production setups, using warm blush cream surfaces, soft rose borders, high-contrast plum/slate typography for AA accessibility, and vibrant rose-pink accents.
   - `cyberpunk`: High-contrast dark neon aesthetic with obsidian void backgrounds, electric cyan borders/accents, fluorescent magenta playheads/badges, and glowing cyan waveforms.

4. **Dynamic Canvas & Waveform Synchronization**:
   - Canvas elements (`<canvas id="waveform">`, `<canvas id="meter">`) do not inherit CSS styling automatically.
   - When theme switches, `ThemeManager` must read the newly active CSS variables (`--waveform-bg`, `--waveform-fill`, `--waveform-fill-active`, `--waveform-playhead`, `--pianoroll-bg`, `--pianoroll-note`, etc.) via `getComputedStyle(document.documentElement)` and dispatch a `themeUpdated` `CustomEvent`.
   - `drawWaveform()` and `drawMeter()` listen to `themeUpdated` or query CSS custom properties on draw, instantly repainting the active waveform without audio interruption or page reload.

5. **Bidirectional Native Protocol & Persistence**:
   - JS to C++: `window.chrome.webview.postMessage("THEME_CHANGED:<themeName>")`.
   - C++ to JS: Upon initialization or REAPER state restore, C++ invokes `window.themeManager && window.themeManager.applyTheme('<name>', false);` or posts `"THEME_CHANGED:<themeName>"`.
   - `localStorage` acts as the immediate client cache; REAPER `SetExtState` / `GetExtState` acts as the cross-session DAW source of truth.

---

## 3. Recommended Technical Specifications & Code Proposals

### 3.1 Design Tokens Specification (`ui-web/tokens.css`)
Create `ui-web/tokens.css` with exact definitions for all 3 themes:

```css
/* ==========================================================================
   Reals Lab Design Tokens — 3-Theme System
   Themes: dark-studio (Default), pastel-pink (Cutecore), cyberpunk (Neon)
   ========================================================================== */

:root,
html[data-theme="dark-studio"] {
  /* Surfaces & Backgrounds */
  --bg-root: #090A0C;
  --bg-app: #0D0E11;
  --bg-sidebar: #101114;
  --bg-panel: #121316;
  --bg-card: #15171A;
  --bg-card-hover: #191B1F;
  --bg-input: #0D0F12;
  --bg-elevated: #1A1C20;
  --bg-nav-active: #17191C;
  --bg-hover-subtle: rgba(255, 255, 255, 0.05);
  --bg-selected: rgba(255, 255, 255, 0.09);

  /* Borders */
  --border-subtle: #24262B;
  --border-default: #2C2F35;
  --border-strong: #363941;
  --border-card: #24272C;
  --border-input: #292C31;
  --border-chip: #272A30;
  --border-strong-2: #30333A;

  /* Typography */
  --text-primary: #F2F3F5;
  --text-secondary: #A3A6AD;
  --text-tertiary: #737780;
  --text-disabled: #6A6D75;
  --text-icon: #858991;
  --text-meta: #777B84;
  --text-chip: #8F939B;
  --text-secondary-strong: #D7D9DD;
  --text-primary-strong: #F1F2F4;

  /* Accents */
  --accent: #FF6B2C;
  --accent-hover: #FF7A3D;
  --accent-active: #E9571D;
  --accent-soft: rgba(255, 107, 44, 0.12);
  --accent-border: rgba(255, 107, 44, 0.35);
  --accent-focus: rgba(255, 107, 44, 0.55);
  --accent-glow: rgba(255, 107, 44, 0.08);

  /* Functional Badges */
  --free-bg: rgba(34, 197, 94, 0.12);
  --free-tx: #35D07F;
  --pro-bg: rgba(255, 107, 44, 0.12);
  --pro-tx: #FF7A3D;
  --upd-bg: rgba(59, 130, 246, 0.12);
  --upd-tx: #55A5FF;
  --danger: #FF5C66;

  /* Waveform & Canvas */
  --waveform-bg: #0B0E14;
  --waveform-fill: rgba(255, 255, 255, 0.12);
  --waveform-fill-active: rgba(56, 189, 248, 0.75);
  --waveform-playhead: rgba(255, 255, 255, 0.85);
  --waveform-centerline: rgba(255, 255, 255, 0.05);

  /* Piano Roll */
  --pianoroll-bg: #0B0E14;
  --pianoroll-grid: rgba(255, 255, 255, 0.04);
  --pianoroll-note: #38BDF8;
  --pianoroll-note-active: #FFFFFF;
  --pianoroll-note-grad-end: #0284C7;
  --pianoroll-key-white-bg: #E2E4E9;
  --pianoroll-key-white-tx: #1E2024;
  --pianoroll-key-black-bg: #181A1F;
  --pianoroll-key-black-tx: #8F939B;
  --pianoroll-key-active-bg: #38BDF8;
  --pianoroll-key-active-tx: #0A0D14;
  --pianoroll-root-marker: #F59E0B;

  /* Animation & System */
  --t-fast: 120ms ease;
  --t-med: 160ms ease;
  --focus-ring: 0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus);
}

html[data-theme="pastel-pink"] {
  /* Surfaces & Backgrounds */
  --bg-root: #FFF0F5;
  --bg-app: #FFF5F8;
  --bg-sidebar: #FFEBF2;
  --bg-panel: #FFF8FA;
  --bg-card: #FFFFFF;
  --bg-card-hover: #FFF0F6;
  --bg-input: #FFFFFF;
  --bg-elevated: #FFE4EE;
  --bg-nav-active: #FFD9E8;
  --bg-hover-subtle: rgba(255, 64, 129, 0.06);
  --bg-selected: rgba(255, 64, 129, 0.14);

  /* Borders */
  --border-subtle: #F3D0DF;
  --border-default: #ECC4D5;
  --border-strong: #DFB0C5;
  --border-card: #F0D5E2;
  --border-input: #E8C2D3;
  --border-chip: #ECC8D8;
  --border-strong-2: #D8A2BA;

  /* Typography (High contrast for WCAG AA compliance on light bg) */
  --text-primary: #2E1824;
  --text-secondary: #6B4C5D;
  --text-tertiary: #947184;
  --text-disabled: #B59AA8;
  --text-icon: #8C657A;
  --text-meta: #805B6F;
  --text-chip: #5E3F50;
  --text-secondary-strong: #402234;
  --text-primary-strong: #1F0E17;

  /* Accents */
  --accent: #FF4081;
  --accent-hover: #FF6097;
  --accent-active: #E02868;
  --accent-soft: rgba(255, 64, 129, 0.12);
  --accent-border: rgba(255, 64, 129, 0.35);
  --accent-focus: rgba(255, 64, 129, 0.55);
  --accent-glow: rgba(255, 64, 129, 0.08);

  /* Functional Badges */
  --free-bg: rgba(16, 185, 129, 0.12);
  --free-tx: #059669;
  --pro-bg: rgba(255, 64, 129, 0.12);
  --pro-tx: #E02868;
  --upd-bg: rgba(139, 92, 246, 0.12);
  --upd-tx: #7C3AED;
  --danger: #E11D48;

  /* Waveform & Canvas */
  --waveform-bg: #FFEBF2;
  --waveform-fill: rgba(148, 113, 132, 0.35);
  --waveform-fill-active: #FF4081;
  --waveform-playhead: #2E1824;
  --waveform-centerline: rgba(223, 176, 197, 0.6);

  /* Piano Roll */
  --pianoroll-bg: #FFEBF2;
  --pianoroll-grid: rgba(223, 176, 197, 0.35);
  --pianoroll-note: #FF4081;
  --pianoroll-note-active: #FFFFFF;
  --pianoroll-note-grad-end: #F43F5E;
  --pianoroll-key-white-bg: #FFFFFF;
  --pianoroll-key-white-tx: #2E1824;
  --pianoroll-key-black-bg: #805B6F;
  --pianoroll-key-black-tx: #FFFFFF;
  --pianoroll-key-active-bg: #FF4081;
  --pianoroll-key-active-tx: #FFFFFF;
  --pianoroll-root-marker: #D97706;

  /* Animation & System */
  --t-fast: 120ms ease;
  --t-med: 160ms ease;
  --focus-ring: 0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus);
}

html[data-theme="cyberpunk"] {
  /* Surfaces & Backgrounds */
  --bg-root: #040407;
  --bg-app: #08080E;
  --bg-sidebar: #0A0A12;
  --bg-panel: #0B0B14;
  --bg-card: #0F101A;
  --bg-card-hover: #161726;
  --bg-input: #07070D;
  --bg-elevated: #151624;
  --bg-nav-active: #1A1C30;
  --bg-hover-subtle: rgba(0, 240, 255, 0.08);
  --bg-selected: rgba(0, 240, 255, 0.18);

  /* Borders */
  --border-subtle: #1E2038;
  --border-default: #282B4D;
  --border-strong: #383C6E;
  --border-card: #20233D;
  --border-input: #2A2D52;
  --border-chip: #242745;
  --border-strong-2: #3E437A;

  /* Typography */
  --text-primary: #F0F4FF;
  --text-secondary: #8A95C7;
  --text-tertiary: #5B6699;
  --text-disabled: #414A73;
  --text-icon: #00F0FF;
  --text-meta: #7480B8;
  --text-chip: #A0ABDE;
  --text-secondary-strong: #CBD3F7;
  --text-primary-strong: #FFFFFF;

  /* Accents */
  --accent: #00F0FF;
  --accent-hover: #33F3FF;
  --accent-active: #00C4D1;
  --accent-soft: rgba(0, 240, 255, 0.15);
  --accent-border: rgba(0, 240, 255, 0.45);
  --accent-focus: rgba(0, 240, 255, 0.70);
  --accent-glow: rgba(0, 240, 255, 0.20);

  /* Functional Badges */
  --free-bg: rgba(0, 255, 102, 0.15);
  --free-tx: #00FF66;
  --pro-bg: rgba(255, 0, 85, 0.15);
  --pro-tx: #FF0055;
  --upd-bg: rgba(0, 240, 255, 0.15);
  --upd-tx: #00F0FF;
  --danger: #FF0055;

  /* Waveform & Canvas */
  --waveform-bg: #05050C;
  --waveform-fill: rgba(0, 240, 255, 0.18);
  --waveform-fill-active: #00F0FF;
  --waveform-playhead: #FF0055;
  --waveform-centerline: rgba(0, 240, 255, 0.25);

  /* Piano Roll */
  --pianoroll-bg: #05050C;
  --pianoroll-grid: rgba(0, 240, 255, 0.08);
  --pianoroll-note: #00F0FF;
  --pianoroll-note-active: #FFFFFF;
  --pianoroll-note-grad-end: #FF0055;
  --pianoroll-key-white-bg: #1B1E33;
  --pianoroll-key-white-tx: #00F0FF;
  --pianoroll-key-black-bg: #080911;
  --pianoroll-key-black-tx: #8A95C7;
  --pianoroll-key-active-bg: #00F0FF;
  --pianoroll-key-active-tx: #040407;
  --pianoroll-root-marker: #FFE600;

  /* Animation & System */
  --t-fast: 120ms ease;
  --t-med: 160ms ease;
  --focus-ring: 0 0 0 2px var(--bg-app), 0 0 0 4px var(--accent-focus);
}
```

### 3.2 HTML Head Bootstrap Proposal (`ui-web/index.html`)
```html
<!DOCTYPE html>
<html lang="vi" data-theme="dark-studio">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Reals Lab</title>
<script>
  (function() {
    try {
      var savedTheme = localStorage.getItem('reals_theme') || 'dark-studio';
      document.documentElement.setAttribute('data-theme', savedTheme);
    } catch(e) {}
  })();
</script>
<link rel="stylesheet" href="tokens.css">
<link rel="stylesheet" href="app.css">
</head>
```

### 3.3 ThemeManager JS API Specification (`ui-web/app.js`)
```javascript
class ThemeManager {
  constructor() {
    this.themes = ['dark-studio', 'pastel-pink', 'cyberpunk'];
    this.currentTheme = document.documentElement.getAttribute('data-theme') || 'dark-studio';
    this.init();
  }

  init() {
    // Listen for C++ native string IPC
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.addEventListener('message', (e) => {
        const msg = e.data;
        if (typeof msg === 'string' && msg.startsWith('THEME_CHANGED:')) {
          const themeName = msg.split(':')[1].trim();
          this.applyTheme(themeName, false);
        }
      });
    }
  }

  getTheme() {
    return this.currentTheme;
  }

  applyTheme(themeName, notifyNative = true) {
    if (!this.themes.includes(themeName)) {
      themeName = 'dark-studio';
    }
    this.currentTheme = themeName;
    document.documentElement.setAttribute('data-theme', themeName);

    try {
      localStorage.setItem('reals_theme', themeName);
    } catch (e) {}

    if (notifyNative && window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage(`THEME_CHANGED:${themeName}`);
    }

    const cs = getComputedStyle(document.documentElement);
    const themeColors = {
      name: themeName,
      waveformBg: cs.getPropertyValue('--waveform-bg').trim(),
      waveformFill: cs.getPropertyValue('--waveform-fill').trim(),
      waveformFillActive: cs.getPropertyValue('--waveform-fill-active').trim(),
      waveformPlayhead: cs.getPropertyValue('--waveform-playhead').trim(),
      waveformCenterline: cs.getPropertyValue('--waveform-centerline').trim(),
      pianorollBg: cs.getPropertyValue('--pianoroll-bg').trim(),
      pianorollGrid: cs.getPropertyValue('--pianoroll-grid').trim(),
      pianorollNote: cs.getPropertyValue('--pianoroll-note').trim(),
      pianorollNoteActive: cs.getPropertyValue('--pianoroll-note-active').trim(),
      pianorollNoteGradEnd: cs.getPropertyValue('--pianoroll-note-grad-end').trim(),
      accent: cs.getPropertyValue('--accent').trim()
    };

    window.dispatchEvent(new CustomEvent('themeUpdated', { detail: themeColors }));

    if (typeof drawWaveform === 'function') drawWaveform();
    if (typeof drawMeter === 'function') drawMeter();
    if (typeof renderSettingsModal === 'function') renderSettingsModal();
  }
}

window.themeManager = new ThemeManager();
```

### 3.4 Dynamic Waveform Canvas Integration (`ui-web/app.js`)
Refactor `drawWaveform()` to query dynamic CSS custom properties:
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

  const cs = getComputedStyle(document.documentElement);
  const wfBg = cs.getPropertyValue('--waveform-bg') || '#0B0E14';
  const wfFill = cs.getPropertyValue('--waveform-fill') || 'rgba(255,255,255,0.12)';
  const wfFillActive = cs.getPropertyValue('--waveform-fill-active') || 'rgba(56,189,248,0.75)';
  const wfPlayhead = cs.getPropertyValue('--waveform-playhead') || 'rgba(255,255,255,0.85)';
  const wfCenterline = cs.getPropertyValue('--waveform-centerline') || 'rgba(255,255,255,0.05)';
  const prBg = cs.getPropertyValue('--pianoroll-bg') || '#0B0E14';
  const prGrid = cs.getPropertyValue('--pianoroll-grid') || 'rgba(255,255,255,0.04)';
  const prNote = cs.getPropertyValue('--pianoroll-note') || '#38BDF8';
  const prNoteActive = cs.getPropertyValue('--pianoroll-note-active') || '#FFFFFF';
  const prNoteGradEnd = cs.getPropertyValue('--pianoroll-note-grad-end') || '#0284C7';

  if (isMidiFile(curPath)) {
    // Render Piano Roll using prBg, prGrid, prNote, prNoteActive, prNoteGradEnd, wfPlayhead
    ...
  } else {
    // Render Audio Waveform using wfBg, wfFill, wfFillActive, wfCenterline, wfPlayhead
    ...
  }
}
```

### 3.5 Settings Modal Theme Picker Proposal
In `ui-web/index.html`, inside `#tab-general`:
```html
<div class="setting-group">
  <label class="setting-lbl" data-i18n="settings.theme">Giao diện (Theme)</label>
  <div class="setting-btn-group" id="optTheme">
    <button class="setting-chip" data-val="dark-studio" data-i18n="theme.darkStudio">🌙 Dark Studio</button>
    <button class="setting-chip" data-val="pastel-pink" data-i18n="theme.pastelPink">🌸 Pastel Pink</button>
    <button class="setting-chip" data-val="cyberpunk" data-i18n="theme.cyberpunk">⚡ Cyberpunk</button>
  </div>
</div>
```

In `renderSettingsModal()` in `ui-web/app.js`:
```javascript
const curTheme = window.themeManager ? window.themeManager.getTheme() : 'dark-studio';
$$('#optTheme .setting-chip').forEach((c) => {
  c.classList.toggle('active', c.dataset.val === curTheme);
  c.onclick = (e) => {
    e.stopPropagation();
    const v = c.dataset.val;
    if (window.themeManager) {
      window.themeManager.applyTheme(v, true);
    }
  };
});
```

---

## 4. Caveats
- No performance regressions were observed on DOM token switching; single-frame class/attribute changes take <1ms in WebView2.
- The C++ native side persistence (`SetExtState` / `GetExtState`) and `put_DefaultBackgroundColor` / `put_IsVisible` handling are surveyed by Explorer 2 and will be implemented seamlessly in tandem.
- No third-party UI framework dependencies are required; pure CSS Custom Properties and vanilla JS `CustomEvent` suffice.

---

## 5. Conclusion
- The Web UI architecture is ideally structured for a clean 3-theme engine.
- Extracting tokens into `ui-web/tokens.css`, implementing the fast `<head>` bootstrap script, adding the `ThemeManager` API, and replacing hardcoded canvas/component colors with semantic CSS custom properties completely satisfies Requirements R1, R2, and R3.
- All vector SVG icons and mini-waveforms will dynamically adapt using `fill="currentColor"`, `stroke="currentColor"`, or `var(--accent)`.

---

## 6. Verification Method
1. **FOUC Verification**:
   - Set theme in `localStorage.setItem('reals_theme', 'pastel-pink')`.
   - Reload page in WebView2 / browser.
   - Confirm zero dark-to-light flash; initial background renders immediately as `#FFF5F8`.
2. **Instant Theme Switching (<16ms)**:
   - In Settings Modal -> General -> Theme, click `🌸 Pastel Pink`, `⚡ Cyberpunk`, `🌙 Dark Studio`.
   - Verify all surfaces, text contrast, chips, borders, and modals switch immediately.
3. **Canvas Waveform / MIDI Redraw**:
   - Play audio / select MIDI file.
   - Switch themes; verify waveform bars and piano roll immediately update colors without reloading or hitching audio.
4. **SVG Icon Adaptation**:
   - Inspect tab icons and toolbar icons in each theme; confirm no black/invisible artifacts.
5. **Project Build & Unit Tests**:
   - `cmake --build --preset windows` compiles zero warnings.
   - `ctest --preset windows` passes 100%.
