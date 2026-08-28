import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_lines = f.readlines()

print("=== ALL TOAST CALLS IN APP.JS ===")
for idx, line in enumerate(js_lines, 1):
    if 'toast(' in line:
        # find content inside toast(...)
        matches = re.findall(r'toast\((.*?)\)', line)
        for m in matches:
            print(f"app.js:{idx}: toast({m.strip()})")

print("\n=== ALL DIRECT INNERHTML / TEXTCONTENT ASSIGNMENTS WITH STRING LITERALS ===")
for idx, line in enumerate(js_lines, 1):
    if ('.innerHTML =' in line or '.textContent =' in line or '.innerText =' in line):
        if not ('tr(' in line or 't(' in line or 'svg' in line or '""' in line or "''" in line):
            # check if line contains literal text
            literals = re.findall(r'["\'`]([^"\'`]+)["\'`]', line)
            user_texts = [lit for lit in literals if len(lit) > 2 and any(c.isalpha() for c in lit) and not lit.startswith('#') and not lit.startswith('.') and not lit.startswith('<') and not lit in ['div', 'span', 'hidden', 'active', 'selected', 'click', 'change', 'keydown', 'none', 'block', 'flex', 'input', 'button']]
            if user_texts:
                print(f"app.js:{idx}: {line.strip()} | text: {user_texts}")
