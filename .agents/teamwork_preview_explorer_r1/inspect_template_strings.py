import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_content = f.read()

lines = js_content.splitlines()

# Search for innerHTML or template strings containing Vietnamese or English words not wrapped in tr()
print("=== TEMPLATE STRINGS / INNERHTML IN APP.JS ===")
for idx, line in enumerate(lines, 1):
    if '`' in line and not line.strip().startswith('//'):
        # extract template literals
        templates = re.findall(r'`([^`]+)`', line)
        for t in templates:
            # Check if template has human readable text outside ${...}
            clean_text = re.sub(r'\$\{[^}]+\}', '', t).strip()
            if len(clean_text) > 3 and any(c.isalpha() for c in clean_text) and not clean_text.startswith('<svg') and not clean_text.startswith('http'):
                # Ignore pure css/style/id
                if not any(k in clean_text for k in ['translate', 'calc', 'px', 'rgba', 'var(--', 'border:', 'color:']):
                    print(f"app.js:{idx}: `{t}` (Text outside exprs: '{clean_text}')")
