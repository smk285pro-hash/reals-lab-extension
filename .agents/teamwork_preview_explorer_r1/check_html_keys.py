import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
html_path = os.path.join(ROOT, "ui-web", "index.html")
with open(html_path, 'r', encoding='utf-8') as f:
    html_content = f.read()

app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_content = f.read()

vi_match = re.search(r'vi:\s*\{([^}]+(?:\{[^}]+\}[^}]*)*)\}', js_content, re.DOTALL)
js_vi_keys = set(re.findall(r'[\'"]([a-zA-Z0-9_.]+)[\'"]\s*:', vi_match.group(1))) if vi_match else set()

html_data_i18n = set(re.findall(r'data-i18n=["\']([^"\']+)["\']', html_content))
html_data_i18n_ph = set(re.findall(r'data-i18n-ph=["\']([^"\']+)["\']', html_content))

all_html_keys = html_data_i18n | html_data_i18n_ph

print(f"Total HTML i18n keys: {len(all_html_keys)}")
missing_in_js_dict = all_html_keys - js_vi_keys
print(f"HTML keys missing in app.js I18N dictionary ({len(missing_in_js_dict)}):")
for k in sorted(list(missing_in_js_dict)):
    print(f"  - {k}")
