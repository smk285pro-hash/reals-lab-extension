const fs = require('fs');
const appJs = fs.readFileSync('ui-web/app.js', 'utf8');
const viJson = JSON.parse(fs.readFileSync('assets/i18n/strings_vi.json', 'utf8'));
const enJson = JSON.parse(fs.readFileSync('assets/i18n/strings_en.json', 'utf8'));
const indexHtml = fs.readFileSync('ui-web/index.html', 'utf8');

// Extract I18N.vi and I18N.en from appJs
const viMatch = appJs.match(/vi:\s*\{([\s\S]*?)\},\s*en:/);
const enMatch = appJs.match(/en:\s*\{([\s\S]*?)\},\s*\};/);

function extractKeys(str) {
  const keys = [];
  const regex = /'([^']+)':/g;
  let m;
  while ((m = regex.exec(str)) !== null) {
    keys.push(m[1]);
  }
  return keys;
}

const jsViKeys = extractKeys(viMatch[1]);
const jsEnKeys = extractKeys(enMatch[1]);
const jsonViKeys = Object.keys(viJson);
const jsonEnKeys = Object.keys(enJson);

// Extract tr('...') calls in app.js
const trKeys = [];
const trRegex = /tr\('([^']+)'\)/g;
let m;
while ((m = trRegex.exec(appJs)) !== null) {
  if (!trKeys.includes(m[1])) trKeys.push(m[1]);
}

// Dynamic tr calls or template strings
const dynamicTrRegex = /tr\(([^)]+)\)/g;
const dynamicTr = [];
while ((m = dynamicTrRegex.exec(appJs)) !== null) {
  if (!m[1].startsWith("'")) dynamicTr.push(m[1]);
}

// Extract data-i18n and data-i18n-ph in index.html
const htmlI18nKeys = [];
const htmlRegex1 = /data-i18n="([^"]+)"/g;
while ((m = htmlRegex1.exec(indexHtml)) !== null) {
  if (!htmlI18nKeys.includes(m[1])) htmlI18nKeys.push(m[1]);
}
const htmlRegex2 = /data-i18n-ph="([^"]+)"/g;
while ((m = htmlRegex2.exec(indexHtml)) !== null) {
  if (!htmlI18nKeys.includes(m[1])) htmlI18nKeys.push(m[1]);
}

console.log('=== STATS ===');
console.log('jsViKeys count:', jsViKeys.length);
console.log('jsEnKeys count:', jsEnKeys.length);
console.log('jsonViKeys count:', jsonViKeys.length);
console.log('jsonEnKeys count:', jsonEnKeys.length);
console.log('trKeys in app.js count:', trKeys.length);
console.log('dynamicTr in app.js:', dynamicTr);
console.log('htmlI18nKeys in index.html count:', htmlI18nKeys.length);

console.log('\n=== Keys in jsonVi vs jsonEn ===');
console.log('in jsonVi not jsonEn:', jsonViKeys.filter(k => !jsonEnKeys.includes(k)));
console.log('in jsonEn not jsonVi:', jsonEnKeys.filter(k => !jsonViKeys.includes(k)));

console.log('\n=== Keys in jsVi vs jsEn ===');
console.log('in jsVi not jsEn:', jsViKeys.filter(k => !jsEnKeys.includes(k)));
console.log('in jsEn not jsVi:', jsEnKeys.filter(k => !jsViKeys.includes(k)));

console.log('\n=== Keys in HTML but missing in jsVi ===');
console.log(htmlI18nKeys.filter(k => !jsViKeys.includes(k)));

console.log('\n=== Keys in HTML but missing in strings_vi.json ===');
console.log(htmlI18nKeys.filter(k => !jsonViKeys.includes(k)));

console.log('\n=== Keys in HTML but missing in strings_en.json ===');
console.log(htmlI18nKeys.filter(k => !jsonEnKeys.includes(k)));

console.log('\n=== Keys used in tr() but missing in jsVi ===');
console.log(trKeys.filter(k => !jsViKeys.includes(k)));

console.log('\n=== Keys used in tr() but missing in strings_vi.json ===');
console.log(trKeys.filter(k => !jsonViKeys.includes(k)));

console.log('\n=== Keys in jsVi but missing in strings_vi.json ===');
console.log(jsViKeys.filter(k => !jsonViKeys.includes(k)));

console.log('\n=== Keys in strings_vi.json but missing in jsVi ===');
console.log(jsonViKeys.filter(k => !jsViKeys.includes(k)));
