const fs = require('fs');
const appJs = fs.readFileSync('ui-web/app.js', 'utf8');

const svgTags = appJs.match(/<svg[\s\S]*?<\/svg>/g) || [];
console.log(`Found ${svgTags.length} SVG strings in app.js:`);
svgTags.forEach((s, idx) => {
  console.log(`\nSVG #${idx + 1}:`);
  console.log(s);
});
