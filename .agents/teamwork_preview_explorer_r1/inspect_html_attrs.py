import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
html_path = os.path.join(ROOT, "ui-web", "index.html")
with open(html_path, 'r', encoding='utf-8') as f:
    html_content = f.read()

lines = html_content.splitlines()
print("=== TITLES AND PLACEHOLDERS IN INDEX.HTML ===")
for idx, line in enumerate(lines, 1):
    titles = re.findall(r'title=["\']([^"\']+)["\']', line)
    phs = re.findall(r'placeholder=["\']([^"\']+)["\']', line)
    if titles:
        print(f"Line {idx:3d} title: {titles} | data-i18n: {'data-i18n' in line} | data-i18n-title: {'data-i18n-title' in line}")
    if phs:
        print(f"Line {idx:3d} placeholder: {phs} | data-i18n-ph: {'data-i18n-ph' in line}")
