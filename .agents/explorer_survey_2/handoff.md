# Survey Phase Investigation Report: R2 — Key Transposer & BPM Lock Invariant Verification

**Explorer**: Explorer 2 (Survey Phase)  
**Target Milestone**: R2 (Key Transposer & BPM Lock Invariant Verification)  
**Date**: 2026-09-02  
**Status**: COMPLETE / VERIFIED  

---

## 1. Observation

### 1.1 State Management & Immutability Invariant (`state.isUserTargetKeyLocked` & `state.userTargetNote`)

Direct code inspection of `ui-web/app.js` reveals the exact lifecycle and guarding mechanisms for locked key transposition:

1. **Explicit Locking and Unlocking Triggers**:
   - **`setTargetNote(targetNote)`** (`ui-web/app.js:1442-1451`):
     ```javascript
     async function setTargetNote(targetNote) {
       const root = state.originalRootNote || 'C';
       const validTarget = extractRootNoteName(targetNote);
       state.isUserTargetKeyLocked = true;
       state.userTargetNote = validTarget;
       state.selectedTargetNote = validTarget;
       const semitones = calculateSemitoneDistance(root, validTarget);
       await setPitchShift(semitones);
       playSynthNote(validTarget, 0.3);
     }
     ```
   - **`resetOriginalKey()`** (`ui-web/app.js:1474-1480`):
     ```javascript
     async function resetOriginalKey() {
       state.isUserTargetKeyLocked = false;
       state.userTargetNote = null;
       state.selectedTargetNote = state.originalRootNote || 'C';
       await setPitchShift(0);
       playSynthNote(state.selectedTargetNote, 0.3);
     }
     ```

2. **Guards Against Asynchronous C++ Bridge Audio Events**:
   - **`onBridgeEvent` for `audio.syncState`** (`ui-web/app.js:880-893`):
     ```javascript
     if (event === 'audio.syncState') {
       if (data) {
         if (typeof data.syncBpm === 'boolean') {
           state.syncBpm = data.syncBpm;
           $('#btnSyncBpm')?.classList.toggle('on', state.syncBpm);
         }
         // CRIT-KEY-LOCK: Do NOT clobber userTargetNote or pitch when key is locked
         if (typeof data.semitones === 'number' && !state.isUserTargetKeyLocked) {
           state.pitchSemitones = data.semitones;
           updateTransposerPopUI();
         }
       }
       return;
     }
     ```
   - **`onBridgeEvent` for `audio.state`** (`ui-web/app.js:894-904`):
     ```javascript
     if (event === 'audio.state') {
       state.playing = !!data.playing;
       state.duration = data.duration || state.duration;
       // CRIT-KEY-LOCK: C++ audio engine emits periodic audio.state. When the user has locked
       // a target key (e.g. Note A), asynchronous state events MUST NEVER overwrite the user's
       // active pitch shift, otherwise sample transitions will randomly jump back/forth in pitch.
       if (typeof data.pitchSemitones === 'number' && !state.isUserTargetKeyLocked) {
         state.pitchSemitones = data.pitchSemitones;
         updateTransposerPopUI();
       }
     ```

3. **Sample Selection Invariant Preservation**:
   - **Row Click / Selection** (`ui-web/app.js:2503-2510`):
     ```javascript
     if (state.isUserTargetKeyLocked && state.userTargetNote) {
       state.selectedTargetNote = state.userTargetNote;
       state.pitchSemitones = calculateSemitoneDistance(rootNote, state.userTargetNote);
     } else {
       state.selectedTargetNote = rootNote;
       state.pitchSemitones = 0;
     }
     updateTransposerPopUI();
     ```
   - **Async MIDI Preview Load** (`ui-web/app.js:2518-2525`):
     ```javascript
     if (state.isUserTargetKeyLocked && state.userTargetNote) {
       state.selectedTargetNote = state.userTargetNote;
       state.pitchSemitones = calculateSemitoneDistance(state.originalRootNote, state.userTargetNote);
     } else {
       state.selectedTargetNote = state.originalRootNote;
       state.pitchSemitones = 0;
     }
     updateTransposerPopUI();
     ```

