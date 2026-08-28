import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    content = f.read()

# Extract keys in I18N.vi and I18N.en in app.js
vi_match = re.search(r'vi:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', content, re.DOTALL)
en_match = re.search(r'en:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', content, re.DOTALL)

js_vi_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', vi_match.group(1))) if vi_match else set()
js_en_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', en_match.group(1))) if en_match else set()

print(f"Total keys in app.js I18N.vi: {len(js_vi_keys)}")
print(f"Total keys in app.js I18N.en: {len(js_en_keys)}")

# All tr('...') in app.js
all_tr_calls = set(re.findall(r'tr\(\s*[\'"]([a-zA-Z0-9_.]+)[\'"]\s*\)', content))
print(f"Total unique tr('...') calls in app.js: {len(all_tr_calls)}")

missing_in_js_vi = all_tr_calls - js_vi_keys
missing_in_js_en = all_tr_calls - js_en_keys

print(f"\ntr(...) keys in app.js MISSING from app.js I18N.vi ({len(missing_in_js_vi)}):")
for k in sorted(list(missing_in_js_vi)):
    print(f"  - {k}")

print(f"\ntr(...) keys in app.js MISSING from app.js I18N.en ({len(missing_in_js_en)}):")
for k in sorted(list(missing_in_js_en)):
    print(f"  - {k}")
