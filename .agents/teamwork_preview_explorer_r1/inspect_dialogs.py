import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_lines = f.readlines()

print("=== CHECKING PROMPT / CONFIRM / CONTEXT MENUS IN APP.JS ===")
for idx, line in enumerate(js_lines, 1):
    if any(k in line for k in ['prompt(', 'confirm(', 'renderCtx', 'createContextMenu', 'showModal', 'openModal']):
        print(f"app.js:{idx}: {line.strip()}")
