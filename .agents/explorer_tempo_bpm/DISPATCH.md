## 2026-08-31T02:42:06Z
Received request from orchestrator:
Investigate BPM sync and Time-Stretching calculations:
- SoundTouch time-stretching ratio calculations in Bridge.cpp (projectBpm / sampleBpm vs timeRatio vs setTempo vs setRate).
- Difference between tempo ratio (speed without pitch change) and playback rate.
- How BPM sync is enabled/disabled, how project BPM is acquired from REAPER (TimeMap_GetDividedBpmAtTime vs Master_GetTempo), and how sample BPM is detected/passed.
- Identify why preview plays faster than DAW tempo (is it multiplied twice, inverted ratio, or sample rate ratio conflated with tempo ratio?).
- Report exact file paths, line numbers, mathematical analysis, and recommended fixes in handoff.md.
