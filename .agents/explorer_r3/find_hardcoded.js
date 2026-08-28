const fs = require('fs');

const appJs = fs.readFileSync('ui-web/app.js', 'utf8');
const indexHtml = fs.readFileSync('ui-web/index.html', 'utf8');

console.log('=== CHECKING index.html FOR HARDCODED TEXT ===');
const htmlLines = indexHtml.split('\n');
htmlLines.forEach((line, idx) => {
  const lineNum = idx + 1;
  // Match text content between tags that is not just whitespace and not inside script/style
  // e.g. <title>Reals Lab</title>
  // title="Cài đặt", title="Thu nhỏ", title="Đóng"
  if (/title="[^"]+"/.test(line)) {
    console.log(`index.html:${lineNum} [hardcoded title attr]: ${line.trim()}`);
  }
  if (/<span class="msg"><b>v0\.2\.0<\/b>/.test(line)) {
    console.log(`index.html:${lineNum} [hardcoded version text]: ${line.trim()}`);
  }
  if (/<button class="lab-tool"/.test(line)) {
    // Check if inner text has hardcoded items
  }
  if (/<span>Reals Lab v0\.1\.0/.test(line)) {
    console.log(`index.html:${lineNum} [hardcoded statusbar text]: ${line.trim()}`);
  }
});

console.log('\n=== CHECKING app.js FOR HARDCODED UI TEXT ===');
const jsLines = appJs.split('\n');
jsLines.forEach((line, idx) => {
  const lineNum = idx + 1;
  // Exclude I18N definition lines (lines 5 to 98)
  if (lineNum >= 5 && lineNum <= 98) return;

  // Look for Vietnamese or English user-facing string literals in DOM manipulation or toast or alerts
  // Examples: toast('...'), agentSay('...'), el('...', '...', '...'), textContent = '...'
  const patterns = [
    /toast\s*\(\s*['"`]([A-Za-zÀ-ỹ\s\(\)\:\—\.\,\!\?]+)['"`]\s*\)/,
    /agentSay\s*\(\s*['"`]([A-Za-zÀ-ỹ\s\(\)\:\—\.\,\!\?]+)['"`]\s*\)/,
    /el\s*\(\s*['"][^'"]+['"]\s*,\s*['"][^'"]*['"]\s*,\s*['"`]([A-Za-zÀ-ỹ\s\(\)\:\—\.\,\!\?]{3,})['"`]\s*\)/,
    /\.textContent\s*=\s*['"`]([A-Za-zÀ-ỹ\s\(\)\:\—\.\,\!\?]{3,})['"`]/,
    /\.innerHTML\s*=\s*['"`]([A-Za-zÀ-ỹ\s\(\)\:\—\.\,\!\?]{3,})['"`]/
  ];

  patterns.forEach(p => {
    const m = line.match(p);
    if (m) {
      // ignore pure selectors, CSS properties, bridge command names, icon svgs
      if (!m[1].includes('<svg') && !m[1].includes('var(') && !m[1].startsWith('#')) {
        console.log(`app.js:${lineNum} [hardcoded string]: "${m[1]}" in: ${line.trim()}`);
      }
    }
  });
});
