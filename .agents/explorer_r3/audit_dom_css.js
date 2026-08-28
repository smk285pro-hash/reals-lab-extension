const fs = require('fs');

const appJs = fs.readFileSync('ui-web/app.js', 'utf8');
const indexHtml = fs.readFileSync('ui-web/index.html', 'utf8');
const appCss = fs.readFileSync('ui-web/app.css', 'utf8');

console.log('=== 1. CHECKING DUPLICATE IDs IN index.html ===');
const idMatches = indexHtml.match(/id="([^"]+)"/g) || [];
const ids = idMatches.map(m => m.replace(/id="|"/g, ''));
const idCounts = {};
ids.forEach(id => idCounts[id] = (idCounts[id] || 0) + 1);
const duplicates = Object.keys(idCounts).filter(id => idCounts[id] > 1);
console.log('Duplicate IDs:', duplicates.length ? duplicates : 'None');

console.log('\n=== 2. CHECKING ALL ID QUERIES IN app.js VS index.html ===');
const jsIdRegex = /[$#]\{?['"]#([a-zA-Z0-9_-]+)['"]|getElementById\(['"]([a-zA-Z0-9_-]+)['"]\)/g;
let m;
const jsQueriedIds = new Set();
const jsLines = appJs.split('\n');
jsLines.forEach((line, idx) => {
  const r = /['"]#([a-zA-Z0-9_-]+)['"]/g;
  let match;
  while ((match = r.exec(line)) !== null) {
    jsQueriedIds.add({ id: match[1], line: idx + 1, text: line.trim() });
  }
  const r2 = /getElementById\(['"]([a-zA-Z0-9_-]+)['"]\)/g;
  while ((match = r2.exec(line)) !== null) {
    jsQueriedIds.add({ id: match[1], line: idx + 1, text: line.trim() });
  }
});

const missingInHtml = [];
for (const item of jsQueriedIds) {
  // Check if id exists in index.html (or is dynamically created in app.js)
  if (!ids.includes(item.id)) {
    missingInHtml.push(item);
  }
}
console.log('Queried IDs in app.js missing from static index.html:');
missingInHtml.forEach(x => console.log(`  Line ${x.line}: ID "${x.id}" -> ${x.text}`));

console.log('\n=== 3. CHECKING TAB IDs VS PANE IDs ===');
// In app.js: TABS = ['market', 'audioLab', 'agent', 'browser', 'account']
// showTab(t): $('#pane-' + t)
// Let's check panes in index.html:
// pane-market, pane-lab (WAIT! is it pane-lab or pane-audioLab???)
console.log('Checking TABS in app.js vs #pane-* in index.html:');
const tabList = ['market', 'audioLab', 'agent', 'browser', 'account'];
tabList.forEach(t => {
  const expectedId = 'pane-' + t;
  const found = ids.includes(expectedId);
  console.log(`  Tab "${t}" expects ID "#${expectedId}" -> Found in HTML: ${found}`);
});

console.log('\n=== 4. CHECKING NAV POSITION CSS CLASSES ===');
const navClasses = ['nav-top', 'nav-bottom', 'nav-left', 'nav-right'];
navClasses.forEach(nc => {
  const inCss = appCss.includes(nc);
  console.log(`  CSS contains .${nc}: ${inCss}`);
});