4. **Background Metadata Hydration Invariant Preservation**:
   - **`playFile` Asynchronous Hydration Callback** (`ui-web/app.js:3262-3276`):
     ```javascript
     const detectedRoot = extractRootNoteName(state.sampleKey);
     state.originalRootNote = detectedRoot;
     if (state.isUserTargetKeyLocked && state.userTargetNote) {
       state.selectedTargetNote = state.userTargetNote;
       state.pitchSemitones = calculateSemitoneDistance(detectedRoot, state.userTargetNote);
     } else {
       state.selectedTargetNote = detectedRoot;
       state.pitchSemitones = 0;
     }
     updateTransposerPopUI();
     if (state.pitchSemitones !== 0) {
       bridge('audio.setPitchShift', { semitones: state.pitchSemitones }).catch(()=>{});
     }
     ```

5. **Exhaustive Variable Scan**:
   - Grep search across `ui-web/app.js` confirmed that `state.userTargetNote` is written ONLY in lines 1446 (`setTargetNote`) and 1476 (`resetOriginalKey`). In all other 9 occurrences, it is read-only.

---

### 1.2 Semitone Calculation & Zero-Glitch / Zero-Lag Pipeline

1. **Transposition Distance Math**:
   - **`calculateSemitoneDistance`** (`ui-web/app.js:1251-1261`):
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
   - **Root Note Normalization** (`ui-web/app.js:1237-1249`):
     ```javascript
     function extractRootNoteName(keyStr) {
       if (!keyStr || keyStr === 'ORIGINAL' || keyStr === 'UNKNOWN') return 'C';
       const s = String(keyStr).trim();
       const m = s.match(/(?:^|[\s_\-\(\[])([A-Ga-g][#b]?)(?:m|maj|min|minor|major)?(?:[\s_\-\)\]]|$)/i) || s.match(/^([A-Ga-g][#b]?)/i);
       if (!m) return 'C';
       let r = m[1].toUpperCase();
       if (r === 'DB') r = 'C#';
       else if (r === 'EB') r = 'D#';
       else if (r === 'GB') r = 'F#';
       else if (r === 'AB') r = 'G#';
       else if (r === 'BB') r = 'A#';
       return r;
     }
     ```

