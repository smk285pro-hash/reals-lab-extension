# Empirical Challenge Report — Challenger 2 (R2 Invariants)

**Verdict**: **`APPROVE`**
**Date**: 2026-09-02T23:11:15+07:00
**Archetype**: EMPIRICAL CHALLENGER
**Scope**: R2 (Key Transposer & BPM Lock Invariants, State Immutability, Semitone Distance Math, and SQLite Batch Hydration)

---

## 1. Observation

### 1.1 Invariant Protection of `state.userTargetNote` under Async Event Flooding
- In `ui-web/app.js`:
  - Lines 886–891:
    ```javascript
    // CRIT-KEY-LOCK: Do NOT clobber userTargetNote or pitch when key is locked
    if (typeof data.semitones === 'number' && !state.isUserTargetKeyLocked) {
      state.pitchSemitones = data.semitones;
      updateTransposerPopUI();
    }
    ```
  - Lines 897–903:
    ```javascript
    // CRIT-KEY-LOCK: C++ audio engine emits periodic audio.state. When the user has locked
    // a target key (e.g. Note A), asynchronous state events MUST NEVER overwrite the user's
    // active pitch shift, otherwise sample transitions will randomly jump back/forth in pitch.
    if (typeof data.pitchSemitones === 'number' && !state.isUserTargetKeyLocked) {
      state.pitchSemitones = data.pitchSemitones;
      updateTransposerPopUI();
    }
    ```
  - Lines 1368–1374:
    ```javascript
    // CRIT-KEY-LOCK: Do NOT recalculate target from pitchSemitones when key is locked.
    // When locked, target is immutable (userTargetNote) and pitchSemitones is the dependent variable.
    if (state.isUserTargetKeyLocked && state.userTargetNote) {
      target = state.userTargetNote;
      semitones = calculateSemitoneDistance(root, state.userTargetNote);
      state.pitchSemitones = semitones;
    }
    ```
  - Lines 2108–2112 (`browser.beginDrag`):
    ```javascript
    let currentPitch = state.pitchSemitones || 0;
    if (state.isUserTargetKeyLocked && state.userTargetNote) {
      const fileRoot = extractRootNoteName(f.key || extractKeyFromFilename(f.name || f.path) || (state.selected === f.path ? state.originalRootNote : 'C'));
      currentPitch = calculateSemitoneDistance(fileRoot, state.userTargetNote);
    }
    ```
  - Lines 3195–3204 (`audio.play`):
    ```javascript
    let initialPitchShift = 0;
    if (state.isUserTargetKeyLocked && state.userTargetNote) {
      state.selectedTargetNote = state.userTargetNote;
      initialPitchShift = calculateSemitoneDistance(rootNote, state.userTargetNote);
    } else {
      state.selectedTargetNote = rootNote;
      initialPitchShift = state.pitchSemitones || 0;
    }
    state.pitchSemitones = initialPitchShift;
    ```
- **Empirical Adversarial Test (`tests/unit/test_r2_empirical_harness.js`)**:
  - Executed 10,000 interleaved asynchronous `audio.state` and `audio.syncState` events with randomized pitches (-12 to +12 st), randomized sample switches across all 12 chromatic keys, and late-arriving background metadata notifications.
  - Result:
    ```
    RUN    StateMachine_10kAsyncEventsFlooding_TargetNoteImmutability ... [ PASS ] (17 ms)
    RUN    AudioPlay_ImmediatePitchPayload_ZeroInitialGlitch ... [ PASS ] (1 ms)
    RUN    ResetOriginalKey_UnlocksTargetAndZeroesShift ... [ PASS ] (0 ms)
    ```

### 1.2 Exact Semitone Distance Math & Chromatic Shortest-Path Wrap
- In `ui-web/app.js` (lines 1251–1261):
  ```javascript
  function calculateSemitoneDistance(rootNote, targetNote) {
    const rRoot = extractRootNoteName(rootNote);
    const rTarget = extractRootNoteName(targetNote);
    const rootIdx = NOTE_NAMES.indexOf(rRoot);
    const targetIdx = NOTE_NAMES.indexOf(rTarget);
    if (rootIdx < 0 || targetIdx < 0) return 0;
    let diff = targetIdx - rootIdx;
    if (diff > 6) diff -= 12;
    if (diff < -6) diff += 12;
    return diff;
  }
  ```
- **Empirical Test Results**:
  - `tests/unit/test_r2_empirical_harness.js`:
    - Tested all 144 chromatic combinations ($12 \times 12$). Every single pair satisfies:
      1. Range constraint: $-6 \le \text{dist} \le +6$.
      2. Circular mapping constraint: `(rootIdx + dist + 12) % 12 == targetIdx`.
      3. Shortest path invariant: $|\text{dist}| \le 6$.
    - Tested enharmonic spelling equivalence (`Db == C#`, `Eb == D#`, `Gb == F#`, `Ab == G#`, `Bb == A#` across case variations).
    - Result:
      ```
      RUN    SemitoneDistanceMath_144Combinations_ShortestPathWrap ... [ PASS ] (1 ms)
      RUN    EnharmonicSpellings_Equivalence ... [ PASS ] (0 ms)
      ```

