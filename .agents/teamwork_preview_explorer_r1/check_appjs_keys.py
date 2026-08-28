import os
import re
import json
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_content = f.read()

en_path = os.path.join(ROOT, "assets", "i18n", "strings_en.json")
with open(en_path, 'r', encoding='utf-8') as f:
    en_json = json.load(f)

vi_match = re.search(r'vi:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', js_content, re.DOTALL)
js_vi_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', vi_match.group(1))) if vi_match else set()

# Find all static tr('key') and dynamic key arrays like ['low', 'scanner.cpuMode.low']
array_pairs = re.findall(r'\[\s*[\'"][a-zA-Z0-9_]+[\'"]\s*,\s*[\'"]([a-zA-Z0-9_.]+)[\'"]\s*\]', js_content)
static_trs = re.findall(r'tr\(\s*[\'"]([a-zA-Z0-9_.]+)[\'"]\s*\)', js_content)

all_targeted_keys = set(array_pairs) | set(static_trs)

print("=== KEYS USED IN APP.JS VS APP.JS I18N DICTIONARY ===")
for k in sorted(all_targeted_keys):
    in_js_dict = k in js_vi_keys
    in_json_file = k in en_json
    if not in_js_dict or not in_json_file:
        print(f"Key: '{k:30s}' | in app.js I18N: {str(in_js_dict):5s} | in assets JSON: {str(in_json_file):5s}")
