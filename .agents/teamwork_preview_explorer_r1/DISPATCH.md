## 2026-08-28T19:08:44Z
<USER_REQUEST>
You are the Architecture & Layer Boundary Auditor for reals-lab-extension.
Your working directory is: c:/Users/smk28/Desktop/reals lab extension/.agents/teamwork_preview_explorer_r1/
Scope document: c:/Users/smk28/Desktop/reals lab extension/.agents/ORIGINAL_REQUEST.md

Instructions:
1. Read c:/Users/smk28/Desktop/reals lab extension/.agents/ORIGINAL_REQUEST.md, AGENTS.md, SPEC.md.
2. IMPORTANT CONSTRAINT: Inspect the codebase directly using file and search tools (view_file, grep_search, find_by_name) without using GitNexus tools.
3. Conduct a comprehensive inspection of Architecture & Layer Boundaries (R1):
   - Check all #include directives in core/, ui/, app/, extension/, bridge/, shell/:
     * core/: verify ZERO inclusions of ImGui, GLFW, or reaper_plugin / reaper_plugin_functions.h.
     * ui/ & bridge/: verify zero inclusions of GLFW or reaper_plugin directly in ui/. Check bridge/ boundaries.
     * app/ & extension/: verify thin shell logic only, ensure business logic resides in core/ or bridge/. Check reaper_plugin.cpp and app/ for logic leaks.
   - UI text localization: verify zero hardcoded display strings in ui-web/ (HTML/JS) and C++ UI strings. Check all strings route through tr("key") backed by assets/i18n/strings_en.json and assets/i18n/strings_vi.json. Cross-reference keys between en and vi, identify missing/unused keys.
   - File size limits: flag every source/header file exceeding ~400 lines (e.g. Bridge.cpp, reaper_plugin.cpp, etc.) and propose modular separation.
4. Document all findings in your working directory at c:/Users/smk28/Desktop/reals lab extension/.agents/teamwork_preview_explorer_r1/handoff.md categorized by:
   - Severity (Critical, Major, Minor, Style/Lint)
   - File path & line reference
   - Rule/Contract violated
   - Concrete remediation recommendation
5. Update progress.md in your working directory and notify the orchestrator when done.
</USER_REQUEST>
