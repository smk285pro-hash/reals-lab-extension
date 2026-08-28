## 2026-08-26T14:41:32Z
You are Spec Miner 1 (AI Models & DSP Spec Miner) for Reals Lab.
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1\
Workspace root: c:\Users\smk28\Desktop\reals lab extension

MANDATORY INSTRUCTIONS:
1. Read the following authoritative documents first:
   - c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md
   - c:\Users\smk28\Desktop\reals lab extension\AGENTS.md
   - c:\Users\smk28\Desktop\reals lab extension\PLAN.md
   - c:\Users\smk28\Desktop\reals lab extension\SPEC.md
   - c:\Users\smk28\Desktop\reals lab extension\DESIGN.md
2. Use GitNexus MCP tools to examine existing code and specifications.
3. Mine and document exact specifications for:
   - R1: AI inference engine (ONNX Runtime C++, model loading from `%APPDATA%\RealsLab\models\`, TempoCNN + RhythmExtractor2013 fallback, EDMA Key Voting + Temperley/Krumhansl, Discogs-MAEST 400 subgenres, Mood-Jamendo multi-label, CLAP 512-dim embedding extraction).
   - R2: Multi-threaded background scanner pool, SQLite cache schema (with hash checksum & vector embedding storage), syntax `/` search parser (`/tag`, `/bpm:min-max`, `/key:val`), and cosine similarity semantic search.
   - R3: DSP SoundTouch / RubberBand time-stretch engine with REAPER `Master_GetTempo()` BPM sync, and ±12 semitones real-time pitch shifter with < 30ms latency.
   - Acceptance criteria A1-A4 mapping and test requirements.
4. Update `progress.md` in your working directory as you work.
5. Write a comprehensive, structured handoff report to: `c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_survey_1\handoff.md`.
6. Use `send_message` to report back to your parent when done with the path to your handoff report.
