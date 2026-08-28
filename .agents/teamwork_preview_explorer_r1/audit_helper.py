import os
import re
import json

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
EXCLUDES = ['build', '.git', '.agents', '.claude', '.gitnexus', 'libs']

def is_excluded(path):
    rel = os.path.relpath(path, ROOT)
    parts = rel.split(os.sep)
    for exc in EXCLUDES:
        if exc in parts:
            return True
    return False

# 1. Line count check
print("=== FILE SIZE AUDIT (>350 lines) ===")
large_files = []
all_files = []
for dirpath, dirnames, filenames in os.walk(ROOT):
    if is_excluded(dirpath):
        continue
    for f in filenames:
        fp = os.path.join(dirpath, f)
        if is_excluded(fp):
            continue
        all_files.append(fp)
        try:
            with open(fp, 'r', encoding='utf-8', errors='ignore') as file_obj:
                lines = len(file_obj.readlines())
                if lines > 350:
                    rel = os.path.relpath(fp, ROOT).replace('\\', '/')
                    large_files.append((lines, rel))
        except Exception as e:
            pass

large_files.sort(key=lambda x: x[0], reverse=True)
for lines, path in large_files:
    print(f"{lines:5d} lines | {path}")

# 2. Check all #includes in core/
print("\n=== CORE INCLUDE AUDIT ===")
core_forbidden = ['imgui', 'glfw', 'reaper', 'webview', 'shell', 'bridge', 'windows.h']
core_files = [f for f in all_files if '\\core\\' in f or '/core/' in f]
for cf in core_files:
    rel = os.path.relpath(cf, ROOT).replace('\\', '/')
    with open(cf, 'r', encoding='utf-8', errors='ignore') as f:
        for idx, line in enumerate(f, 1):
            line_str = line.strip()
            if line_str.startswith('#include'):
                for forb in core_forbidden:
                    if forb in line_str.lower():
                        print(f"[CORE FORBIDDEN] {rel}:{idx}: {line_str}")

# 3. Check all #includes in bridge/
print("\n=== BRIDGE INCLUDE AUDIT ===")
bridge_files = [f for f in all_files if '\\bridge\\' in f or '/bridge/' in f]
for bf in bridge_files:
    rel = os.path.relpath(bf, ROOT).replace('\\', '/')
    with open(bf, 'r', encoding='utf-8', errors='ignore') as f:
        for idx, line in enumerate(f, 1):
            line_str = line.strip()
            if line_str.startswith('#include'):
                if 'reaper' in line_str.lower() or 'glfw' in line_str.lower() or 'webview' in line_str.lower():
                    print(f"[BRIDGE FORBIDDEN] {rel}:{idx}: {line_str}")

# 4. Check i18n files
print("\n=== I18N STRINGS AUDIT ===")
en_json_path = os.path.join(ROOT, "assets", "i18n", "strings_en.json")
vi_json_path = os.path.join(ROOT, "assets", "i18n", "strings_vi.json")

with open(en_json_path, 'r', encoding='utf-8') as f:
    en_data = json.load(f)

with open(vi_json_path, 'r', encoding='utf-8') as f:
    vi_data = json.load(f)

en_keys = set(en_data.keys())
vi_keys = set(vi_data.keys())

print(f"EN keys total: {len(en_keys)}")
print(f"VI keys total: {len(vi_keys)}")

in_en_not_vi = en_keys - vi_keys
in_vi_not_en = vi_keys - en_keys

print(f"In EN but NOT VI ({len(in_en_not_vi)}): {sorted(list(in_en_not_vi))}")
print(f"In VI but NOT EN ({len(in_vi_not_en)}): {sorted(list(in_vi_not_en))}")

# Scan for tr(...) or t(...) usages in ui-web/ and C++
print("\n=== UI / CODE I18N USAGE SCAN ===")
tr_regex = re.compile(r'\b(?:tr|t|i18n\.t)\s*\(\s*["\']([^"\']+)["\']')
used_keys = set()
used_key_locations = {}

for fpath in all_files:
    rel = os.path.relpath(fpath, ROOT).replace('\\', '/')
    if not (rel.startswith('ui-web/') or rel.startswith('core/') or rel.startswith('bridge/') or rel.startswith('shell/') or rel.startswith('extension/')):
        continue
    with open(fpath, 'r', encoding='utf-8', errors='ignore') as f:
        for idx, line in enumerate(f, 1):
            matches = tr_regex.findall(line)
            for k in matches:
                used_keys.add(k)
                if k not in used_key_locations:
                    used_key_locations[k] = []
                used_key_locations[k].append(f"{rel}:{idx}")

print(f"Unique keys found in code/UI: {len(used_keys)}")
missing_in_en = used_keys - en_keys
missing_in_vi = used_keys - vi_keys
unused_en = en_keys - used_keys
unused_vi = vi_keys - used_keys

print(f"Used in code/UI but MISSING in EN ({len(missing_in_en)}): {sorted(list(missing_in_en))}")
print(f"Used in code/UI but MISSING in VI ({len(missing_in_vi)}): {sorted(list(missing_in_vi))}")
print(f"Defined in EN but UNUSED in code/UI ({len(unused_en)}): {sorted(list(unused_en))}")
print(f"Defined in VI but UNUSED in code/UI ({len(unused_vi)}): {sorted(list(unused_vi))}")
