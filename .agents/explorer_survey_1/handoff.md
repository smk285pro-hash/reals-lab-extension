# Handoff Report: DAW Drag & Drop Alignment & Double-DSP Problem

**Explorer**: explorer_survey_1  
**Working Directory**: `c:\Users\smk28\Desktop\reals lab extension\.agents\explorer_survey_1`  
**Date**: 2026-08-28T15:45:00Z  
**Type**: Hard Handoff  

---

## 1. Observation

Direct observations from source code inspection:

1. **`bridge/src/Bridge.cpp:1433–1473` (`browser.beginDrag`)**:
   ```cpp
   } else if (cmd == "browser.beginDrag") {
       const std::string p = narrowPath(args.value("path", ""));
       if (m_actions) {
           bool syncOn = args.value("syncBpm", false);
           ...
           double pitchShift = args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones()));
           std::string pathToDrag = p;

           if (syncOn || std::abs(pitchShift) > 0.001) {
               float sampleBpm = args.value("sampleBpm", 0.0f);
               if (sampleBpm <= 0.0f) {
                   sampleBpm = m_impl->detectBpmForPath(p);
               }
               double projectBpm = m_actions->projectTempo();
               double playrate = 1.0;
               if (sampleBpm > 30.0f && projectBpm > 30.0) {
                   playrate = projectBpm / sampleBpm;
                   playrate = std::clamp(playrate, 0.25, 4.0);
               } else if (std::abs(syncRatio - 1.0f) > 0.001f) {
                   playrate = std::clamp(static_cast<double>(syncRatio), 0.25, 4.0);
               }
               m_actions->queueSyncPlayrate(p, playrate, pitchShift);

               reals::audio::DragExportOptions opt;
               opt.timeRatio = static_cast<float>(playrate);
               opt.pitchSemitones = static_cast<float>(pitchShift);
               const auto dragRes = reals::audio::DragExporter::exportTempWav(p, opt);
               if (dragRes.success && !dragRes.renderedPath.empty()) {
                   pathToDrag = dragRes.renderedPath;
                   m_actions->queueSyncPlayrate(pathToDrag, playrate, pitchShift);
               }
           }
           m_actions->beginDrag(pathToDrag);
       }
       res["ok"] = true;
   ```

2. **`core/src/audio/DragExporter.cpp:278–287` (`exportTempWav`)**:
   ```cpp
   const bool needsDsp = (std::abs(clampedRatio - 1.0f) > 0.001f || std::abs(clampedPitch) > 0.001f);
   if (needsDsp) {
       SoundTouchProcessor processor(sampleRate, channels, true);
       processor.setTimeRatio(clampedRatio);
       processor.setPitchSemitones(clampedPitch);
       outputPcm = processor.processBuffer(pcmBuffer.data(), static_cast<size_t>(framesRead));
   } else {
       outputPcm = std::move(pcmBuffer);
   }
   ```

3. **`extension/src/reaper_plugin.cpp:139–159` (`processPendingSyncPlayrates`)**:
   ```cpp
   auto applyToTake = [&](MediaItem* item, MediaItem_Take* take) -> bool {
       if (!item || !take || !SetMediaItemTakeInfo_Value) return false;
       double curRate = GetMediaItemTakeInfo_Value ? GetMediaItemTakeInfo_Value(take, "D_PLAYRATE") : 1.0;
       double curLen = GetMediaItemInfo_Value ? GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;

       SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", it->playrate);
       SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1); // preserve pitch when stretching
       if (std::abs(it->pitchSemitones) > 0.001) {
           SetMediaItemTakeInfo_Value(take, "D_PITCH", it->pitchSemitones);
       }
       if (curLen > 0.0 && curRate > 0.0 && SetMediaItemInfo_Value) {
           // Adjust item boundary so the full loop fits the project tempo
           double origLen = curLen * curRate;
           double newLen = origLen / it->playrate;
           SetMediaItemInfo_Value(item, "D_LENGTH", newLen);
       }
       char msg[256];
       std::snprintf(msg, sizeof(msg), "Synced item to playrate %.4f (pitch %.2f)", it->playrate, it->pitchSemitones);
       LOG_INFO(kTag, msg);
       return true;
   };
   ```

4. **`shell/win/OleDrag.cpp:272–290` (`beginFileDrag`)**:
   ```cpp
   void beginFileDrag(HWND /*owner*/, const std::wstring& path) {
       if (path.empty())
           return;
       ReleaseCapture();
       g_internalDrag = true;
       auto* src = new DropSource();
       auto* data = new FileDataObject(path);
       DWORD effect = 0;
       const HRESULT hr =
           DoDragDrop(data, src, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect);
       g_internalDrag = false;
       ...
       src->Release();
       data->Release();
   }
   ```

---

## 2. Logic Chain

