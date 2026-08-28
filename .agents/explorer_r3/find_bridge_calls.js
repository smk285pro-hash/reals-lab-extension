const fs = require('fs');
const appJs = fs.readFileSync('ui-web/app.js', 'utf8');

const lines = appJs.split('\n');
lines.forEach((line, idx) => {
  if (line.includes('bridge(')) {
    const lineNum = idx + 1;
    const hasCatch = line.includes('.catch') || line.includes('try') || lines[idx-1]?.includes('try') || lines[idx-2]?.includes('try');
    console.log(`app.js:${lineNum} [hasCatch/try=${hasCatch}]: ${line.trim()}`);
  }
});
