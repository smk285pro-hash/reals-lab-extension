# Empirical Challenger Report — Milestone 1: CSS Design Tokens & Theme Palettes

## 1. Observation
We conducted an exhaustive empirical code audit and token resolution analysis on `ui-web/tokens.css` (333 lines, 10,206 bytes) and `ui-web/app.css` (1,155 lines, 46,709 bytes).

### A. Token Definitions in `ui-web/tokens.css`
1. **Theme Blocks Detected**:
   - `:root, html[data-theme="dark-studio"]` (Default Dark Studio): 82 CSS Custom Properties
   - `html[data-theme="pastel-pink"]` (Light Cutecore Pastel Pink): 82 CSS Custom Properties
   - `html[data-theme="cyberpunk"]` (Neon High-Contrast Cyberpunk): 82 CSS Custom Properties
2. **Token Parity Matrix**:
   - Missing in `pastel-pink`: `[]` (0 missing)
   - Missing in `cyberpunk`: `[]` (0 missing)
   - Extra in `pastel-pink`: `[]` (0 extra)
   - Extra in `cyberpunk`: `[]` (0 extra)
   - **Parity Result**: Exact 100% token definition parity (82/82 variables mapped across all 3 palettes).

### B. Variable Reference Resolution in `ui-web/app.css`
1. Total unique `var(--...)` references used across 1,155 lines in `app.css`: **70 unique variables**.
2. **Resolution breakdown**:
   - 66 tokens resolve directly to semantic design tokens defined in `tokens.css`.
   - 4 tokens (`--row-h`, `--row-fs`, `--tree-fs`, `--tree-pad`) resolve to component sizing variables declared in `app.css`'s own `:root` (lines 589-595) and responsive modifier classes `body.size-small` / `body.size-large` (lines 597-611).
   - Undefined `var(--...)` tokens: **0**.

### C. Hardcoded Color Analysis in `ui-web/app.css`
1. **Hex `#...` Colors**:
   - Scanned all CSS property values in `app.css` for hex color literals (`#3..8` hex digits).
   - Result: **0** unmigrated hex color declarations. (ID selectors like `#app`, `#topbar`, `#files` and data-URI SVG noise filter fragments were properly separated).
2. **HSL/HSLA Colors**:
   - Result: **0** HSL/HSLA color declarations.
3. **RGB/RGBA Colors**:
   - Property styling declarations (`color`, `background`, `border`, `shadow`, etc.): **0** hardcoded RGB/RGBA values.
   - Lines 666-679 contain `mask-image: linear-gradient(90deg, rgba(0,0,0,0.12) ...)` and `-webkit-mask-image`: these are standard CSS alpha masks controlling horizontal fade opacity for `.mini-preview-bg`, while the foreground color is dynamically assigned via semantic tokens (`color: var(--mini-wave-color)`, `color: var(--mini-wave-hover)`, `color: var(--mini-wave-sel)`).
4. **SVG Vector Icons**:
   - Inspected `ui-web/index.html`: SVG elements dynamically use `currentColor` and semantic CSS tokens without hardcoded inline color overrides.

---

## 2. Logic Chain
1. **From Token Coverage to Completeness**: Because every one of the 82 tokens defined in `:root` / `dark-studio` is explicitly overridden in both `pastel-pink` and `cyberpunk` with zero missing keys, theme switching between all 3 modes is guaranteed to provide full palette coverage without falling back to unstyled browser defaults.
2. **From Variable Usage to Safe Rendering**: Because all 70 unique `var(--...)` expressions in `app.css` resolve to either `tokens.css` (66 tokens) or component size definitions in `app.css` (4 tokens), the browser CSS parser encounters zero unresolved variables, preventing rendering drops or broken layouts.
3. **From Color Elimination to Theme Compliance**: Because 100% of color, background, border, and shadow properties in `app.css` reference CSS custom properties rather than raw color literals, switching the `data-theme` attribute on `<html>` instantly repaints the entire DOM UI hierarchy with zero lingering hardcoded dark/light colors.

---

## 3. Caveats
- `ui-web/app.js` contains hardcoded colors in legacy canvas routines (`drawWaveform()`, `drawMeterSmoothed()`), tag presets (`TAG_COLORS`), and market demo items. These are explicitly scheduled for refactoring in **Milestone 3 (Dynamic Canvas & Settings Theme Picker)** per `PROJECT.md`.
- No modifications were made to implementation code (strict review-only constraint observed).

---

## 4. Conclusion
**Verdict: APPROVE**

