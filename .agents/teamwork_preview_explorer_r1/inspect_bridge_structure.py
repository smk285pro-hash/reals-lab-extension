import os
import re
import sys

sys.stdout.reconfigure(encoding='utf-8')

ROOT = r"c:\Users\smk28\Desktop\reals lab extension"
bridge_cpp = os.path.join(ROOT, "bridge", "src", "Bridge.cpp")
with open(bridge_cpp, 'r', encoding='utf-8') as f:
    lines = f.readlines()

print(f"Total lines in Bridge.cpp: {len(lines)}")

print("\n=== Top-level classes / structs / functions in Bridge.cpp ===")
for idx, line in enumerate(lines, 1):
    if re.match(r'^(?:static\s+)?[a-zA-Z0-9_:*&<>]+\s+[a-zA-Z0-9_:]+\s*\([^)]*\)\s*\{', line):
        print(f"Line {idx:4d}: {line.strip()}")
    elif line.strip().startswith('// ----') or line.strip().startswith('// ===='):
        print(f"Line {idx:4d}: {line.strip()}")
    elif 'handle(' in line:
        print(f"Line {idx:4d}: {line.strip()}")
