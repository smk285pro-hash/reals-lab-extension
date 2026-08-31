const fs = require('fs');
const path = require('path');

const indexHtml = fs.readFileSync(path.join(__dirname, '../../ui-web/index.html'), 'utf8');
const appJs = fs.readFileSync(path.join(__dirname, '../../ui-web/app.js'), 'utf8');

console.log('=== 1. HTML INLINE STYLES & SVG AUDIT ===');
const styleAttrRegex = /style=\"([^\"]+)\"/g;
let match;
while ((match = styleAttrRegex.exec(indexHtml)) !== null) {
  console.log('Inline style in index.html:', match[1]);
}

const svgColorRegex = /(?:fill|stroke)=\"([^\"]+)\"/g;
const svgColors = new Set();
while ((match = svgColorRegex.exec(indexHtml)) !== null) {
  svgColors.add(match[1]);
}
console.log('SVG fill/stroke in index.html:', Array.from(svgColors));

console.log('\n=== 2. APP.JS SVG & COLOR AUDIT ===');
// Search for fill= or stroke= in app.js
const jsSvgColors = new Set();
while ((match = svgColorRegex.exec(appJs)) !== null) {
  jsSvgColors.add(match[1]);
}
console.log('SVG fill/stroke in app.js:', Array.from(jsSvgColors));

// Check TAB_ICONS in app.js
const tabIconsIdx = appJs.indexOf('TAB_ICONS');
if (tabIconsIdx !== -1) {
  console.log('\nTAB_ICONS snippet:\n', appJs.slice(tabIconsIdx, tabIconsIdx + 600));
}