Milestone 1 satisfies all acceptance criteria in `ORIGINAL_REQUEST.md` and `PROJECT.md`:
- `ui-web/tokens.css` defines a complete 82-token design system across 3 distinct palettes (`dark-studio`, `pastel-pink`, `cyberpunk`).
- `ui-web/app.css` has eliminated 100% of hardcoded color styles in favor of semantic CSS custom properties.
- Zero unresolved CSS variable references exist.

---

## 5. Verification Method
To independently reproduce and verify these empirical findings, execute the following script from the repository root:

```powershell
@'
const fs = require('fs');
const tokensCss = fs.readFileSync('ui-web/tokens.css', 'utf8');
const appCss = fs.readFileSync('ui-web/app.css', 'utf8');

// 1. Verify token parity across themes
function parseTokens(css) {
  const blocks = {};
  const blockRegex = /([^{]+)\{([^}]+)\}/g;
  let match;
  while ((match = blockRegex.exec(css)) !== null) {
    const sel = match[1].trim();
    const body = match[2];
    const tokens = {};
    const tRegex = /(--[a-zA-Z0-9_-]+)\s*:\s*([^;]+);/g;
    let tm;
    while ((tm = tRegex.exec(body)) !== null) {
      tokens[tm[1].trim()] = tm[2].trim();
    }
    blocks[sel] = tokens;
  }
  return blocks;
}

const themeBlocks = parseTokens(tokensCss);
const darkStudio = Object.entries(themeBlocks).find(([k]) => k.includes('dark-studio'))?.[1] || {};
const pastelPink = Object.entries(themeBlocks).find(([k]) => k.includes('pastel-pink'))?.[1] || {};
const cyberpunk  = Object.entries(themeBlocks).find(([k]) => k.includes('cyberpunk'))?.[1] || {};

const darkKeys = Object.keys(darkStudio);
const pastelKeys = Object.keys(pastelPink);
const cyberKeys = Object.keys(cyberpunk);

const allTokensTokensCss = new Set([...darkKeys, ...pastelKeys, ...cyberKeys]);
const appTokens = parseTokens(appCss);
const allTokensAppCss = new Set();
Object.values(appTokens).forEach(b => Object.keys(b).forEach(k => allTokensAppCss.add(k)));
const allDefinedTokens = new Set([...allTokensTokensCss, ...allTokensAppCss]);

// 2. Verify var usages
const varRegex = /var\(\s*(--[a-zA-Z0-9_-]+)(?:\s*,\s*([^)]+))?\)/g;
const usedVars = new Map();
appCss.split(/\r?\n/).forEach((line, idx) => {
  let vm;
  while ((vm = varRegex.exec(line)) !== null) {
    const vName = vm[1];
    if (!usedVars.has(vName)) usedVars.set(vName, []);
    usedVars.get(vName).push(idx + 1);
  }
});
const undefinedVars = [...usedVars.keys()].filter(k => !allDefinedTokens.has(k));

// 3. Verify hardcoded colors
const lines = appCss.split(/\r?\n/);
const colorViolations = [];
const allowedMaskLines = [666, 667, 672, 673, 678, 679];

lines.forEach((line, idx) => {
  const lineNum = idx + 1;
  let l = line.replace(/\/\*.*?\*\//g, '').trim();
  if (!l || l.startsWith('@') || l.startsWith('unicode-range')) return;
  l = l.replace(/url\(\s*['"]?data:image\/svg\+xml[^'")]+['"]?\s*\)/gi, '');
  const colonIdx = l.indexOf(':');
  if (colonIdx === -1) return;
  const val = l.substring(colonIdx + 1).trim();

  const hexMatches = val.match(/#[0-9a-fA-F]{3,8}\b/g);
  if (hexMatches) colorViolations.push({ lineNum, type: 'HEX', val });

  const rgbMatches = val.match(/rgba?\s*\([^)]*\)/gi);
  if (rgbMatches && !allowedMaskLines.includes(lineNum)) colorViolations.push({ lineNum, type: 'RGB/RGBA', val });

  const hslMatches = val.match(/hsla?\s*\([^)]*\)/gi);
  if (hslMatches) colorViolations.push({ lineNum, type: 'HSL/HSLA', val });
});

console.log(`Tokens per theme: dark=${darkKeys.length}, pastel=${pastelKeys.length}, cyber=${cyberKeys.length}`);
console.log(`Undefined var references in app.css: ${undefinedVars.length}`);
console.log(`Hardcoded color violations in app.css: ${colorViolations.length}`);
'@ | node
```

**Expected Result**:
- Tokens per theme: `dark=82, pastel=82, cyber=82`
- Undefined var references in app.css: `0`
- Hardcoded color violations in app.css: `0`
