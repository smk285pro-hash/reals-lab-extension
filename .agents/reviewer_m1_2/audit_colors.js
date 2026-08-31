const fs = require('fs');
const path = require('path');

const appCss = fs.readFileSync(path.join(__dirname, '../../ui-web/app.css'), 'utf8');

// Parse CSS rules
const lines = appCss.split('\n');
const colorProps = new Set([
  'color', 'background', 'background-color', 'border', 'border-color',
  'border-top', 'border-bottom', 'border-left', 'border-right',
  'border-top-color', 'border-bottom-color', 'border-left-color', 'border-right-color',
  'outline', 'outline-color', 'box-shadow', 'text-shadow', 'fill', 'stroke',
  'caret-color', 'accent-color'
]);

const colorHexOrRgbRegex = /#(?:[0-9a-fA-F]{3,8})\b|rgba?\([^)]+\)|hsla?\([^)]+\)/g;

const findings = [];

lines.forEach((rawLine, idx) => {
  const lineNum = idx + 1;
  const line = rawLine.trim();

  // skip comments, @font-face url, data:image
  if (line.startsWith('/*') || line.startsWith('*') || line.startsWith('src:') || line.includes('data:image')) return;

  // Check if line contains a CSS property declaration
  const colonIdx = line.indexOf(':');
  if (colonIdx === -1) return;

  const propName = line.slice(0, colonIdx).trim().toLowerCase();
  const propVal = line.slice(colonIdx + 1).trim();

  // If property name matches colorProps or value has color keywords
  const matches = propVal.match(colorHexOrRgbRegex);
  if (matches) {
    // Check if matches are inside var() or if they are hardcoded
    // Let's filter out gradients that are masks (like mask-image), or transparent/inherit/currentColor
    if (propName.includes('mask')) return; // mask-image gradients are alpha masks, not colors
    
    // Check if the match is a hardcoded color
    matches.forEach(m => {
      // Is it inside a fallback in var(--something, fallback)? Or direct value?
      findings.push({
        lineNum,
        propName,
        propVal,
        matchedColor: m
      });
    });
  }
});

console.log('=== CSS PROPERTY COLOR AUDIT ===');
console.log('Total color occurrences found:', findings.length);
findings.forEach(f => {
  console.log(`L${f.lineNum}: [${f.propName}] -> ${f.propVal} (Matched: ${f.matchedColor})`);
});
