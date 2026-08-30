## 2026-08-30T19:42:06Z
You are Spec Miner (REAPER 8-Point Playhead Phase Sync Spec Investigator).
Your working directory is: c:\Users\smk28\Desktop\reals lab extension\.agents\spec_miner_phase_sync

Audit extension/src/reaper_plugin.cpp, core/src/audio/Engine.cpp, and bridge/src/Bridge.cpp against the 8-Point Playhead Phase Sync Master Specification:
- Point 1: Goal (Beat phase sync between preview and REAPER playhead without drift/jitter)
- Point 2: Two-tier access (Main Thread vs Audio Hook)
- Point 3: Registration (Audio_RegHardwareHook, GetBuffer)
- Point 4: Position & Tempo (GetPlayPosition2Ex, TimeMap_GetDividedBpmAtTime / Master_GetTempo, TimeMap2_timeToBeats)
- Point 5: Phase calculation loop (Discontinuity detection)
- Point 6: Thread communication (std::atomic relaxed)
- Point 7: Phase compensation (PI controller / hard seek)
- Point 8: Safety rules (Zero malloc, zero file I/O, zero mutex in audio thread)
