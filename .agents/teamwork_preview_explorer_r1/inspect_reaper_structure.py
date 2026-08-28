import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
reaper_cpp = os.path.join(ROOT, "extension", "src", "reaper_plugin.cpp")
with open(reaper_cpp, 'r', encoding='utf-8') as f:
    lines = f.readlines()

print(f"Total lines in reaper_plugin.cpp: {len(lines)}")

print("\n=== Top-level functions / classes in reaper_plugin.cpp ===")
for idx, line in enumerate(lines, 1):
    # match function definitions or struct/class definitions
    if re.match(r'^(?:static\s+)?[a-zA-Z0-9_:*&<>]+\s+[a-zA-Z0-9_]+\s*\([^)]*\)\s*\{', line):
        print(f"Line {idx:4d}: {line.strip()}")
    elif line.strip().startswith('// ----') or line.strip().startswith('// ===='):
        print(f"Line {idx:4d}: {line.strip()}")
