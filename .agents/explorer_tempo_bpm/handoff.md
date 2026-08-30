# Handoff Report — Explorer 2 (BPM & Time-Stretching Math Investigator)

## 1. Observation

### 1.1 SoundTouch Ratio Calculations & API Mapping
- **File**: `core/src/audio/SoundTouchProcessor.cpp:85-90`
  ```cpp
  void SoundTouchProcessor::setTimeRatio(const float ratio) {
      const float clamped = std::clamp(ratio, 0.1f, 10.0f);
      m_impl->timeRatio = clamped;
      m_impl->st.setTempo(clamped);
  }
  ```
- **File**: `bridge/src/Bridge.cpp:846-854` (inside `audio.play`)
  ```cpp
  if (projectBpm > 30.0 && sampleBpm > 30.0) {
      const float ratio = std::clamp(static_cast<float>(projectBpm / sampleBpm), 0.25f, 4.0f);
      eng.setTimeRatio(ratio);
      {
          const std::lock_guard lock(m_impl->syncMutex);
          m_impl->syncRatio = ratio;
          m_impl->syncSampleBpm = sampleBpm;
      }
  }
  ```
- **File**: `bridge/src/Bridge.cpp:1060-1070` (inside `audio.setSyncBpm`)
  ```cpp
  if (enabled) {
      if (projectBpm > 0.0f && sampleBpm > 0.0f) {
          ratio = projectBpm / sampleBpm;
      } else if (args.contains("ratio")) {
          ratio = args.value("ratio", 1.0f);
      } else {
          if (projectBpm > 0.0f) ratio = projectBpm / 120.0f;
      }
  }
  eng.setTimeRatio(ratio);
  ```

### 1.2 SoundTouch Parameter Behavior (`setTempo` vs `setRate` vs `setPitch`)
- SoundTouch C++ Library Specification:
  - `soundtouch::SoundTouch::setTempo(float newTempo)`: Adjusts playback speed without affecting pitch (WSOLA time-domain overlap-add). Value `1.0` = 100% (nominal speed), `1.2` = 120% speed (playback is 20% faster, output duration $= \text{InputDuration} / 1.2$).
  - `soundtouch::SoundTouch::setRate(float newRate)`: Adjusts speed and pitch simultaneously (pure sample rate conversion / tape-style speedup).
  - `soundtouch::SoundTouch::setPitchSemiTones(float semitones)`: Adjusts pitch without affecting tempo (resampling + inverse WSOLA stretch).
  - `SoundTouchProcessor.cpp` never calls `st.setRate()`. `setTimeRatio` calls `st.setTempo(clamped)` and `setPitchSemitones` calls `st.setPitchSemiTones(clamped)`.

### 1.3 REAPER BPM Acquisition Defect
- **File**: `extension/src/reaper_plugin.cpp:564-566`
  ```cpp
  double projectTempo() const override {
      return Master_GetTempo ? Master_GetTempo() : 0.0;
  }
  ```
- **File**: `extension/src/reaper_plugin.cpp:368-372` (in `ReaperOnAudioBuffer`)
  ```cpp
  double bpm = 120.0;
  if (TimeMap_GetDividedBpmAtTime) {
      bpm = TimeMap_GetDividedBpmAtTime(playPos);
  } else if (Master_GetTempo) {
      bpm = Master_GetTempo();
  }
  ```
- Observation: When a project has tempo change markers (e.g. initial project tempo is 120 BPM, but current playhead section marker is 80 BPM or 90 BPM), `Master_GetTempo()` returns the static initial tempo `120.0` instead of the local tempo at `playPos`. Meanwhile, `ReaperOnAudioBuffer` records the true tempo `80.0` into `g_liveTransport.bpm`. When Bridge calls `m_actions->projectTempo()`, it gets `120.0`, computing `ratio = 120 / sampleBpm` instead of `80 / sampleBpm`.

### 1.4 Host Sample Rate Handling and Decoder Resampling
- **File**: `extension/src/reaper_plugin.cpp:342-344`
  ```cpp
  if (srate > 0.0) {
      reals::audio::Engine::instance().setTargetSampleRate(static_cast<int>(srate));
  }
  ```
- **File**: `core/src/audio/Engine.cpp:448-454`
  ```cpp
  const int targetSr = (m_impl->targetSampleRate > 0) ? m_impl->targetSampleRate : m_impl->track.sampleRate;
  const int channels = (m_impl->track.channels > 0) ? m_impl->track.channels : 2;

  ma_decoder_config decConfig = ma_decoder_config_init(
      ma_format_f32,
      static_cast<ma_uint32>(channels),
      static_cast<ma_uint32>(targetSr));
  ```
- **File**: `core/src/audio/Engine.cpp:964-988` (`Engine::renderFrames`)
  ```cpp
  void Engine::renderFrames(float* outL, float* outR, size_t frames) {
      ...
      ma_uint64 framesRead = 0;
      dsp_on_read(&m_impl->dspSource.base, interleaved.data(), frames, &framesRead);
      ...
  }
  ```
