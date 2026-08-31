# Orchestration Plan: Reals Lab REAPER Extension Enhancements

## 1. Survey Phase
Spawn 3 Explorers / Spec Miners in parallel:
- **Explorer 1 (Core C++ & Storage)**: Examine `src/core/`, SQLite/database, file listing/scanning backend, favorites storage, search algorithms, indexing, and roots management.
- **Explorer 2 (UI & Frontend / IPC)**: Examine `src/ui/`, HTML/JS/CSS webview, IPC messages between C++ shell and WebView2 frontend, `#favOnly` toggle, `#search` bar, tree rendering, and virtual list scrolling.
- **Explorer 3 (Build, Tests & Performance)**: Examine CMake presets, existing test suite in `tests/`, benchmarking capabilities, build flags, and Windows build environment.

## 2. Synthesis & Project Decomposition
- Synthesize findings into `PROJECT.md` with Architecture, Feature Inventory, Milestones, and Interface Contracts.
- Define `TEST_INFRA.md` for E2E testing track.

## 3. Execution Loops
Execute milestones sequentially with strict gating:
- Milestone 1: Clean Initial Default Roots
- Milestone 2: Global Favorites View (`★`)
- Milestone 3: Global Recursive Search with Filters (`/tag`, `/bpm:range`, `/key:note`) & View Restore
- Milestone 4: Performance Optimization (<30ms listing/search for 5000+ files, 60fps virtualized rendering, thread safety)
- Final Milestone: E2E Test Suite Pass (Tiers 1-4) + Adversarial Hardening (Tier 5) + Forensic Audit.

## 4. Verification & Audit
- Continuous gate check: Build clean (0 warnings/errors), 100% tests pass, Reviewers APPROVE, Challengers confirm performance & correctness, Forensic Auditor CLEAN.
