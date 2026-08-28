import os
import re
import json
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
en_json_path = os.path.join(ROOT, "assets", "i18n", "strings_en.json")
with open(en_json_path, 'r', encoding='utf-8') as f:
    en_keys = json.load(f)

# Inspect index.html
html_path = os.path.join(ROOT, "ui-web", "index.html")
with open(html_path, 'r', encoding='utf-8') as f:
    html_content = f.read()

# Look for data-i18n attributes
data_i18n_keys = re.findall(r'data-i18n=["\']([^"\']+)["\']', html_content)
print(f"data-i18n keys in index.html: {len(data_i18n_keys)}")
print(f"data-i18n unique keys: {len(set(data_i18n_keys))}")

# Check missing data-i18n keys in en_keys
missing_html_keys = set(data_i18n_keys) - set(en_keys.keys())
print(f"data-i18n keys missing from strings_en.json: {missing_html_keys}")

# Find elements in index.html with inner text but WITHOUT data-i18n
# Let's inspect index.html lines
print("\n=== Elements in index.html with potential hardcoded text ===")
lines = html_content.splitlines()
for idx, line in enumerate(lines, 1):
    # check if line has English or Vietnamese text between tags that doesn't have data-i18n
    if '>' in line and '<' in line:
        # extract text between > and <
        matches = re.findall(r'>([^<]+)<', line)
        for m in matches:
            text = m.strip()
            # check if text looks like UI text and not just icons or whitespace
            if text and not text.startswith('&') and not text.isdigit() and len(text) > 1:
                # check if this tag or parent has data-i18n on the same line
                if 'data-i18n' not in line and 'data-i18n-placeholder' not in line and 'data-i18n-title' not in line:
                    # ignore svg, script, style, comments
                    if not any(k in line for k in ['<script', '<style', '<!--', '<svg', '<path']):
                        print(f"Line {idx:3d}: {line.strip()} (Extracted text: '{text}')")

# Inspect app.js
print("\n=== Checking app.js i18n implementation and hardcoded strings ===")
js_path = os.path.join(ROOT, "ui-web", "app.js")
with open(js_path, 'r', encoding='utf-8') as f:
    js_content = f.read()

js_lines = js_content.splitlines()
for idx, line in enumerate(js_lines, 1):
    # check t(...) usages
    t_matches = re.findall(r'\bt\(\s*["\']([^"\']+)["\']\s*\)', line)
    for tm in t_matches:
        if tm not in en_keys:
            print(f"Missing key in en: '{tm}' at app.js:{idx}")

# Also check how i18n dictionary is loaded in app.js
for idx, line in enumerate(js_lines[:150], 1):
    if 'i18n' in line.lower() or 'lang' in line.lower() or 'locale' in line.lower() or 'dict' in line.lower():
        print(f"app.js:{idx}: {line.strip()}")
