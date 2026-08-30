# Audio Sample Rate & Pipeline Investigation Report

## 1. Observation

Direct code observations across `core/src/audio/Engine.cpp`, `core/src/audio/SoundTouchProcessor.cpp`, `bridge/src/Bridge.cpp`, and `extension/src/reaper_plugin.cpp`:

### O1. `targetSampleRate` is Uninitialized (0) at Extension Startup and Standalone Mode
- **File**: `core/src/audio/Engine.cpp:356, 364-371`
  ```cpp
  356: int targetSampleRate = 0;
  ...
  364: void Engine::setTargetSampleRate(const int sampleRate) {
  365:     if (!m_impl) m_impl = std::make_unique<Impl>();
  366:     m_impl->targetSampleRate = sampleRate;
  367: }
  ```
- **File**: `core/src/audio/Engine.cpp:448`
  ```cpp
  448: const int targetSr = (m_impl->targetSampleRate > 0) ? m_impl->targetSampleRate : m_impl->track.sampleRate;
  ```
- **File**: `extension/src/reaper_plugin.cpp:339-345`
  ```cpp
  339: static void ReaperOnAudioBuffer(bool isPost, int len, double srate, struct audio_hook_register_t* reg) {
  340:     if (len <= 0 || !reg) return;
  341: 
  342:     if (srate > 0.0) {
  343:         reals::audio::Engine::instance().setTargetSampleRate(static_cast<int>(srate));
  344:     }
  ```
- **File**: `extension/src/reaper_plugin.cpp:1258-1280` (`REAPER_PLUGIN_ENTRYPOINT`)
  `Audio_RegHardwareHook` is registered, but `REAPER_PLUGIN_ENTRYPOINT` never queries `GetAudioDeviceInfo("SRATE", ...)` on plugin startup to seed `targetSampleRate`.
- **File**: `core/src/audio/Engine.cpp:378-395` (`Engine::init`)
  When `useDevice == true` (Standalone app), `ma_engine_init(nullptr, &m_impl->engine)` is called, but `m_impl->targetSampleRate` is left as `0` instead of querying `ma_engine_get_sample_rate(&m_impl->engine)`.

### O2. Frame Dimension Inconsistency Between `track.totalFrames` (File Rate) and `dspSource.totalFrames` (Host Rate)
- **File**: `core/src/audio/Engine.cpp:438, 485-493, 533-543`
  ```cpp
  438: m_impl->track = probeFile(path); // Populates track.sampleRate (e.g. 44100) and track.totalFrames (e.g. 352800 for an 8s loop)
  ...
  485: m_impl->dspSource.pcmData = std::move(tempPcm); // Resampled to targetSr (e.g. 48000) -> 384000 frames
  ...
  492: m_impl->dspSource.totalFrames.store(static_cast<ma_uint64>(m_impl->dspSource.pcmData.size() / channels)); // 384000 frames
  ...
  533: const ma_uint64 totalF = static_cast<ma_uint64>(m_impl->track.totalFrames); // 352800 (from 44.1k probe!)
  534: const ma_uint64 refFrames = (nominalLoopFrames > 0 && nominalLoopFrames <= totalF)
  535:     ? nominalLoopFrames
  536:     : totalF;
  537: 
  538: const ma_uint64 startFrame = (refFrames > 0 && clampedFraction > 0.0)
  539:     ? static_cast<ma_uint64>(clampedFraction * refFrames)
  540:     : 0;
  541: 
  542: m_impl->dspSource.cursorFrames.store(startFrame, std::memory_order_relaxed);
  ```
  When `nominalLoopFrames` is computed at 48kHz (384000 frames), `nominalLoopFrames <= totalF` (384000 <= 352800) is **FALSE**. `refFrames` falls back to `totalF = 352800`.
  `startFrame` is computed as `clampedFraction * 352800` into a 384000-frame PCM buffer, shifting start phase by `(1 - 44.1/48.0) * fraction = 8.125%`.

### O3. UI Waveform and Level Meter Running Ahead of Audio Playback
- **File**: `core/src/audio/Engine.cpp:917-920, 950-960`
  ```cpp
  917: const double frac = (!m_impl->soundLoaded || m_impl->track.totalFrames <= 0) ? 0.0
  918:     : static_cast<double>(m_impl->dspSource.cursorFrames.load()) / m_impl->track.totalFrames;
  ...
  950: double Engine::positionFraction() const {
  ...
  955:     const ma_uint64 bound = m_impl->dspSource.loopBoundaryFrames.load();
  956:     const double denom = (bound > 0 && bound <= static_cast<ma_uint64>(m_impl->track.totalFrames))
  957:         ? static_cast<double>(bound)
  958:         : m_impl->track.totalFrames;
  959:     return denom > 0.0 ? (static_cast<double>(m_impl->dspSource.cursorFrames.load()) / denom) : 0.0;
  960: }
  ```
  `cursorFrames` counts frames up to `dspSource.totalFrames` (480000), but `denom` uses `m_impl->track.totalFrames` (441000). At 9.1875s of a 10s file, `positionFraction()` reaches 1.0 (100%), and then exceeds 1.0.

