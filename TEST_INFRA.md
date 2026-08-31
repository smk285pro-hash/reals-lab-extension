# E2E Test Infra: Reals Lab REAPER Extension

## Test Philosophy
- Opaque-box, requirement-driven, derived strictly from `ORIGINAL_REQUEST.md`.
- Systematic 4-tier methodology: Category-Partition, Boundary Value Analysis (BVA), Pairwise Combinatorial, and Real-World Workload Testing, plus Tier 5 Adversarial Coverage Hardening.

## Feature Inventory
| # | Feature | Source (requirement) | Tier 1 | Tier 2 | Tier 3 | Tier 4 |
|---|---------|---------------------|:------:|:------:|:------:|:------:|
| 1 | R3: Clean Initial Default Roots | ORIGINAL_REQUEST §R3 | 5 | 5 | ✓ | ✓ |
| 2 | R1: Global Favorites View (`★`) | ORIGINAL_REQUEST §R1 | 5 | 5 | ✓ | ✓ |
| 3 | R2: Global Recursive Search & Filters | ORIGINAL_REQUEST §R2 | 5 | 5 | ✓ | ✓ |
| 4 | R4: 5,000+ Files Performance Benchmarks | ORIGINAL_REQUEST §R4 | 5 | 5 | ✓ | ✓ |

## Test Architecture
- **Test Runner**: Zero-dependency `TestRunner.h` compiled into `reals_tests.exe`.
- **Command**: `ctest --preset windows` or `.\build\windows\tests\Debug\reals_tests.exe`.
- **Pass/Fail Semantics**: 0 exit code indicates 100% assertions passed.

## Real-World Application Scenarios (Tier 4)
| # | Scenario | Features Exercised | Complexity |
|---|----------|--------------------|------------|
| 1 | Fresh Install Library Setup & Root Addition | Clean roots, Add folder, Initial tree scan | Medium |
| 2 | Multi-Root Cross-Library Global Search & Filter | Global search, /tag, /bpm, /key, view restore | High |
| 3 | Unified Global Favorites Audition & Transpose | Global favorites query, audio preview, pitch shift | High |
| 4 | 5,000+ Samples Zero-Lag Stress & Drag into REAPER | 5k directory walk, search <30ms, 60fps scroll, drag OLE | Critical |

## Coverage Thresholds
- Tier 1 (Feature Coverage): ≥5 tests per feature (≥20 tests)
- Tier 2 (Boundary & Corner Cases): ≥5 tests per feature (≥20 tests)
- Tier 3 (Cross-Feature Combinations): Pairwise coverage of major feature interactions (≥10 tests)
- Tier 4 (Real-World Workloads): Realistic multi-step workflows (≥5 tests)
- Tier 5 (Adversarial White-Box & Performance Benchmarks): Dedicated stress suite (`TestSuite_PerformanceBenchmark.cpp` + `TestSuite_Requirements_R1_R2_R3.cpp`).