2. **Audio Play Initial Payload Transmission** (`ui-web/app.js:3195-3213`):
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
   updateTransposerPopUI();

   const sampleBpm = (fileObj && fileObj.bpm) || (state.selected === path ? state.sampleBpm : 0) || 0;
   const d = await bridge('audio.play', {
     path,
     loop: state.loop,
     syncBpm: !!state.syncBpm,
     sampleBpm: sampleBpm,
     pitchSemitones: initialPitchShift
   });
   ```

3. **C++ Bridge `audio.play` Pitch Configuration** (`bridge/src/Bridge.cpp:947-948`):
   ```cpp
   const float pitchShift = static_cast<float>(args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones())));
   eng.setPitchSemitones(pitchShift);
   ```
   SoundTouch receives the pitch shift setting *prior* to starting playback, eliminating initial unshifted audio glitch.

4. **OLE Drag Start Pitch & Playrate Transmission** (`ui-web/app.js:2108-2118`):
   ```javascript
   let currentPitch = state.pitchSemitones || 0;
   if (state.isUserTargetKeyLocked && state.userTargetNote) {
     const fileRoot = extractRootNoteName(f.key || extractKeyFromFilename(f.name || f.path) || (state.selected === f.path ? state.originalRootNote : 'C'));
     currentPitch = calculateSemitoneDistance(fileRoot, state.userTargetNote);
   }
   bridge('browser.beginDrag', {
     path: f.path,
     syncBpm: !!state.syncBpm,
     sampleBpm: f.bpm || (state.selected === f.path ? state.sampleBpm : 0) || 0,
     pitchSemitones: currentPitch
   }).catch(() => {});
   ```

5. **Mechanism A (Native REAPER Drag) Bridge Queueing** (`bridge/src/Bridge.cpp:1838-1877`):
   ```cpp
   double pitchShift = static_cast<double>(eng.getPitchSemitones());
   if (args.contains("pitchSemitones") && args["pitchSemitones"].is_number()) {
       pitchShift = args["pitchSemitones"].get<double>();
   }
   ...
   if (syncOn || std::abs(pitchShift) > 0.001) {
       m_actions->queueSyncPlayrate(p, playrate, pitchShift);
   }
   m_actions->beginDrag(p);
   ```

6. **REAPER Plugin Take Injection & Grid Alignment** (`extension/src/reaper_plugin.cpp:233-250`):
   ```cpp
   // Mechanism A: Native REAPER Drag & Playrate Alignment
   double curRate = GetMediaItemTakeInfo_Value ? GetMediaItemTakeInfo_Value(take, "D_PLAYRATE") : 1.0;
   double curLen = GetMediaItemInfo_Value ? GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;

   SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", it->playrate);
   SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1); // preserve pitch when stretching
   SetMediaItemTakeInfo_Value(take, "D_PITCH", it->pitchSemitones);
   if (curLen > 0.0 && curRate > 0.0 && it->playrate > 0.0 && SetMediaItemInfo_Value) {
       // Adjust item boundary so the full loop fits the project tempo grid bar
       double origLen = curLen * curRate;
       double newLen = origLen / it->playrate;
       SetMediaItemInfo_Value(item, "D_LENGTH", newLen);
   }
   if (UpdateItemInProject) UpdateItemInProject(item);
   ```

---

### 1.3 SQLite Metadata Hydration in `fs.list` via `Database::getSamplesByPaths()`

1. **Bridge Batch Hydration in `fs.list`** (`bridge/src/Bridge.cpp:800-826`):
   ```cpp
   std::vector<std::string> audioPaths;
   audioPaths.reserve(files.size());
   for (const auto& f : files) {
       if (f.isAudio && !f.isDir) {
           audioPaths.push_back(f.path);
       }
   }
   if (!audioPaths.empty()) {
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
   }

   json arr = json::array();
   for (const auto& f : files)
       arr.push_back(entryToJson(f));
   res["ok"] = true;
   res["data"] = arr;
   ```

2. **`Database::getSamplesByPaths` Implementation** (`core/src/db/Database.cpp:458-495`):
   - Thread safe: Protected by `const std::lock_guard lock(m_mutex);`.
   - Chunked SQL query generation: Partitions paths into 400-path chunks to adhere to SQLite parameter limits.
   - Resource safe: Wrapped in `StmtGuard guard{stmt};`.
   - Populates `std::unordered_map<std::string, SampleRecord>`.

3. **Hydration Benchmark Verification** (`tests/benchmarks/TestSuite_EmpiricalBenchmark_M4.cpp:518-650`):
   - Confirmed: Listing unhydrated coverage is 0.0% BPM, 0.0% Key, 0.0% Duration.
   - Confirmed: Post-hydration coverage reaches 100.0% with sub-15ms batch lookup times across large directories.

---

### 1.4 GitNexus MCP Analysis Findings

Using GitNexus CLI and code graph queries:
- **Symbol Analysis**:
  - `Database::getSamplesByPaths`: Defined in `core/include/reals/db/Database.h:64` and implemented in `core/src/db/Database.cpp:458-495`.
  - `browser.beginDrag`: Defined in `bridge/include/reals/bridge/Bridge.h:51`, implemented in `bridge/src/Bridge.cpp:1827-1877` and `extension/src/reaper_plugin.cpp:592-598`.
  - `calculateSemitoneDistance`: Implemented in `ui-web/app.js:1251-1261`.
- **Impact & Blast Radius**:
  - `getSamplesByPaths` blast radius: Direct callers `Bridge.cpp:808` (`fs.list`). Safe, isolated dependency.
  - `beginDrag` blast radius: Direct callers `ui-web/app.js:2113`, `extension/src/reaper_plugin.cpp`, `TestSuite_EmpiricalChallenger_R2.cpp`.
- **Index Statistics**:
  - Repository contains 2,880 nodes, 7,066 edges, 110 functional clusters, 187 execution flows.

---

## 2. Logic Chain

1. **State Preservation Verification**:
   - Observation 1.1.1 shows that `state.userTargetNote` is initialized on user request (`setTargetNote`) and cleared only on user request (`resetOriginalKey`).
   - Observations 1.1.2, 1.1.3, and 1.1.4 demonstrate that across sample selection, MIDI preview generation, periodic `audio.state`/`audio.syncState` ticks, and background AI metadata hydration, every code branch checks `state.isUserTargetKeyLocked`.
   - When locked, `state.selectedTargetNote` is assigned `state.userTargetNote`, and `state.pitchSemitones` is re-calculated as a dependent variable of the sample's root note.
   - Therefore, the user's locked target key is strictly immutable and cannot be corrupted by external or asynchronous events.

2. **Semitone Calculation & Latency Verification**:
   - Observation 1.2.1 shows that `calculateSemitoneDistance` accurately computes the shortest circular distance between two chromatic pitch classes, bounding shifts to `[-6, +6]` semitones.
   - Observation 1.2.2 & 1.2.3 verify that when `audio.play` is triggered, the computed pitch shift is packaged in the initial RPC request and applied to the DSP engine prior to playback start, eliminating any unshifted audio playback or audible glitch.
   - Observation 1.2.4, 1.2.5, and 1.2.6 verify that `browser.beginDrag` computes the pitch shift relative to the target sample and user locked key, queues the parameters in `queueSyncPlayrate`, and initiates native REAPER OLE drag on the original file path (0ms disk render lag).
   - In REAPER, `processPendingSyncPlayrates` applies `D_PLAYRATE`, `B_PPITCH = 1`, and `D_PITCH`, while adjusting item length (`D_LENGTH = (curLen * curRate) / playrate`) to ensure perfect tempo grid alignment.

3. **Metadata Hydration Verification**:
   - Observation 1.3.1 & 1.3.2 show that `fs.list` executes `Database::getSamplesByPaths(audioPaths)` for all audio files in the queried folder.
   - The query uses prepared `WHERE path IN (?,?,...)` batches of up to 400 paths, fetching BPM, Key, Camelot, and Duration into the returned `FileEntry` objects.
   - This ensures the UI is populated with genuine indexed metadata rather than relying on filename regexes or defaulting to 0 BPM / 'C' key.

---

## 3. Caveats

1. **Shortest Distance Interval Choice**: `calculateSemitoneDistance` restricts transposition to `[-6, +6]` semitones (nearest octave). If a user specifically desires an octave shift (+12st / -12st), that must be manually adjusted via semitone controls, as the key locking system defaults to minimum chromatic displacement for acoustic naturalness.
2. **Pre-Rendered WAV Safeguard (Mechanism B)**: When dragging pre-rendered files (e.g. from DragExporter temp cache), `reaper_plugin.cpp` enforces `D_PLAYRATE = 1.0` and `D_PITCH = 0.0` to avoid double-processing DSP.

---

## 4. Conclusion

- **Audit Status**: PASS.
- All invariants specified in Requirement R2 are fully satisfied, robustly implemented, and verified against the codebase:
  1. `state.isUserTargetKeyLocked` strictly protects `state.userTargetNote` across all state transitions, background events, and user interactions.
  2. Semitone calculations in `audio.play` and `browser.beginDrag` are exact, instantaneous, and glitch-free.
  3. SQLite metadata hydration in `fs.list` via `Database::getSamplesByPaths()` reliably populates BPM, Key, Camelot, and Duration for all indexed audio files.

---

## 5. Verification Method

### 5.1 Independent Code Inspection
Inspect the following files and line numbers:
- `ui-web/app.js`: lines 880-920 (`audio.syncState` & `audio.state`), 1251-1261 (`calculateSemitoneDistance`), 1442-1451 (`setTargetNote`), 2108-2118 (`browser.beginDrag`), 2503-2510 (`selectFile`), 3195-3213 (`audio.play`), 3262-3276 (`audio.getSampleMeta` callback).
- `bridge/src/Bridge.cpp`: lines 800-826 (`fs.list`), 947-948 (`audio.play`), 1838-1877 (`browser.beginDrag`).
- `core/src/db/Database.cpp`: lines 458-495 (`Database::getSamplesByPaths`).
- `extension/src/reaper_plugin.cpp`: lines 233-250 (`processPendingSyncPlayrates`).

### 5.2 Test Execution
Run the automated test suites using the project test runner:
```powershell
# Run the complete test suite
.\build\windows\tests\Release\reals_tests.exe

# Run via ctest preset
ctest --preset windows --output-on-failure
```

**Verified Test Output**:
```
Test project C:/Users/smk28/Desktop/reals lab extension/build/windows
    Start 1: reals_e2e_tests
1/1 Test #1: reals_e2e_tests ..................   Passed  222.50 sec

100% tests passed, 0 tests failed out of 1

======================================================================
                          TEST SUMMARY
======================================================================
  Total Executed : 334
  Passed         : 334
  Failed         : 0
  Total Time     : 95009 ms

  >>> 100% ALL TESTS PASSED SUCCESSFULLY! <<<
```

### 5.3 Invalidation Conditions
The verification is invalidated if:
1. `onBridgeEvent` for `audio.state` or `audio.syncState` is modified to mutate `state.pitchSemitones` when `state.isUserTargetKeyLocked == true`.
2. `audio.play` is triggered without passing `pitchSemitones` in the initial payload.
3. `fs.list` in `Bridge.cpp` omits `db.getSamplesByPaths()` batch hydration.
