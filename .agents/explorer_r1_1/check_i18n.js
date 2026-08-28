const fs = require('fs');
const path = require('path');

const en = JSON.parse(fs.readFileSync('assets/i18n/strings_en.json', 'utf8'));
const vi = JSON.parse(fs.readFileSync('assets/i18n/strings_vi.json', 'utf8'));
const enKeys = Object.keys(en).sort();
const viKeys = Object.keys(vi).sort();

console.log('=== 1. PARITY BETWEEN EN AND VI ===');
console.log('Total EN keys:', enKeys.length);
console.log('Total VI keys:', viKeys.length);
const missingInVi = enKeys.filter(k => !(k in vi));
const missingInEn = viKeys.filter(k => !(k in en));
console.log('Missing in VI:', missingInVi);
console.log('Missing in EN:', missingInEn);

console.log('\n=== 2. USAGE IN UI-WEB ===');
const appJs = fs.readFileSync('ui-web/app.js', 'utf8');
const indexHtml = fs.readFileSync('ui-web/index.html', 'utf8');

const trMatches = [...appJs.matchAll(/tr\(\s*['"]([^'"]+)['"]\s*\)/g)].map(m => m[1]);
const dynamicTrMatches = [...appJs.matchAll(/tr\(\s*([a-zA-Z0-9_$.]+)\s*\)/g)].map(m => m[1]);
const dataI18nMatches = [...indexHtml.matchAll(/data-i18n=['"]([^'"]+)['"]/g)].map(m => m[1]);
const dataI18nTitleMatches = [...indexHtml.matchAll(/data-i18n-title=['"]([^'"]+)['"]/g)].map(m => m[1]);
const dataI18nPlaceholderMatches = [...indexHtml.matchAll(/data-i18n-placeholder=['"]([^'"]+)['"]/g)].map(m => m[1]);

const allDirectlyUsed = Array.from(new Set([...trMatches, ...dataI18nMatches, ...dataI18nTitleMatches, ...dataI18nPlaceholderMatches])).sort();
console.log('Total unique keys directly used in ui-web:', allDirectlyUsed.length);

const usedMissingInEn = allDirectlyUsed.filter(k => !(k in en));
console.log('Used keys missing in EN JSON:', usedMissingInEn);

const unusedInEn = enKeys.filter(k => !allDirectlyUsed.includes(k));
console.log('Keys in EN JSON not directly matched in ui-web:', unusedInEn);

console.log('\n=== 3. C++ I18N STRINGS (core/src/i18n/I18n.cpp) ===');
const i18nCpp = fs.readFileSync('core/src/i18n/I18n.cpp', 'utf8');
const cppEnMatches = [...i18nCpp.matchAll(/\{\s*"([^"]+)"\s*,\s*"([^"]*)"\s*\}/g)].map(m => m[1]);
console.log('Total C++ hardcoded fallback keys in I18n.cpp:', cppEnMatches.length);
const cppMissingInEn = cppEnMatches.filter(k => !(k in en));
console.log('C++ fallback keys missing in JSON:', cppMissingInEn);
const enMissingInCpp = enKeys.filter(k => !cppEnMatches.includes(k));
console.log('JSON keys missing in C++ fallback:', enMissingInCpp);
