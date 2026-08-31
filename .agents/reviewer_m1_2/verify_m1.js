const fs = require('fs');
const path = require('path');

const tokensCss = fs.readFileSync(path.join(__dirname, '../../ui-web/tokens.css'), 'utf8');
const appCss = fs.readFileSync(path.join(__dirname, '../../ui-web/app.css'), 'utf8');
const indexHtml = fs.readFileSync(path.join(__dirname, '../../ui-web/index.html'), 'utf8');
const appJs = fs.readFileSync(path.join(__dirname, '../../ui-web/app.js'), 'utf8');

console.log('=== 1. TOKENS.CSS PARSING & PARITY ===');

// Extract theme blocks
const themeBlocks = {};
let currentSelector = null;
let currentVars = {};

tokensCss.split('\n').forEach(line => {
  const trimmed = line.trim();
  if (trimmed.endsWith('{')) {
    currentSelector = trimmed.slice(0, -1).trim();
    currentVars = {};
  } else if (trimmed === '}') {
    if (currentSelector) {
      themeBlocks[currentSelector] = currentVars;
      currentSelector = null;
    }
  } else if (currentSelector && trimmed.startsWith('--')) {
    const colonIdx = trimmed.indexOf(':');
    if (colonIdx !== -1) {
      const k = trimmed.slice(0, colonIdx).trim();
      const v = trimmed.slice(colonIdx + 1).replace(/;.*$/, '').trim();
      currentVars[k] = v;
    }
  }
});

const themeKeys = Object.keys(themeBlocks);
console.log('Theme Blocks found:');
themeKeys.forEach(k => {
  console.log(` - "${k}": ${Object.keys(themeBlocks[k]).length} tokens`);
});

const darkStudio = themeBlocks[':root,\nhtml[data-theme="dark-studio"]'] || themeBlocks[':root,\r\nhtml[data-theme="dark-studio"]'] || Object.values(themeBlocks)[0];
const darkStudioVars = Object.keys(darkStudio);

themeKeys.forEach(k => {
  const vars = themeBlocks[k];
  const varKeys = new Set(Object.keys(vars));
  const missing = darkStudioVars.filter(x => !varKeys.has(x));
  const extra = Object.keys(vars).filter(x => !darkStudioVars.includes(x));
  console.log(`Parity for "${k.replace(/\r?\n/g, ' ')}": Missing=${missing.length}, Extra=${extra.length}`);
  if (missing.length > 0) console.log('  Missing:', missing);
  if (extra.length > 0) console.log('  Extra:', extra);
});