- Observation: If `Engine::playFile` is called before `setTargetSampleRate` has received the host sample rate from REAPER (i.e. `targetSampleRate == 0`), `targetSr` defaults to the file's sample rate (e.g. 44.1kHz). When `ReaperOnAudioBuffer` subsequently requests 48kHz frames, `renderFrames` outputs 44.1kHz samples directly into the 48kHz audio buffer.

---

## 2. Logic Chain

### 2.1 Mathematical Proof of Time-Stretching Ratio
1. Let the original sample have tempo $B_{\text{sample}}$ (beats per minute) and duration $D_{\text{sample}} = \frac{N \times 60}{B_{\text{sample}}}$ seconds for an $N$-beat loop.
2. In a DAW project running at tempo $B_{\text{project}}$, the desired playback duration for $N$ beats is $D_{\text{target}} = \frac{N \times 60}{B_{\text{project}}}$.
3. The required playback speed factor $R$ satisfies:
   $$D_{\text{target}} = \frac{D_{\text{sample}}}{R} \implies R = \frac{D_{\text{sample}}}{D_{\text{target}}} = \frac{(N \times 60) / B_{\text{sample}}}{(N \times 60) / B_{\text{project}}} = \frac{B_{\text{project}}}{B_{\text{sample}}}$$
4. SoundTouch's `st.setTempo(R)` directly scales playback speed by factor $R$ while preserving pitch through WSOLA:
   $$\text{Speed}_{\text{new}} = R \times \text{Speed}_{\text{orig}} = \frac{B_{\text{project}}}{B_{\text{sample}}} \times B_{\text{sample}} = B_{\text{project}}$$
5. Therefore, `timeRatio = projectBpm / sampleBpm` is mathematically exact.

### 2.2 Identification of Root Causes for "Preview Plays Faster than DAW Tempo"

#### Root Cause A: Host Sample Rate Conflation (44.1kHz vs 48kHz / 96kHz)
- **Mechanism**:
  - Sample file: 44.1kHz.
  - REAPER ASIO Device: 48kHz or 96kHz.
  - If `Engine::instance().targetSampleRate()` is 0 at the moment `Engine::playFile()` decodes the file, miniaudio decodes at 44.1kHz.
  - In `ReaperOnAudioBuffer`, REAPER consumes `len` frames at 48kHz clock speed.
  - 44,100 samples of the file are consumed in $\frac{44100}{48000} = 0.91875\text{ seconds}$ instead of 1.0 second.
  - **Speedup Factor**: $\frac{48000}{44100} = 1.0884\times$ (audio plays **8.84% faster** and $+1.47$ semitones sharper).
  - At 96kHz host sample rate: $\frac{96000}{44100} = 2.1769\times$ (audio plays **2.18x faster** and $+13.5$ semitones sharper).

#### Root Cause B: Static `Master_GetTempo()` vs Dynamic `TimeMap_GetDividedBpmAtTime(playPos)`
- **Mechanism**:
  - In `extension/src/reaper_plugin.cpp:564`, `ExtHostActions::projectTempo()` calls `Master_GetTempo()`.
  - `Master_GetTempo()` in REAPER SDK returns the project's base tempo at start (e.g. 120 BPM).
  - If the active project timeline section has a tempo marker (e.g. 80 BPM or 90 BPM), `Master_GetTempo()` continues returning `120.0`.
  - Bridge calculates `ratio = 120.0 / sampleBpm`.
  - When the sample is 80 BPM, Bridge computes `ratio = 120 / 80 = 1.5`, whereas the true ratio should be `80 / 80 = 1.0`.
  - **Speedup Factor**: $\frac{120}{80} = 1.50\times$ (audio plays **50% faster than DAW playhead**).

#### Root Cause C: Half-Time / Double-Time Detection Errors
- **Mechanism**:
  - In `core/src/ai/TempoDetector.cpp`, rhythm autocorrelation or CNN onset peak-picking may detect half-tempo (e.g. detecting 70 BPM for a 140 BPM drum track, or 87 BPM for a 174 BPM DnB break).
  - When DAW tempo is 140 BPM and detected sample BPM is 70 BPM:
    $\text{ratio} = \frac{140}{70} = 2.0\times$.
  - **Speedup Factor**: The preview plays at $2.0\times$ speed (double tempo, 280 BPM equivalent).

#### Root Cause D: Drag-and-Drop Double Time-Stretching (Take Property Interaction)
- **Mechanism**:
  - When dragging a file into REAPER: if an offline processor pre-renders a temporary WAV stretched by factor $R = \frac{B_{\text{project}}}{B_{\text{sample}}}$ (Mechanism B), and REAPER then assigns take property `D_PLAYRATE = R`, REAPER stretches the already-stretched file a second time.
  - **Speedup Factor**: $R^2 = (\frac{B_{\text{project}}}{B_{\text{sample}}})^2$. For $120 \to 140$ BPM, $1.167^2 = 1.362\times$.

