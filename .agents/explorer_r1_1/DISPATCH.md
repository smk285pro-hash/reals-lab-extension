## 2026-08-28T18:56:47Z

You are Explorer R1: Architecture & Layer Boundary Auditor for reals-lab-extension.

Your working directory: c:/Users/smk28/Desktop/reals lab extension/.agents/explorer_r1_1
Read ORIGINAL_REQUEST.md at c:/Users/smk28/Desktop/reals lab extension/.agents/ORIGINAL_REQUEST.md first.
Also read AGENTS.md, SPEC.md, PLAN.md, DESIGN.md.

IMPORTANT CONSTRAINTS:
1. Direct inspection using file and search tools (grep_search, find_by_name, view_file, list_dir). DO NOT use GitNexus tools.
2. DO NOT modify any source code. Read only.

YOUR MISSION:
Perform an exhaustive architecture, layer boundary, localization, and file metric audit across the entire codebase (core/, bridge/, shell/, app/, extension/, ui-web/, assets/i18n/):

1. Layer Isolation & Include Compliance:
   - Check every single #include directive in core/ (both core/include/ and core/src/): verify ZERO inclusions of ImGui, GLFW, reaper_plugin, windows.h/win32 APIs in non-platform files, or UI headers.
   - Check every #include in ui/ / ui-web/ / shell/: verify UI does not include GLFW or reaper_plugin directly (only shell/win for win32/WebView2, extension/ for reaper_plugin).
   - Check app/ and extension/: verify they are thin shells that delegate all logic to bridge / core.

2. UI Text Localization Audit:
   - Cross-examine ui-web/ (index.html, app.js, styles.css) and C++ UI strings.
   - Check whether any display text is hardcoded instead of using tr("key") or i18n lookup.
   - Inspect assets/i18n/strings_en.json and assets/i18n/strings_vi.json:
     * Check if all keys exist in both English and Vietnamese.
     * Identify missing keys, unreferenced keys, or discrepancies between keys in code and JSON.

3. File Size Metrics Audit:
   - Calculate line counts for ALL .h, .cpp, .js, .html, .css, .json files.
   - Flag any source/header file exceeding ~400 lines (AGENTS.md rule: "1 class trách nhiệm rõ ràng; file <= ~400 dòng thì tách").

4. Output Deliverables:
   - Write your comprehensive findings to c:/Users/smk28/Desktop/reals lab extension/.agents/explorer_r1_1/r1_report.md
   - Write your handoff summary to c:/Users/smk28/Desktop/reals lab extension/.agents/explorer_r1_1/handoff.md with all issues categorized by Severity (Critical, Major, Minor, Style/Lint), with exact File & Line Reference, Rule/Contract Violated, and Concrete Remediation.
   - When complete, send a message back to the orchestrator.
