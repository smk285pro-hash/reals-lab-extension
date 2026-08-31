const fs = require('fs');

console.log('=== CHECK 1: TOKEN PARITY ACROSS THEMES ===');
const tokensCss = fs.readFileSync('ui-web/tokens.css', 'utf8');
const themeBlocks = {};
let currentTheme = null;

tokensCss.split('\n').forEach((line) => {
  const trimmed = line.trim();
  if (trimmed.startsWith('/*') || trimmed.startsWith('*') || !trimmed) return;
  
  if (trimmed.includes('{')) {
    const sel = trimmed.split('{')[0].trim();
    currentTheme = sel;
    if (!themeBlocks[currentTheme]) themeBlocks[currentTheme] = {};
  } else if (trimmed.includes('}')) {
    currentTheme = null;
  } else if (currentTheme && trimmed.startsWith('--')) {
    const [varName, ...rest] = trimmed.split(':');
    const val = rest.join(':').replace(';', '').trim();
    themeBlocks[currentTheme][varName.trim()] = val;
  }
});

const themeKeys = Object.keys(themeBlocks);
console.log('Detected theme blocks:', themeKeys);
themeKeys.forEach(t => {
  console.log(t, 'token count:', Object.keys(themeBlocks[t]).length);
});

const baseTheme = themeKeys[0];
const baseTokens = Object.keys(themeBlocks[baseTheme]);

for (let i = 1; i < themeKeys.length; i++) {
  const t = themeKeys[i];
  const missing = baseTokens.filter(k => !(k in themeBlocks[t]));
  const extra = Object.keys(themeBlocks[t]).filter(k => !baseTokens.includes(k));
  console.log(`Theme [${t}] vs [${baseTheme}]:`);
  console.log(`  Missing tokens (${missing.length}):`, missing);
  console.log(`  Extra tokens (${extra.length}):`, extra);
}

console.log('\n=== CHECK 2: UNDEFINED VARIABLES IN APP.CSS ===');
const appCss = fs.readFileSync('ui-web/app.css', 'utf8');
const allDefinedVars = new Set(baseTokens);
const varRegex = /var\(\s*(--[a-zA-Z0-9_-]+)/g;
const usedVars = new Set();
let match;
while ((match = varRegex.exec(appCss)) !== null) {
  usedVars.add(match[1]);
}
console.log('Total unique var() usages in app.css:', usedVars.size);
const undefinedVars = Array.from(usedVars).filter(v => !allDefinedVars.has(v));
console.log(`Undefined vars (${undefinedVars.length}):`, undefinedVars);

console.log('\n=== CHECK 3: HARDCODED COLORS IN APP.CSS ===');
const lines = appCss.split('\n');
const colorIssues = [];
let inComment = false;
let inFontFace = false;

lines.forEach((rawLine, idx) => {
  let line = rawLine.trim();
  if (line.startsWith('/*')) inComment = true;
  if (line.endsWith('*/')) { inComment = false; return; }
  if (inComment) return;
  if (line.includes('@font-face')) inFontFace = true;
  if (inFontFace) {
    if (line.includes('}')) inFontFace = false;
    return;
  }
  if (line.includes('unicode-range') || line.includes('data:image/svg+xml')) return;

  // Check for hex colors
  const hexMatches = line.match(/#[0-9a-fA-F]{3,8}\b/g);
  if (hexMatches) {
    // Filter out ID selectors like #search, #app, #btnPlay, #roots, #preview, etc.
    const actualHex = hexMatches.filter(h => {
      // In CSS, an ID selector starts with # followed by letters/digits, but hex colors are 3,4,6,8 hex digits
      // Check if it's a property value (e.g. after a colon)
      const colonIdx = line.indexOf(':');
      if (colonIdx === -1) return false;
      const valPart = line.slice(colonIdx);
      return valPart.includes(h);
    });
    if (actualHex.length > 0) {
      colorIssues.push({ line: idx + 1, content: rawLine, matches: actualHex });
    }
  }

  // Check for rgba / rgb / hsla / hsl that are not inside linear-gradient masks or var()
  // Wait, let's see any rgb/rgba in value parts
  const colonIdx = line.indexOf(':');
  if (colonIdx !== -1) {
    const valPart = line.slice(colonIdx);
    const rgbMatches = valPart.match(/(?:rgba?|hsla?)\s*\([^\)]+\)/g);
    if (rgbMatches) {
      // Exclude mask linear-gradients if standard alpha masks:
      const nonMaskRgb = rgbMatches.filter(r => !line.includes('mask-image') && !line.includes('-webkit-mask-image'));
      if (nonMaskRgb.length > 0) {
        colorIssues.push({ line: idx + 1, content: rawLine, matches: nonMaskRgb });
      }
    }
  }
});

console.log('Hardcoded color findings count:', colorIssues.length);
colorIssues.forEach(c => console.log(`  Line ${c.line}: ${c.content.trim()} => matches: ${JSON.stringify(c.matches)}`));

console.log('\n=== CHECK 4: SVG ICONS IN INDEX.HTML & APP.JS ===');
const indexHtml = fs.readFileSync('ui-web/index.html', 'utf8');
const appJs = fs.readFileSync('ui-web/app.js', 'utf8');

function checkSvg(name, content) {
  console.log(`Checking SVGs in ${name}...`);
  // Look for hardcoded fill or stroke colors like fill="#..." or stroke="#..." or fill="rgb(...)"
  const svgMatches = [];
  const svgRe = /(?:fill|stroke)=["']([^"']+)["']/g;
  let m;
  while ((m = svgRe.exec(content)) !== null) {
    const val = m[1].trim();
    if (val !== 'none' && val !== 'currentColor' && !val.startsWith('var(') && !val.startsWith('url(')) {
      svgMatches.push({ match: m[0], val: val });
    }
  }
  console.log(`  Non-token / non-currentColor SVG attributes (${svgMatches.length}):`, svgMatches);
}

checkSvg('index.html', indexHtml);
checkSvg('app.js', appJs);
