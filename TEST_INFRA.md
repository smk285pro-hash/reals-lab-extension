# E2E Test Infra: Reals Lab Theme Engine

## Test Philosophy
- Opaque-box, requirement-driven. Derives from ORIGINAL_REQUEST.md.
- Methodology: Category-Partition + BVA + Pairwise + Workload Testing.

## Feature Inventory
| # | Feature | Source (requirement) | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|---|---------|---------------------|:------:|:------:|:------:|:------:|
| 1 | 82 Design Tokens & 3 Palettes | ORIGINAL_REQUEST §R1 | 5 | 5 | ✓ | ✓ |
| 2 | Bidirectional IPC & REAPER ExtState | ORIGINAL_REQUEST §R2 | 5 | 5 | ✓ | ✓ |
| 3 | Zero-FOUC & WebView2 Transparency | ORIGINAL_REQUEST §R2 | 5 | 5 | ✓ | ✓ |
| 4 | Dynamic Canvas Redraw & Theme Picker | ORIGINAL_REQUEST §R3 | 5 | 5 | ✓ | ✓ |
| 5 | Early DLL Deployment & Zero Warnings | ORIGINAL_REQUEST §R4 | 5 | 5 | ✓ | ✓ |

## Test Architecture
- Native Test Runner: `build/windows/tests/Debug/reals_tests.exe --suite=ThemeEngine` and `ctest --preset windows`
- Token Parity Validator: `python tests/verify_tokens_test.py`
- Test cases located in: `tests/suites/TestSuite_ThemeEngine.cpp`
- Directory layout: `tests/suites/`, `tests/`

## Coverage Thresholds
- Tier 1: ≥5 per feature (Feature isolation)
- Tier 2: ≥5 per feature (Boundaries & corner cases: invalid strings, null bytes, long strings, injection attacks)
- Tier 3: Pairwise combinations (Rapid switching, concurrency, ExtState reload)
- Tier 4: Real-world scenarios (Full lifecycle, REAPER start/stop/reload, live canvas sync)