1. **Step 1 (Offline Pre-render)**: From `Bridge.cpp:1463–1470`, when `syncBpm` is true or pitch is shifted, `DragExporter::exportTempWav` decodes the input audio and processes it through `SoundTouchProcessor` with `timeRatio = playrate` and `pitchSemitones = pitchShift`. It writes a newly stretched/shifted file `drag_xxx.wav` to disk.
2. **Step 2 (Queue Duplication)**: From `Bridge.cpp:1469`, `m_actions->queueSyncPlayrate(pathToDrag, playrate, pitchShift)` queues the newly rendered `drag_xxx.wav` path into `g_pendingPlayrates` with the same `playrate` and `pitchShift`.
3. **Step 3 (OLE Drag)**: From `OleDrag.cpp:272–290` and `reaper_plugin.cpp:371–377`, `DoDragDrop` initiates a Win32 OLE drag operation containing `drag_xxx.wav`.
4. **Step 4 (Timeline Drop & Secondary Processing)**: When the user drops `drag_xxx.wav` onto the REAPER timeline, REAPER imports `drag_xxx.wav` as a media item take. `reaper_plugin.cpp:120–229` (`processPendingSyncPlayrates()`) detects the new take matching `drag_xxx.wav` and modifies:
   - `D_PLAYRATE = playrate` (REAPER's native Élastique engine stretches the already-stretched audio a 2nd time).
   - `D_PITCH = pitchSemitones` (REAPER shifts the already-shifted pitch a 2nd time).
   - `D_LENGTH = curLen / playrate` (Divides the already-shortened duration by `playrate` again).
5. **Step 5 (Consequences)**:
   - Playback speed: $(\text{playrate})^2$ (e.g. $1.1667 \times 1.1667 = 1.3611\times$, sounds 2x too fast and distorted).
   - Pitch transposition: $2 \times \text{pitch}$ (e.g. $+3\text{ st} + 3\text{ st} = +6\text{ st}$).
   - Timeline length: $\text{length} / (\text{playrate})^2$ (item is truncated and does not match the bar grid).
   - Project integrity: The REAPER project `.rpp` references `%TEMP%\RealsLab\drag_export\drag_xxx.wav`. When `%TEMP%` is purged, the project suffers from missing audio files.
   - UI responsiveness: Decoding and offline rendering inside the UI bridge handler blocks the mouse down event for 20–300 ms.

---

## 3. Caveats

1. **External Plugins / External DAWs vs Native REAPER**:
   - In REAPER, dragging the original file (`Mechanism A`) allows REAPER's native SDK and `processPendingSyncPlayrates` to non-destructively handle tempo and pitch using Élastique Pro 3 / RubberBand, while permanently referencing the user's sample library.
   - If a sample is dropped into an external 3rd-party sampler plugin (e.g., Serum, Vital, Kontakt) or an external DAW (e.g., Ableton, FL Studio), the external app does not know about REAPER SDK or pending playrates. It will receive the raw file unless pre-baked (`Mechanism B`).
2. **Deterministic Cache & Disk Cleanup**:
   - `DragExporter` has a deterministic FNV-1a cache with mtime checks and a cleanup pruning method (`cleanupTempFiles`). However, relying on temporary files for native DAW tracks is inherently unsafe compared to native original file references.
3. **Tests Dependency**:
   - `TestSuite_BridgeUI.cpp` has a unit test `F16_AutoRenderTemp_BeginDragWithSync` that currently asserts `dragged != samplePath`. When switching `browser.beginDrag` to Mechanism A, that assertion in the test suite must be updated to expect the original sample path.

---

## 4. Conclusion

1. **Root Cause**: The Double-DSP defect is caused by the combination of offline SoundTouch rendering in `DragExporter::exportTempWav` and subsequent REAPER take property assignment (`D_PLAYRATE`, `D_PITCH`, `D_LENGTH`) in `processPendingSyncPlayrates()`.
2. **Mechanism A (Recommended & Standard for REAPER Native Extension)**:
   - `browser.beginDrag` should pass the **original sample path `p`** to `queueSyncPlayrate(p, playrate, pitchShift)` and `beginDrag(p)`.
   - `DragExporter::exportTempWav` should **not** be called during `browser.beginDrag`.
   - REAPER native Élastique engine performs real-time, non-destructive, professional time-stretch and pitch shift.
   - Zero drag start lag (0 ms), 100% exact grid bar alignment, and permanent project file referencing.
3. **Mechanism B Safeguard**:
   - In `processPendingSyncPlayrates()`, if any media item path contains `drag_` or `drag_export`, ensure `D_PLAYRATE = 1.0` and `D_PITCH = 0.0` to guarantee no double-DSP occurs even if a baked WAV is dragged.

---

## 5. Verification Method

1. **Static / Code Verification**:
   - Inspect `bridge/src/Bridge.cpp:1433–1474` and verify that `browser.beginDrag` dispatches `p` to `m_actions->beginDrag(p)` and `m_actions->queueSyncPlayrate(p, playrate, pitchShift)`.
   - Inspect `extension/src/reaper_plugin.cpp:139–159` to verify take property updates.
2. **Test Suite Verification**:
   - Run: `ctest --preset windows -C Debug`
   - Run E2E test binary: `.\build\windows\tests\Debug\reals_tests.exe`
3. **DAW Interactive Test**:
   - In REAPER (140 BPM), drag a 120 BPM sample with Sync ON (+2 semitones).
   - Check REAPER Media Item:
     - `Take Playrate` = `1.1667`
     - `Pitch Shift` = `+2.00`
     - `Source File` = `C:\path\to\original_sample.wav`
     - `Item Length` = Exactly 2 bars / 4 bars on REAPER grid.
