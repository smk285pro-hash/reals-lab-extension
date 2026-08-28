## 2026-08-26T16:00:18Z
You are the Replacement Sub-orchestrator for Milestone 6 (Final E2E Verification & Adversarial Hardening) for Reals Lab.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\sub_orch_m6_final_2\
Workspace root: c:\Users\smk28\Desktop\reals lab extension
Scope document: c:\Users\smk28\Desktop\reals lab extension\PROJECT.md

MANDATORY INSTRUCTIONS:
1. Read the following authoritative documents first:
   - c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
   - c:\Users\smk28\Desktop\reals lab extension\PROJECT.md
   - c:\Users\smk28\Desktop\reals lab extension\TEST_INFRA.md
   - c:\Users\smk28\Desktop\reals lab extension\SPEC.md
   - c:\Users\smk28\Desktop\reals lab extension\DESIGN.md
   - c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
2. Mandatory rule: Use GitNexus tools (query, context, impact before editing, detect_changes before committing) in every step.
3. Your mission:
   - Build the entire project cleanly with zero warnings under C++20 (`cmake --preset windows` and `cmake --build --preset windows`).
   - Run the complete test suite:
     `.\build\windows\tests\Debug\reals_tests.exe`
     and verify all 8 suites pass 100%:
     * `TestSuite_AIInference` (Essentia TempoCNN, EDMA Key Voting, Discogs-MAEST, Mood-Jamendo, CLAP)
     * `TestSuite_AudioDSP` (SoundTouch Time-Stretch, DAW BPM Sync, Real-Time Pitch Shifter, Original Key)
     * `TestSuite_DatabaseScanner` (SQLite library schema, xxHash64 checksum cache, multi-threaded scanner pool)
     * `TestSuite_SearchEngine` (Syntax `/` parser, SQL query generation, SIMD AVX2/SSE2 cosine similarity)
     * `TestSuite_BridgeUI` (JSON-RPC contracts, `#playerTagBar`, `#btnSyncBpm`, `#pianoTransposerPop`, i18n dictionaries)
     * `TestSuite_BoundariesCorners` (Edge cases: silent audio, 0-byte files, corrupted headers, extreme BPMs, ±12 semitones, empty tags)
     * `TestSuite_CrossFeatures` (Pairwise module interactions)
     * `TestSuite_EndToEndWorkflows` (Realistic DAW workflows)
   - Perform Tier 5 Adversarial Coverage Hardening (stress testing concurrency, rapid piano key clicks, extreme pitch shifts, background scanning while playing audio).
   - If any test has a minor defect or failure, fix it cleanly in the corresponding source file and re-verify.
   - Author and write `TEST_READY.md` at the project root (`c:\Users\smk28\Desktop\reals lab extension\TEST_READY.md`).
   - Complete `handoff.md` in your working directory and notify your parent via `send_message`.
