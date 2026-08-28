## 2026-08-26T14:47:56Z
You are the Sub-orchestrator for Milestone 3 (SQLite Database & Background Scanner Pool) for Reals Lab.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\sub_orch_m3_db_scanner\
Workspace root: c:\Users\smk28\Desktop\reals lab extension
Scope document: c:\Users\smk28\Desktop\reals lab extension\PROJECT.md

MANDATORY INSTRUCTIONS:
1. Read the following authoritative documents first:
   - c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
   - c:\Users\smk28\Desktop\reals lab extension\PROJECT.md
   - c:\Users\smk28\Desktop\reals lab extension\SPEC.md
   - c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
2. Mandatory rule: Use GitNexus tools (query, context, impact before editing, detect_changes before committing) in every step.
3. Your scope is Milestone 3 (Features 11-13 in PROJECT.md):
   - SQLite3 library integration in `libs/sqlite3/` and `core/db/`.
   - Implement `reals::db::Database` (`Database.h/.cpp`) managing `%APPDATA%/RealsLab/library.db` with schema: `samples`, `analysis` (including 512-dim float embedding BLOB), `user_tags`, and indexing.
   - Implement fast file hash checksumming (`reals::util::Hash` using xxHash64 / SHA256) to detect file changes and skip unchanged samples.
   - Implement `reals::scanner::BackgroundScanner` (`BackgroundScanner.h/.cpp`): multi-threaded asynchronous worker pool recursively scanning sample roots, computing hashes, updating DB, and emitting progress events (`scanner.progress`).
   - Build cleanly with zero warnings under C++20 (`cmake --preset windows` / `cmake --build --preset windows`).
   - Write comprehensive unit tests for DB operations, hash checksumming, and multi-threaded scanner concurrency.
4. Execute the iteration loop (Explorers -> Worker -> Reviewers -> Challengers -> Forensic Auditor -> Gate).
   - Include mandatory anti-cheating warning in worker dispatch.
5. Maintain `progress.md`, `BRIEFING.md`, `SCOPE.md`, and `GATE_STATUS.md` in your working directory.
6. When complete and gate passes, write `handoff.md` and use `send_message` to report back to your parent.
