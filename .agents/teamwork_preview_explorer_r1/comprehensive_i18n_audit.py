import os
import re
import json
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"

# 1. Load JSON files
en_path = os.path.join(ROOT, "assets", "i18n", "strings_en.json")
vi_path = os.path.join(ROOT, "assets", "i18n", "strings_vi.json")

with open(en_path, 'r', encoding='utf-8') as f:
    en_json = json.load(f)

with open(vi_path, 'r', encoding='utf-8') as f:
    vi_json = json.load(f)

# 2. Load C++ embedded table from core/src/i18n/I18n.cpp
i18n_cpp = os.path.join(ROOT, "core", "src", "i18n", "I18n.cpp")
cpp_embedded_keys = set()
with open(i18n_cpp, 'r', encoding='utf-8') as f:
    cpp_content = f.read()
    # Match {"app.title", "REALS LAB", "REALS LAB"},
    matches = re.findall(r'\{\s*"([^"]+)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}', cpp_content)
    for m in matches:
        cpp_embedded_keys.add(m[0])

# 3. Load app.js I18N table
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_content = f.read()

vi_match = re.search(r'vi:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', js_content, re.DOTALL)
en_match = re.search(r'en:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', js_content, re.DOTALL)

js_vi_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', vi_match.group(1))) if vi_match else set()
js_en_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', en_match.group(1))) if en_match else set()

# 4. Extract data-i18n from index.html
html_path = os.path.join(ROOT, "ui-web", "index.html")
with open(html_path, 'r', encoding='utf-8') as f:
    html_content = f.read()

html_data_i18n = set(re.findall(r'data-i18n=["\']([^"\']+)["\']', html_content))
html_data_i18n_ph = set(re.findall(r'data-i18n-ph=["\']([^"\']+)["\']', html_content))
html_data_i18n_title = set(re.findall(r'data-i18n-title=["\']([^"\']+)["\']', html_content))

# 5. Extract all tr(...) / t(...) usages in all C++ & JS code
tr_regex = re.compile(r'\b(?:tr|t|i18n::tr|reals::i18n::tr)\s*\(\s*["\']([^"\']+)["\']')
all_code_keys = set()
code_key_locations = {}

for root, dirs, files in os.walk(ROOT):
    if any(exc in root for exc in ['build', '.git', '.agents', '.claude', '.gitnexus', 'libs']):
        continue
    for file in files:
        if file.endswith(('.cpp', '.h', '.js', '.html')):
            fpath = os.path.join(root, file)
            rel = os.path.relpath(fpath, ROOT).replace('\\', '/')
            with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
                for idx, line in enumerate(f, 1):
                    for k in tr_regex.findall(line):
                        all_code_keys.add(k)
                        if k not in code_key_locations:
                            code_key_locations[k] = []
                        code_key_locations[k].append(f"{rel}:{idx}")

print("================================================================")
print("             COMPREHENSIVE I18N AUDIT REPORT                    ")
print("================================================================")
print(f"Total keys in strings_en.json:          {len(en_json)}")
print(f"Total keys in strings_vi.json:          {len(vi_json)}")
print(f"Total keys in C++ I18n.cpp embedded:    {len(cpp_embedded_keys)}")
print(f"Total keys in ui-web/app.js I18N (vi):  {len(js_vi_keys)}")
print(f"Total keys in ui-web/app.js I18N (en):  {len(js_en_keys)}")
print(f"Total unique keys in index.html (i18n): {len(html_data_i18n | html_data_i18n_ph | html_data_i18n_title)}")
print(f"Total unique keys referenced in code:   {len(all_code_keys)}")

print("\n--- 1. Keys mismatch between strings_en.json and strings_vi.json ---")
en_diff_vi = set(en_json.keys()) ^ set(vi_json.keys())
print(f"Symmetric difference: {len(en_diff_vi)} keys")

print("\n--- 2. Keys referenced in Code/HTML but MISSING in strings_en.json / strings_vi.json ---")
all_active_keys = all_code_keys | html_data_i18n | html_data_i18n_ph | html_data_i18n_title
missing_in_assets = all_active_keys - set(en_json.keys())
for k in sorted(missing_in_assets):
    print(f"  [MISSING IN JSON] '{k}' -> used at: {code_key_locations.get(k, 'HTML attribute')}")

print("\n--- 3. Keys referenced in app.js tr(...) but MISSING in app.js I18N dictionary ---")
missing_in_js_dict = all_code_keys - js_vi_keys
for k in sorted(missing_in_js_dict):
    if any('app.js' in loc for loc in code_key_locations.get(k, [])):
        print(f"  [MISSING IN JS I18N] '{k}' -> used at: {code_key_locations.get(k, [])}")

print("\n--- 4. Keys in strings_*.json MISSING in C++ I18n.cpp embedded table ---")
missing_in_cpp_embedded = set(en_json.keys()) - cpp_embedded_keys
print(f"Count: {len(missing_in_cpp_embedded)} keys missing from C++ embedded fallback")

print("\n--- 5. Dead / Unused keys in strings_*.json (defined in JSON but not found in any Code or HTML) ---")
unused_in_json = set(en_json.keys()) - all_active_keys
print(f"Count: {len(unused_in_json)} keys defined in JSON but never referenced in code/HTML")
for k in sorted(unused_in_json):
    print(f"  [UNUSED KEY] '{k}'")
