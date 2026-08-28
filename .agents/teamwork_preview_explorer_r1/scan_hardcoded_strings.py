import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"

# Scan bridge/src/Bridge.cpp for hardcoded error messages, toast strings, labels
bridge_cpp = os.path.join(ROOT, "bridge", "src", "Bridge.cpp")
with open(bridge_cpp, 'r', encoding='utf-8') as f:
    bridge_lines = f.readlines()

print("=== BRIDGE HARDCODED STRINGS & ERROR MESSAGES ===")
for idx, line in enumerate(bridge_lines, 1):
    # Check for toast push, error messages, etc.
    if 'toast' in line.lower() or 'error' in line.lower() or 'pushEvent' in line or 'emitEvent' in line or 'sendToast' in line:
        # Check string literals
        strings = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', line)
        for s in strings:
            if len(s) > 3 and not s.startswith('#') and not s.startswith('SELECT') and not s.startswith('UPDATE') and not s.startswith('INSERT'):
                # Check if it's user facing text
                if any(c.isalpha() and c.islower() for c in s) and ' ' in s:
                    print(f"Bridge.cpp:{idx}: \"{s}\"")

# Scan extension/src/reaper_plugin.cpp
reaper_cpp = os.path.join(ROOT, "extension", "src", "reaper_plugin.cpp")
with open(reaper_cpp, 'r', encoding='utf-8') as f:
    reaper_lines = f.readlines()

print("\n=== REAPER_PLUGIN.CPP HARDCODED STRINGS ===")
for idx, line in enumerate(reaper_lines, 1):
    strings = re.findall(r'"([^"\\]*(?:\\.[^"\\]*)*)"', line)
    for s in strings:
        if len(s) > 3 and ' ' in s and not s.startswith('#'):
            print(f"reaper_plugin.cpp:{idx}: \"{s}\"")

# Scan ui-web/app.js for hardcoded strings in alert(), toast(), showToast(), innerHTML, textContent, etc.
app_js = os.path.join(ROOT, "ui-web", "app.js")
with open(app_js, 'r', encoding='utf-8') as f:
    js_lines = f.readlines()

print("\n=== APP.JS HARDCODED USER-FACING STRINGS ===")
for idx, line in enumerate(js_lines, 1):
    # Look for showToast, alert, confirm, innerHTML, or string concatenation
    if any(k in line for k in ['showToast', 'toast', 'alert(', 'confirm(', 'innerHTML', 'textContent', 'placeholder', 'title']):
        # Find literals that don't use tr(...) or t(...)
        literals = re.findall(r'["\'`]([^"\'`]+)["\'`]', line)
        for lit in literals:
            if len(lit) > 3 and ' ' in lit and not lit.startswith('<') and not lit.startswith('data:') and not lit.startswith('http'):
                # check if tr(lit) was used or if lit is a key
                if not (f"tr('{lit}')" in line or f'tr("{lit}")' in line or f"t('{lit}')" in line or f't("{lit}")' in line):
                    # Check if lit contains English or Vietnamese words
                    print(f"app.js:{idx}: '{lit}'")
