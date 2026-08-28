# Original User Request

## 2026-08-28T18:55:17Z

Perform a comprehensive multi-agent audit and code inspection of all files across the entire codebase to identify defects, rule violations, and edge-case risks without using GitNexus.

Working directory: c:/Users/smk28/Desktop/reals lab extension
Integrity mode: development

## Requirements

### R1. Architecture & Layer Boundary Audit
Scan and verify compliance with `AGENTS.md` and `SPEC.md`:
- `core/`: ensure zero inclusions of ImGui, GLFW, or `reaper_plugin`.
- `ui/`: ensure zero inclusions of GLFW or `reaper_plugin` (only shell interfaces).
- `app/` & `extension/`: ensure thin shell logic only, with business logic residing in `core/`.
- UI text localization: verify zero hardcoded display strings (all must route through `tr("key")` backed by `assets/i18n/`).
- File size limit: flag any source file exceeding ~400 lines without modular separation.

### R2. Code Quality, Memory & Concurrency Inspection
Inspect all C++ headers and implementation files for:
- C++20 compliance, naming conventions (PascalCase classes/methods, camelCase variables, `m_` members, `k` constants).
- Smart pointer usage and elimination of owning raw `new`/`delete`.
- Audio thread real-time safety: verify audio callback code contains no heap allocations, mutex locking, or blocking operations.
- Error handling, null checks, and boundary conditions.

### R3. Build & Test Diagnostics
- Verify build configuration (`CMakeLists.txt`, `CMakePresets.json`) and detect potential lock contention or environment-specific failures.
- Inspect test suites in `tests/` to identify missing test coverage or silent edge cases.

### R4. Synthesis & Structured Audit Report
Consolidate all findings into a structured markdown report `CODEBASE_AUDIT_REPORT.md` categorized by:
- Severity (Critical, Major, Minor, Style/Lint)
- File path & line reference
- Rule / contract violated
- Concrete remediation recommendation

## Acceptance Criteria

### Completeness of File Coverage
- [ ] Every `.h` and `.cpp` file in `core/`, `ui/`, `app/`, `extension/`, `bridge/`, and `tests/` is inspected.
- [ ] Localization keys in `assets/i18n/strings_en.json` and `assets/i18n/strings_vi.json` are cross-referenced with UI string usages.

### Layer & Rule Verification
- [ ] 100% check of `#include` directives against layer isolation constraints.
- [ ] Zero unflagged raw pointers with ownership semantics.
- [ ] Audio thread hot paths audited for non-realtime functions (malloc/free/mutex).

### Actionable Deliverable
- [ ] `CODEBASE_AUDIT_REPORT.md` generated with total error counts, categorizations, and concrete fix proposals.
