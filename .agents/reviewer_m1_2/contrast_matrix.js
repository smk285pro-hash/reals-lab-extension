const fs = require('fs');
const path = require('path');

const tokensCss = fs.readFileSync(path.join(__dirname, '../../ui-web/tokens.css'), 'utf8');

// Parse themes
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

function hexToRgb(hex) {
  if (!hex) return null;
  hex = hex.trim().replace('#', '');
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

function parseColor(val, bgRgb = [13, 14, 17]) {
  if (!val) return null;
  val = val.trim();
  if (val.startsWith('#')) return hexToRgb(val);
  const rgbaMatch = val.match(/rgba?\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)(?:\s*,\s*([0-9.]+))?\s*\)/);
  if (rgbaMatch) {
    const r = parseFloat(rgbaMatch[1]);
    const g = parseFloat(rgbaMatch[2]);
    const b = parseFloat(rgbaMatch[3]);
    const a = rgbaMatch[4] !== undefined ? parseFloat(rgbaMatch[4]) : 1.0;
    // Composite over bgRgb
    return [
      Math.round(r * a + bgRgb[0] * (1 - a)),
      Math.round(g * a + bgRgb[1] * (1 - a)),
      Math.round(b * a + bgRgb[2] * (1 - a))
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

function contrast(rgb1, rgb2) {
  if (!rgb1 || !rgb2) return null;
  const l1 = sRgbLuminance(rgb1);
  const l2 = sRgbLuminance(rgb2);
  const brightest = Math.max(l1, l2);
  const darkest = Math.min(l1, l2);
  return (brightest + 0.05) / (darkest + 0.05);
}

console.log('=== COMPREHENSIVE WCAG CONTRAST MATRIX ===\n');

Object.entries(themeBlocks).forEach(([themeKey, vars]) => {
  const themeName = themeKey.replace(/\r?\n/g, ' ');
  console.log(`----------------------------------------`);
  console.log(`THEME: ${themeName}`);
  console.log(`----------------------------------------`);
  
  const bgAppRgb = parseColor(vars['--bg-app']) || [13, 14, 17];
  const bgCardRgb = parseColor(vars['--bg-card'], bgAppRgb);
  const bgSidebarRgb = parseColor(vars['--bg-sidebar'], bgAppRgb);
  const bgInputRgb = parseColor(vars['--bg-input'], bgAppRgb);
  const bgElevatedRgb = parseColor(vars['--bg-elevated'], bgAppRgb);

  // Check Text Contrast on various surfaces
  const textTokens = [
    '--text-primary',
    '--text-primary-strong',
    '--text-secondary',
    '--text-secondary-strong',
    '--text-chip',
    '--text-meta',
    '--text-tertiary',
    '--text-disabled'
  ];

  const surfaces = [
    { name: '--bg-app', rgb: bgAppRgb },
    { name: '--bg-card', rgb: bgCardRgb },
    { name: '--bg-sidebar', rgb: bgSidebarRgb },
    { name: '--bg-input', rgb: bgInputRgb },
    { name: '--bg-elevated', rgb: bgElevatedRgb }
  ];

  surfaces.forEach(s => {
    console.log(`\n  Surface: ${s.name} (${vars[s.name]})`);
    textTokens.forEach(t => {
      const textVal = vars[t];
      const textRgb = parseColor(textVal, s.rgb);
      const cr = contrast(textRgb, s.rgb);
      let status = 'PASS AA';
      if (t.includes('disabled') || t.includes('tertiary')) {
        // Tertiary/disabled text typically exempt or requires 3:1 for large/incidental
        status = cr >= 3.0 ? 'PASS (3:1)' : 'WARN (<3:1)';
      } else {
        status = cr >= 4.5 ? 'PASS AA (>=4.5:1)' : 'FAIL AA (<4.5:1)';
      }
      console.log(`    ${t.padEnd(24)} (${textVal.padEnd(10)}) -> ${cr ? cr.toFixed(2).padStart(5) : 'N/A'}:1 | ${status}`);
    });
  });

  // Check Functional Badges
  console.log(`\n  Functional Badges:`);
  const badges = [
    { name: 'FREE badge', bgToken: '--free-bg', txToken: '--free-tx' },
    { name: 'PRO badge',  bgToken: '--pro-bg',  txToken: '--pro-tx' },
    { name: 'UPD badge',  bgToken: '--upd-bg',  txToken: '--upd-tx' },
    { name: 'MIDI badge', bgToken: '--badge-midi-bg', txToken: '--badge-midi-tx' },
    { name: 'DANGER',     bgToken: '--danger-soft', txToken: '--danger' }
  ];

  badges.forEach(b => {
    const badgeBgRgb = parseColor(vars[b.bgToken], bgCardRgb);
    const badgeTxRgb = parseColor(vars[b.txToken], badgeBgRgb);
    const cr = contrast(badgeTxRgb, badgeBgRgb);
    const status = cr >= 4.5 ? 'PASS AA' : (cr >= 3.0 ? 'PASS (3:1 Large/Badge)' : 'FAIL');
    console.log(`    ${b.name.padEnd(16)}: ${vars[b.txToken]} on ${vars[b.bgToken]} -> ${cr ? cr.toFixed(2) : 'N/A'}:1 | ${status}`);
  });

  // Check Button & Accent Contrast
  console.log(`\n  Accent & Button Contrast:`);
  const accentRgb = parseColor(vars['--accent'], bgAppRgb);
  const accentContrastRgb = parseColor(vars['--accent-contrast'], accentRgb);
  const btnCr = contrast(accentContrastRgb, accentRgb);
  console.log(`    --accent-contrast on --accent (${vars['--accent-contrast']} on ${vars['--accent']}): ${btnCr ? btnCr.toFixed(2) : 'N/A'}:1 | ${btnCr >= 4.5 ? 'PASS AA' : (btnCr >= 3.0 ? 'PASS (3:1)' : 'FAIL')}`);

  // Piano Roll keys
  console.log(`\n  Piano Roll Keys:`);
  const whiteKeyBg = parseColor(vars['--pianoroll-key-white-bg']);
  const whiteKeyTx = parseColor(vars['--pianoroll-key-white-tx'], whiteKeyBg);
  const whiteCr = contrast(whiteKeyTx, whiteKeyBg);
  console.log(`    White key text on bg (${vars['--pianoroll-key-white-tx']} on ${vars['--pianoroll-key-white-bg']}): ${whiteCr ? whiteCr.toFixed(2) : 'N/A'}:1`);

  const blackKeyBg = parseColor(vars['--pianoroll-key-black-bg']);
  const blackKeyTx = parseColor(vars['--pianoroll-key-black-tx'], blackKeyBg);
  const blackCr = contrast(blackKeyTx, blackKeyBg);
  console.log(`    Black key text on bg (${vars['--pianoroll-key-black-tx']} on ${vars['--pianoroll-key-black-bg']}): ${blackCr ? blackCr.toFixed(2) : 'N/A'}:1`);
});