### O4. Dynamic `audio.setSyncBpm` Calculates `nominalLoopFrames` Using Native File Rate
- **File**: `bridge/src/Bridge.cpp:1111-1116`
  ```cpp
  1111: double beatInLoop = std::fmod(transport.fullBeats, resolvedBeats);
  1112: if (beatInLoop < 0.0) beatInLoop += resolvedBeats;
  1113: const double syncFrac = std::clamp(beatInLoop / resolvedBeats, 0.0, 0.999);
  1114: const double nominalLoopSec = (resolvedBeats * 60.0) / sampleBpm;
  1115: if (trk.sampleRate > 0) {
  1116:     eng.setLoopBoundaryFrames(static_cast<uint64_t>(nominalLoopSec * trk.sampleRate));
  1117: }
  ```
  `trk.sampleRate` is 44100 Hz. For an 8.0s loop, `loopBoundaryFrames` is set to `8.0 * 44100 = 352800` frames.
  In `dsp_on_read` (`Engine.cpp:140-143`), `effectiveLoopFrames` becomes 352800.
  When rendering at 48000 Hz, `cursorFrames` wraps around at `352800 / 48000 = 7.35 seconds` instead of 8.0 seconds, cutting off 0.65 seconds early every loop iteration.

### O5. SoundTouch Initialization and Low-Latency Setting
- **File**: `core/src/audio/Engine.cpp:50`
  `SoundTouchProcessor processor{44100, 2, true};`
- **File**: `core/src/audio/Engine.cpp:496`
  `m_impl->dspSource.processor.setSampleRate(targetSr);`
- **File**: `core/src/audio/SoundTouchProcessor.cpp:17-35, 55-60`
  When `setSampleRate(targetSr)` is called with `targetSr > 0`, SoundTouch updates its internal sample rate and recalculates the sequence, seek, and overlap frame windows correctly according to `targetSr`.

---

## 2. Logic Chain

```
[Observation O1: targetSampleRate defaults to 0 and is not queried from GetAudioDeviceInfo("SRATE") on plugin load]
   │
   ▼
[Step 1]: If user triggers preview before the first ReaperOnAudioBuffer hook runs, or in any zero-rate scenario, targetSr falls back to track.sampleRate (44100 Hz).
   │
   ▼
[Step 2]: Audio is decoded into pcmData at 44100 Hz without resampling.
   │
   ▼
[Step 3]: ReaperOnAudioBuffer runs at 48000 Hz and pulls 48000 samples per second from pcmData via renderFrames.
   │
   ▼
[Inference 1]: Outputting 44100 Hz PCM at 48000 Hz hardware rate causes 48000/44100 = 1.0884x playback speed (+8.84% tempo acceleration) and +1.47 semitone pitch shift.

[Observation O2 & O3: track.totalFrames is native file rate (44.1k), but pcmData is host rate (48k)]
   │
   ▼
[Step 4]: In playFile (Engine.cpp:534), nominalLoopFrames (384000) > totalF (352800). Condition nominalLoopFrames <= totalF fails!
   │
   ▼
[Step 5]: refFrames drops back to totalF (352800). startFrame = clampedFraction * 352800 is applied to a 384000-frame buffer.
   │
   ▼
[Inference 2]: Phase offset of (1 - 44.1/48.0) * fraction (over 650ms at startFraction=1.0) is introduced on every playFile call.
   │
   ▼
[Step 6]: positionFraction() divides cursorFrames (up to 480k) by track.totalFrames (441k), causing the UI cursor to run 8.84% faster than the audio.

[Observation O4: Bridge.cpp:1114 uses trk.sampleRate instead of targetSampleRate during setSyncBpm]
   │
   ▼
[Step 7]: setLoopBoundaryFrames is given nominalLoopSec * 44100 = 352800 frames instead of 384000 frames.
   │
   ▼
[Step 8]: In dsp_on_read, cursorFrames wraps around at 352800 frames. At 48000 Hz, 352800 frames = 7.35 seconds.
   │
   ▼
[Inference 3]: An 8-second 120 BPM 4-bar loop truncates at 7.35 seconds, advancing the loop cycle by 1.3 beats each repeat and causing audible tempo rushing against REAPER's transport grid.
```

---

## 3. Caveats