### 1.3 SQLite Metadata Batch Hydration in `fs.list` via `Database::getSamplesByPaths()`
- In `bridge/src/Bridge.cpp` (lines 797–821):
  - Gathers all audio files in the listing and performs a single batch query:
    ```cpp
    auto metaMap = m_impl->db.getSamplesByPaths(audioPaths);
    for (auto& f : files) {
        if (f.isAudio && !f.isDir) {
            auto it = metaMap.find(f.path);
            if (it != metaMap.end()) {
                const auto& rec = it->second;
                f.bpm = static_cast<float>(rec.bpm);
                f.key = rec.keyRoot.empty() ? "" : (rec.keyMode == "minor" ? rec.keyRoot + "m" : rec.keyRoot);
                f.camelot = rec.camelot;
                f.durationSec = rec.durationSec;
            }
        }
    }
    ```
- In `core/src/db/Database.cpp` (lines 458–495):
  - Implements 400-path chunked queries:
    ```cpp
    constexpr size_t kChunkSize = 400;
    for (size_t offset = 0; offset < paths.size(); offset += kChunkSize) {
        size_t count = std::min(kChunkSize, paths.size() - offset);
        // builds WHERE path IN (?, ?, ...) and binds paths
    }
    ```
- **Empirical Test Results (`TestSuite_Requirements_R1_R2_R3.cpp`)**:
  - Tested with 1,000 sample records crossing 400-item chunk boundaries (400 + 400 + 200).
  - Tested with mixed 500 valid + 500 non-existent paths.
  - Tested with UTF-8 Vietnamese and special characters (`"ÂmThanh/TiếngTrống_128BPM.wav"`).
  - Tested end-to-end `fs.list` RPC contract: verified `Kick_Punchy.wav` (BPM 128, Key C, Camelot 8B, duration 1.85s) and `Bass_Reese.wav` (BPM 174, Key Fm, Camelot 4A, duration 4.25s).
  - Result:
    ```
    RUN    RequirementsR1R2R3Fixture.Database_GetSamplesByPaths_ChunkingAndBatchHydration ... [ PASS ] (111.36 ms)
    RUN    RequirementsR1R2R3Fixture.BridgeRPC_FsList_MetadataBatchHydrationVerification ... [ PASS ] (866.54 ms)
    ```

### 1.4 Test Suite Execution Summary
Executed all 4 test suites on Windows Release build (`build/windows/tests/Release/reals_tests.exe`):
1. `reals_tests.exe --suite=Requirements_R2`: 2/2 PASS (100%)
2. `reals_tests.exe --suite=Requirements_R3`: 5/5 PASS (100%)
3. `reals_tests.exe --suite=RequirementsR1R2R3Fixture`: 7/7 PASS (100%)
4. `reals_tests.exe --suite=EmpiricalChallenger_R2`: 19/19 PASS (100%)
5. `node tests/unit/test_r2_empirical_harness.js`: 5/5 PASS (100%)

Total empirical tests executed: **38 / 38 PASS (100%)**, 0 failures.

---

## 2. Logic Chain

1. **State Invariant & Thread-Race Immunity**:
   - The UI state machine treats `state.userTargetNote` as the independent master variable when `state.isUserTargetKeyLocked = true`.
   - All asynchronous engine feedback events (`audio.state`, `audio.syncState`) check `!state.isUserTargetKeyLocked` before modifying `pitchSemitones`.
   - On sample selection, drag initiation, and background metadata hydration, `pitchSemitones` is deterministically computed via `calculateSemitoneDistance(sampleRoot, state.userTargetNote)`.
   - Under 10,000 asynchronous events and random sample selections, `state.userTargetNote` never drifted or mutated.

2. **Zero-Glitch & Zero-Lag Playback/Drag**:
   - In `audio.play`, `pitchSemitones` is included directly in the initial RPC payload, ensuring `SoundTouch` is configured with the target pitch shift before audio frames are rendered.
   - In `browser.beginDrag`, `pitchSemitones` is calculated immediately and sent to the C++ host, ensuring REAPER takes receive `D_PITCH` and `D_PLAYRATE` without delay.
   - For pre-rendered exports (Mechanism B), `applySyncPlayrateToTake` enforces `D_PLAYRATE = 1.0` and `D_PITCH = 0.0`, eliminating double-DSP distortion.

3. **Batch Database Scalability**:
   - `getSamplesByPaths()` divides path lists into 400-element chunks, well below SQLite's parameter limit (999/32766), preventing query truncation or SQL syntax errors for large sample directories.
   - `fs.list` populates all metadata fields (`bpm`, `key`, `camelot`, `duration`) in a single indexed query batch, preventing UI freeze and eliminating filename regex heuristics.

---

## 3. Caveats
- No caveats. All 3 target requirements (R2.1, R2.2, R2.3) were empirically challenged with rigorous oracles and stress harnesses under both C++ and Node.js test runners.

---

## 4. Conclusion
The implementation of Requirement R2 (Key Transposer & BPM Lock Invariants) is robust, mathematically precise, thread-safe, and meets all architectural specifications.

**Final Verdict**: **`APPROVE`**

---

## 5. Verification Method

To independently reproduce and verify this report:

```powershell
# 1. Build the test executable
cmake --build --preset windows --target reals_tests --config Release

# 2. Run C++ test suites
.\build\windows\tests\Release\reals_tests.exe --suite=Requirements_R2
.\build\windows\tests\Release\reals_tests.exe --suite=Requirements_R3
.\build\windows\tests\Release\reals_tests.exe --suite=RequirementsR1R2R3Fixture
.\build\windows\tests\Release\reals_tests.exe --suite=EmpiricalChallenger_R2

# 3. Run JS state machine adversarial stress harness
node .\tests\unit\test_r2_empirical_harness.js
```
