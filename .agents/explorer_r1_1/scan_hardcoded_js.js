const fs = require('fs');

const appJs = fs.readFileSync('ui-web/app.js', 'utf8');
const lines = appJs.split('\n');

// Find string literals that might be user-facing hardcoded text (e.g. Vietnamese characters or display messages)
const vietnameseRegex = /[àáạảãâầấậẩẫăằắặẳẵèéẹẻẽêềếệểễìíịỉĩòóọỏõôồốộổỗơờớợởỡùúụủũưừứựửữỳýỵỷỹđÀÁẠẢÃÂẦẤẬẨẪĂẰẮẶẲẴÈÉẸẺẼÊỀẾỆỂỄÌÍỊỈĨÒÓỌỎÕÔỒỐỘỔỖƠỜỚỢỞỠÙÚỤỦŨƯỪỨỰỬỮỲÝỴỶỸĐ]/;

console.log('=== VIETNAMESE HARDCODED STRINGS IN APP.JS ===');
lines.forEach((line, idx) => {
  // skip comments
  const trimmed = line.trim();
  if (trimmed.startsWith('//') || trimmed.startsWith('/*')) return;
  if (vietnameseRegex.test(line)) {
    console.log(`Line ${idx + 1}: ${trimmed}`);
  }
});