console.log('\n=== 2. APP.CSS VARIABLE USAGE AUDIT ===');
const definedTokens = new Set(darkStudioVars);
const varRegex = /var\(\s*(--[a-zA-Z0-9_-]+)/g;
const usedTokens = new Set();
let match;
while ((match = varRegex.exec(appCss)) !== null) {
  usedTokens.add(match[1]);
}

const undefinedTokens = Array.from(usedTokens).filter(x => !definedTokens.has(x));
console.log(`Total unique tokens used in app.css: ${usedTokens.size}`);
console.log(`Undefined tokens referenced in app.css: ${undefinedTokens.length}`);
if (undefinedTokens.length > 0) console.log('  Undefined tokens:', undefinedTokens);

console.log('\n=== 3. HARDCODED COLOR AUDIT IN APP.CSS ===');
// Strip out comments, url(), and CSS variable declarations
const cleanAppCss = appCss
  .replace(/\/\*[\s\S]*?\*\//g, '') // remove comments
  .replace(/url\([^)]+\)/g, '')      // remove url() (e.g. data:image/svg)
  .replace(/--[a-zA-Z0-9_-]+:\s*[^;]+;/g, ''); // remove any custom properties if any

// Look for hex colors #..., rgb(...), rgba(...), hsl(...)
const hexMatches = cleanAppCss.match(/#[0-9a-fA-F]{3,8}\b/g) || [];
// Filter out non-color hex if any (e.g. within unicode ranges or IDs)
// Check properties where hex appears
const suspiciousLines = [];
cleanAppCss.split('\n').forEach((line, idx) => {
  const trimmed = line.trim();
  // Check if line is a CSS declaration (has colon and semicolon)
  if (trimmed.includes(':') && (trimmed.includes('#') || trimmed.includes('rgb(') || trimmed.includes('rgba('))) {
    // Check if it's inside a var() fallback or an actual hardcoded property
    // We want to see all occurrences
    suspiciousLines.push({ lineNum: idx + 1, content: trimmed });
  }
});

console.log(`Suspicious color lines in app.css: ${suspiciousLines.length}`);
suspiciousLines.forEach(l => {
  console.log(`  L${l.lineNum}: ${l.content}`);
});

console.log('\n=== 4. CONTRAST CALCULATION (WCAG 2.1 AA) ===');

function hexToRgb(hex) {
  hex = hex.replace('#', '');
  if (hex.length === 3) hex = hex.split('').map(c => c + c).join('');
  if (hex.length === 6) {
    return [
      parseInt(hex.slice(0, 2), 16),
      parseInt(hex.slice(2, 4), 16),
      parseInt(hex.slice(4, 6), 16)
    ];
  }
  return null;
}

function sRgbLuminance(rgb) {
  const a = rgb.map(v => {
    v /= 255;
    return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
  });
  return a[0] * 0.2126 + a[1] * 0.7152 + a[2] * 0.0722;
}

function contrastRatio(hex1, hex2) {
  const rgb1 = hexToRgb(hex1);
  const rgb2 = hexToRgb(hex2);
  if (!rgb1 || !rgb2) return null;
  const l1 = sRgbLuminance(rgb1);
  const l2 = sRgbLuminance(rgb2);
  const brightest = Math.max(l1, l2);
  const darkest = Math.min(l1, l2);
  return (brightest + 0.05) / (darkest + 0.05);
}

// Calculate contrast for main surfaces and texts in each theme
themeKeys.forEach(tKey => {
  const themeName = tKey.replace(/\r?\n/g, ' ');
  const vars = themeBlocks[tKey];
  console.log(`\nContrast check for: ${themeName}`);
  
  const bgApp = vars['--bg-app'];
  const bgCard = vars['--bg-card'];
  const bgInput = vars['--bg-input'];
  const bgSidebar = vars['--bg-sidebar'];
  const textPrimary = vars['--text-primary'];
  const textSecondary = vars['--text-secondary'];
  const textTertiary = vars['--text-tertiary'];
  const textDisabled = vars['--text-disabled'];
  const accent = vars['--accent'];

  const pairs = [
    { label: 'text-primary on bg-app', fg: textPrimary, bg: bgApp, min: 4.5 },
    { label: 'text-secondary on bg-app', fg: textSecondary, bg: bgApp, min: 4.5 },
    { label: 'text-primary on bg-card', fg: textPrimary, bg: bgCard, min: 4.5 },
    { label: 'text-secondary on bg-card', fg: textSecondary, bg: bgCard, min: 4.5 },
    { label: 'text-primary on bg-sidebar', fg: textPrimary, bg: bgSidebar, min: 4.5 },
    { label: 'text-secondary on bg-sidebar', fg: textSecondary, bg: bgSidebar, min: 4.5 },
    { label: 'text-primary on bg-input', fg: textPrimary, bg: bgInput, min: 4.5 },
    { label: 'accent on bg-app', fg: accent, bg: bgApp, min: 3.0 },
    { label: 'accent on bg-card', fg: accent, bg: bgCard, min: 3.0 }
  ];

  pairs.forEach(p => {
    if (p.fg && p.bg && p.fg.startsWith('#') && p.bg.startsWith('#')) {
      const cr = contrastRatio(p.fg, p.bg);
      const pass = cr >= p.min ? 'PASS' : 'FAIL';
      console.log(`  ${p.label} (${p.fg} on ${p.bg}): ${cr.toFixed(2)}:1 -> [${pass}] (req >= ${p.min}:1)`);
    } else {
      console.log(`  ${p.label} (${p.fg} on ${p.bg}): [SKIP - non-hex or rgba]`);
    }
  });
});