---

## 3. Caveats
1. **Time-Varying Tempo Ramps**: If a project has continuous tempo automation (smooth tempo ramps rather than step markers), the preview ratio is locked to the tempo at preview trigger time; real-time continuous stretching during preview would require per-block `setTimeRatio` modulation inside `ReaperOnAudioBuffer`.
2. **Audio Device Initialization Timing**: On Windows, when REAPER starts, `Audio_RegHardwareHook` is called during extension load, but `srate` is first populated when REAPER's audio hardware stream starts.

---

## 4. Conclusion & Recommended Fixes

### 4.1 Summary of Findings
1. The mathematical ratio $\text{timeRatio} = \frac{\text{projectBpm}}{\text{sampleBpm}}$ with SoundTouch `st.setTempo(timeRatio)` is **100% correct** for pitch-neutral tempo synchronization.
2. `SoundTouchProcessor` properly separates tempo (`setTempo`) from pitch (`setPitchSemiTones`) and does not touch `setRate`.
3. The discrepancy where preview plays faster than DAW is caused by:
   - Sample rate mismatch between source audio files (44.1kHz) and host ASIO rate (48kHz/96kHz) when `targetSampleRate` is uninitialized.
   - `ExtHostActions::projectTempo()` calling static `Master_GetTempo()` instead of dynamic `g_liveTransport.bpm` / `TimeMap_GetDividedBpmAtTime(playPos)`.

### 4.2 Proposed Code Fixes

#### Fix 1: Dynamic Project BPM in `extension/src/reaper_plugin.cpp`
```cpp
// Target: extension/src/reaper_plugin.cpp line 564
// Before:
double projectTempo() const override {
    return Master_GetTempo ? Master_GetTempo() : 0.0;
}

// After:
double projectTempo() const override {
    const double liveBpm = g_liveTransport.bpm.load(std::memory_order_relaxed);
    if (liveBpm > 30.0) return liveBpm;

    double playPos = 0.0;
    if (GetPlayState && (GetPlayState() & 1)) {
        if (GetPlayPosition2Ex) {
            ReaProject* proj = EnumProjects ? EnumProjects(-1, nullptr, 0) : nullptr;
            playPos = GetPlayPosition2Ex(proj);
        } else if (GetPlayPosition2) {
            playPos = GetPlayPosition2();
        } else if (GetPlayPosition) {
            playPos = GetPlayPosition();
        }
    } else if (GetCursorPosition) {
        playPos = GetCursorPosition();
    }

    if (TimeMap_GetDividedBpmAtTime) {
        const double bpm = TimeMap_GetDividedBpmAtTime(playPos);
        if (bpm > 30.0) return bpm;
    }
    return Master_GetTempo ? Master_GetTempo() : 120.0;
}
```

#### Fix 2: Early Host Sample Rate Acquisition in `extension/src/reaper_plugin.cpp`
```cpp
// In extension initialization or ExtHostActions constructor:
if (GetAudioDeviceInfo) {
    char srateBuf[64] = {0};
    GetAudioDeviceInfo("SRATE", srateBuf, sizeof(srateBuf));
    const int devSr = std::atoi(srateBuf);
    if (devSr > 0) {
        reals::audio::Engine::instance().setTargetSampleRate(devSr);
    }
}
```

#### Fix 3: Robust Project BPM Fallback in `bridge/src/Bridge.cpp`
```cpp
// In Bridge.cpp:838-844 (inside audio.play)
double projectBpm = 0.0;
if (m_actions) {
    const auto transport = m_actions->hostTransport();
    if (transport.bpm > 30.0) projectBpm = transport.bpm;
    else projectBpm = m_actions->projectTempo();
}
```

---

## 5. Verification Method

### 5.1 Independent Test Verification Commands
1. Build test suite:
   ```powershell
   cmake --build --preset windows
   ```
2. Run Audio DSP, SoundTouch, and PhaseSync diagnostic suites:
   ```powershell
   ctest --preset windows -R "(AudioDSP|SoundTouchCore|PhaseSyncDiagnostics)" --output-on-failure
   ```
3. Run full test suite:
   ```powershell
   ctest --preset windows
   ```

### 5.2 Files to Inspect
- `core/src/audio/SoundTouchProcessor.cpp` (lines 85–90)
- `core/src/audio/Engine.cpp` (lines 448–505, 964–988)
- `bridge/src/Bridge.cpp` (lines 834–858, 1060–1075)
- `extension/src/reaper_plugin.cpp` (lines 339–398, 564–610)
- `tests/suites/TestSuite_PhaseSyncDiagnostics.cpp` (lines 75–260)
