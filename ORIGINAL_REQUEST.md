# Original User Request

## 2026-09-02T13:26:27Z

Conduct a comprehensive investigation of all logic and algorithmic errors in file browsing, BPM detection/synchronization, and musical Key/Tone transposition across Reals Lab (core/, bridge/, and ui-web/).

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

## Requirements

### R1. Root Cause Audit for Key/Tone Transposer & Root Note Fallback
Audit the entire Tone Transposer pipeline (KeyDetector, detectKeyForPath, extractKeyFromFilename, extractRootNoteName, calculateSemitoneDistance, and setPitchShift). Identify why unlabelled samples (files without Key in their filename) default to 'C', causing semitone calculations to apply incorrect pitch shifts when the user selects a target key on the piano keyboard.

### R2. Algorithmic BPM Detection & Metadata Propagation Deep Audit
Analyze the BPM extraction and time-stretch ratio calculation (TempoDetector, detectBpmForPath, fs.list, BrowserModel::listDir, db::Database). Investigate why unlabelled audio samples (loops without "XXXbpm" in their filename) fail to resolve their true tempo or get assigned inaccurate fallback values, leading to improper time-stretching during preview and DAW drag-and-drop.

### R3. File Browser Metadata Hydration & Database Sync Analysis
Examine fs.list vs db::Database metadata synchronization. Determine why directory navigation returns bare file system entries without hydrating pre-computed BPM, Key, and Duration from the SQLite library database, leaving the frontend relying on superficial filename heuristics.

### R4. Programmatic Accuracy Benchmark & Fix Roadmap
Develop a concrete verification suite and benchmark measuring:
1. Musical key detection accuracy and semitone transposition correctness on labelled vs unlabelled samples.
2. Tempo detection accuracy (within ±1 BPM) across drum loops, melodies, and full mixes.
3. Database metadata coverage during file listing.

## Acceptance Criteria

### Technical Analysis & Bug Diagnostics
- [ ] Detailed trace explaining why unlabelled samples default to Root 'C' and distort user pitch transposition.
- [ ] Detailed trace explaining why TempoDetector fails or produces octave errors on unlabelled loops.
- [ ] Evaluation of fs.list missing DB metadata hydration.

### Empirical Benchmarking & Verification
- [ ] Benchmark testing key detection on 12 chromatic scales with harmonic Chroma verification.
- [ ] Benchmark testing tempo detection across standard EDM/Hip-hop/Pop tempos (70-175 BPM).
