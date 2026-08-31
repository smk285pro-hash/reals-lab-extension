## 2026-09-01T01:19:15+07:00
You are Explorer 1 (Core C++ & Storage Specialist) for Reals Lab REAPER Extension.
Your working directory is `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_core\`.
You MUST read `c:\Users\smk28\Desktop\reals lab extension\ORIGINAL_REQUEST.md`, `AGENTS.md`, `PLAN.md`, `SPEC.md`, `DESIGN.md`.

Your mission is to explore and analyze the core C++ backend of the codebase, focusing on:
1. Current folder root management & default roots initialization: Where are roots stored, initialized, and loaded? Where are default OS folders (Music/Desktop/Downloads) added, and how can they be cleanly removed on fresh installs?
2. Current Favorites mechanism: How are favorites stored (SQLite/database/metadata/JSON)? How does the core handle favorites queries? Can it query ALL favorites across all roots and subfolders globally?
3. Current Search implementation: How does `browser.search` or `fs.list` work? How is recursive scanning implemented? How are `/tag`, `/bpm:range`, `/key:note` filters parsed and applied?
4. File indexing & performance: How are directory listings and file metadata cached or scanned? How can we ensure <30ms listing/search for 5,000+ files and thread safety across REAPER main thread, background worker threads, and IPC bridge?
