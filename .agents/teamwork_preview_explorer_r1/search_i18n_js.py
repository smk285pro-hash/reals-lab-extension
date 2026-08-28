import os
import re
import json

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
js_path = os.path.join(ROOT, "ui-web", "app.js")
with open(js_path, 'r', encoding='utf-8') as f:
    js_content = f.read()

# Check how applyI18n works in app.js
lines = js_content.splitlines()
for idx, line in enumerate(lines, 1):
    if 'data-i18n' in line or 'applyI18n' in line or 'setLanguage' in line or 'updateI18n' in line:
        print(f"app.js:{idx}: {line}")
