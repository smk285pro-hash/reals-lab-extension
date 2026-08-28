import os
import re
import json
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_content = f.read()

# Find all dynamic tr(...) patterns like tr('prefix.' + var)
dynamic_trs = re.findall(r'tr\(\s*[\'"]([a-zA-Z0-9_.]+)[\'"]\s*\+\s*([^)]+)\)', js_content)
print("=== DYNAMIC TR PATTERNS IN APP.JS ===")
for prefix, var in dynamic_trs:
    print(f"  Prefix: '{prefix}' + {var}")

# Also check template literal tr calls e.g. tr(`prefix.${var}`)
template_trs = re.findall(r'tr\(\s*`([^`]+)`\s*\)', js_content)
print("=== TEMPLATE TR CALLS IN APP.JS ===")
for t in template_trs:
    print(f"  Template: `{t}`")
