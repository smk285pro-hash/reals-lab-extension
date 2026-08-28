#pragma once

// Audio playback engine built on miniaudio. Owns the audio thread.
// Rules (AGENTS.md): no locks/allocations inside realtime callbacks.
// Metering is data-driven: the played file is decoded once into a peak
// envelope used for the waveform and level meter.
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
    bool init();
    void shutdown();
    [[nodiscard]] bool isReady() const;

    // Playback of a decoded file with optional phase/start fraction [0.0..1.0). Returns false if the file cannot be decoded.
    bool playFile(const std::string& path, bool loop = false, double startFraction = 0.0);
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
    struct Impl;
    Impl* m_impl = nullptr; // PIMPL: keeps miniaudio out of public headers
};

} // namespace reals::audio