- **No Caveats regarding pipeline structure**: Every file in the audio rendering path (`reaper_plugin.cpp`, `Bridge.cpp`, `Engine.cpp`, `SoundTouchProcessor.cpp`, `DragExporter.cpp`) was fully reviewed.
- REAPER's `Audio_RegHardwareHook` is invoked on the realtime audio thread. All sample rate conversion inside `Engine` is performed during offline decoding (`ma_decoder_read_pcm_frames`) in `playFile`, ensuring 0 allocations and 0 file I/O on the realtime `ReaperOnAudioBuffer` callback.

---

## 4. Conclusion

The tempo acceleration and phase drift when previewing in REAPER have **three primary root causes**:

1. **Host Sample Rate Cold-Start / Fallback to 44.1kHz**: `targetSampleRate` starts at `0` and is only set once `ReaperOnAudioBuffer` fires. If uninitialized, a 44.1kHz sample is decoded as 44.1kHz and played back into a 48kHz ASIO buffer, accelerating playback by **+8.84% (and +1.47 semitones)**.
2. **Frame Metric Mismatch (`track.totalFrames` vs `dspSource.totalFrames`)**: `probeFile` returns native file frame count (e.g. 352800 frames at 44.1kHz), but `pcmData` contains resampled frames at host rate (e.g. 384000 frames at 48kHz). This causes `playFile` to reject `nominalLoopFrames`, calculate incorrect start frame indices, and causes the UI playhead in `positionFraction()` to race 8.84% ahead of the audio.
3. **Loop Boundary Frame Rate Mismatch in `Bridge.cpp:1114`**: `audio.setSyncBpm` calculates `nominalLoopFrames = nominalLoopSec * trk.sampleRate` (44.1k) instead of `targetSampleRate` (48k), causing the loop boundary to wrap at 7.35s instead of 8.0s.

### Recommended Fixes:
1. **Initialize Host Sample Rate Immediately on Plugin Load**:
   - In `reaper_plugin.cpp` (`REAPER_PLUGIN_ENTRYPOINT`) and `ExtHostActions::hostTransport()`:
     ```cpp
     char srateBuf[64] = {0};
     if (GetAudioDeviceInfo) {
         GetAudioDeviceInfo("SRATE", srateBuf, sizeof(srateBuf));
         int sr = std::atoi(srateBuf);
         if (sr > 0) {
             reals::audio::Engine::instance().setTargetSampleRate(sr);
         }
     }
     ```
2. **Synchronize `TrackInfo` and Engine Frame Calculations with `targetSr`**:
   - In `Engine.cpp:playFile`:
     ```cpp
     m_impl->track.sampleRate = targetSr;
     m_impl->track.totalFrames = static_cast<double>(m_impl->dspSource.totalFrames.load());
     ```
   - In `Engine.cpp` (`positionFraction()`, `level()`, `seekFraction()`, `playFile` start frame):
     Use `m_impl->dspSource.totalFrames.load()` as the reference total frames.
3. **Fix `Bridge.cpp:1114` to Use `targetSampleRate`**:
   - In `Bridge.cpp:1114` (`audio.setSyncBpm`):
     ```cpp
     const int effectiveSr = (eng.targetSampleRate() > 0) ? eng.targetSampleRate() : trk.sampleRate;
     if (effectiveSr > 0) {
         eng.setLoopBoundaryFrames(static_cast<uint64_t>(nominalLoopSec * effectiveSr));
     }
     ```
4. **Capture Device Sample Rate in Standalone Mode (`Engine::init`)**:
   - In `Engine.cpp:init(bool useDevice)`:
     ```cpp
     if (useDevice && m_impl->engineInited) {
         m_impl->targetSampleRate = static_cast<int>(ma_engine_get_sample_rate(&m_impl->engine));
     }
     ```

---

## 5. Verification Method

To independently verify these findings:

1. **Inspect Code Locations**:
   - `core/src/audio/Engine.cpp:448, 533-543, 917-920, 955-959`
   - `bridge/src/Bridge.cpp:907, 1114-1116`
   - `extension/src/reaper_plugin.cpp:342-344, 628-632, 1258-1280`
2. **Build and Run Unit Tests**:
   ```powershell
   cmake --build --preset windows
   ctest --preset windows -R PhaseSyncDiagnostics --output-on-failure
   ```
3. **Reproduce Sample Rate Mismatch Test**:
   - Create a test where `Engine::instance().setTargetSampleRate(48000)`.
   - Load a 44100 Hz test loop (e.g. 8.0 seconds = 352800 frames @ 44.1k).
   - Verify that:
     - `Engine::instance().loopBoundaryFrames()` equals `8.0 * 48000 = 384000` frames.
     - `Engine::instance().positionFraction()` at 4.0s is exactly `0.500`.
     - `Engine::instance().currentTrack().sampleRate` equals `48000`.
     - `Engine::instance().currentTrack().totalFrames` equals `384000.0`.
