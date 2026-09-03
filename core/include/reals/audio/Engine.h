#pragma once

// Audio playback engine built on miniaudio. Owns the audio thread.
// Rules (AGENTS.md): no locks/allocations inside realtime callbacks.
// Metering is data-driven: the played file is decoded once into a peak
// envelope used for the waveform and level meter.
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace reals::audio {

struct TrackInfo {
    std::string path;
    double durationSeconds = 0.0;
    double totalFrames = 0.0;
    int sampleRate = 0;
    int channels = 0;
};

struct LevelState {
    float peak = 0.0f;   // 0..1 linear, envelope at the playhead
    float rms = 0.0f;    // 0..1 linear, averaged around the playhead
};

class Engine {
public:
    static Engine& instance();

    // Initialize/teardown the output device (default device).
    bool init(bool useDevice = true);
    void shutdown();
    [[nodiscard]] bool isReady() const;
    void renderFrames(float* outL, float* outR, size_t frames);
    void setTargetSampleRate(int sampleRate);
    [[nodiscard]] int targetSampleRate() const;

    // Phase anchor: invoked by playFile AFTER the file is fully decoded and
    // the DSP chain is configured but BEFORE the start position is applied
    // and playback begins. Receives the preset startFraction and returns the
    // fraction to actually use. This lets the caller re-sample the DAW
    // transport at the last possible moment, eliminating decode-time phase
    // lag (playhead phase sync fix).
    using PhaseAnchor = std::function<double(double presetFraction)>;

    // Playback of a decoded file with optional phase/start fraction [0.0..1.0) and optional nominal loop frames boundary. Returns false if the file cannot be decoded.
    bool playFile(const std::string& path, bool loop = false, double startFraction = 0.0,
                  const PhaseAnchor& phaseAnchor = nullptr, uint64_t nominalLoopFrames = 0);
    void setLoopBoundaryFrames(uint64_t frames);
    [[nodiscard]] uint64_t loopBoundaryFrames() const;
    void stop();
    // Play the file; if it is already playing, stop it instead.
    void toggle(const std::string& path, bool loop);
    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] bool isPlayingPath(const std::string& path) const;

    // Header-only probe: duration / sample rate / channels, no playback, no envelope.
    [[nodiscard]] static TrackInfo probeFile(const std::string& path);

    // Detect BPM of file via TempoDetector (local, no API). Returns 0 if fails.
    [[nodiscard]] static float detectBpm(const std::string& path);
    // Detect musical key of file via KeyDetector. Returns empty string if fails.
    [[nodiscard]] static std::string detectKey(const std::string& path);

    // Seek to a fraction of duration (0..1). No-op if nothing is loaded.
    void seekFraction(double fraction);

    // Latency (seconds) between the moment content is fed into the playback
    // pipeline and the moment it becomes audible: the SoundTouch initial
    // latency, active in DSP (time-stretch / pitch) mode. The bypass
    // fast-path reports 0. Callers advance the phase anchor position by this
    // amount so the audible output lands exactly on the DAW grid.
    [[nodiscard]] double pipelineLatencySeconds() const;
    [[nodiscard]] double deviceBufferLatencySeconds() const;

    // DAW BPM Sync: Time-stretch playback without affecting pitch (1.0 = original tempo).
    void setTimeRatio(float ratio);
    [[nodiscard]] float getTimeRatio() const;

    // Real-time Pitch Shifting: Transpose pitch in semitones [-12.0, +12.0] without changing tempo (<30ms latency).
    void setPitchSemitones(float semitones);
    [[nodiscard]] float getPitchSemitones() const;

    // Reset pitch back to original key (0.0 semitones).
    void resetPitch();
    void setOriginalKey();

    void setVolume(float linear); // 0..1
    [[nodiscard]] float volume() const;

    // Loop the current/next playback. Applies immediately to a loaded sound.
    void setLoop(bool loop);
    [[nodiscard]] bool loop() const;

    // Inject a custom time-stretch processor (e.g. REAPER's native élastique 3 Pro in extension shell)
    void setTimeStretchProcessor(std::shared_ptr<class ITimeStretchProcessor> proc);

    // Level of the currently playing file (poll from the UI thread).
    [[nodiscard]] LevelState level() const;

    [[nodiscard]] const TrackInfo& currentTrack() const;
    // Normalized peak envelope (0..1) of the current track, ~160 buckets.
    [[nodiscard]] const std::vector<float>& envelope() const;
    // Compute peak envelope (~160 buckets) without blocking playback. Can be run on any thread.
    [[nodiscard]] static std::vector<float> computeEnvelope(const std::string& path);
    // Asynchronously assign the computed envelope to the active track.
    void setEnvelope(const std::string& path, const std::vector<float>& env);
    // Playback position as a fraction of duration (0..1).
    [[nodiscard]] double positionFraction() const;

private:
    Engine() = default;
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    struct Impl;
    std::unique_ptr<Impl> m_impl; // PIMPL: keeps miniaudio out of public headers (MAJ-07)
};

} // namespace reals::audio
