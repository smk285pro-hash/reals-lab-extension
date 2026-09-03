#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include "ITimeStretchProcessor.h"

namespace reals::audio {

/**
 * @brief Real-time DSP processor for independent time-stretching and pitch-shifting.
 * 
 * Built on SoundTouch DSP engine. Conforms to C++20, zero-warning standards.
 * Supports:
 *  - DAW BPM Sync (pitch-neutral time stretching: setTimeRatio)
 *  - Real-time Chromatic Pitch Shifting (tempo-neutral pitch shifting: setPitchSemitones, range [-12, +12])
 *  - Original key reset (resetPitch / setOriginalKey)
 *  - Low-latency real-time preview mode (< 30ms latency)
 */
class SoundTouchProcessor : public ITimeStretchProcessor {
public:
    explicit SoundTouchProcessor(int sampleRate = 44100, int channels = 2, bool lowLatency = false);
    ~SoundTouchProcessor() override;

    SoundTouchProcessor(const SoundTouchProcessor&) = delete;
    SoundTouchProcessor& operator=(const SoundTouchProcessor&) = delete;

    SoundTouchProcessor(SoundTouchProcessor&&) noexcept;
    SoundTouchProcessor& operator=(SoundTouchProcessor&&) noexcept;

    // --- Configuration ---
    void setSampleRate(int sampleRate) override;
    // SoundTouch operates at the input (native) sample rate.
    // Rate conversion is folded into the time ratio by the Engine.
    void setSampleRates(int inSampleRate, int outSampleRate) override;
    [[nodiscard]] int getSampleRate() const override;

    void setChannels(int channels) override;
    [[nodiscard]] int getChannels() const override;

    void setLowLatencyMode(bool lowLatency);
    [[nodiscard]] bool isLowLatencyMode() const;

    // --- Time-Stretching (DAW BPM Sync: Changes speed without changing pitch) ---
    // ratio > 1.0 => faster playback (shorter duration); ratio < 1.0 => slower playback.
    void setTimeRatio(float ratio) override;
    [[nodiscard]] float getTimeRatio() const override;

    // --- Pitch-Shifting (Real-time Key Transposition: Changes pitch without changing duration) ---
    // semitones in range [-12.0, +12.0]. 0.0 = original pitch.
    void setPitchSemitones(float semitones) override;
    [[nodiscard]] float getPitchSemitones() const override;

    void setPitchOctaves(float octaves);
    [[nodiscard]] float getPitchOctaves() const;

    void setPitchRatio(float ratio);
    [[nodiscard]] float getPitchRatio() const;

    // Reset pitch back to original 0.0 semitones (1.0 ratio)
    void resetPitch();
    void setOriginalKey();

    // --- Streaming Audio IO ---
    // Feeds interleaved float PCM samples into the processor
    void putSamples(const float* interleavedSamples, size_t numFrames) override;

    // Receives processed interleaved float PCM samples from the processor.
    // Returns the number of frames actually received (<= maxFrames).
    [[nodiscard]] size_t receiveSamples(float* outInterleavedSamples, size_t maxFrames) override;

    // Number of processed frames available to read immediately
    [[nodiscard]] size_t numSamplesAvailable() const override;

    // Flushes internal buffers to drain any remaining samples
    void flush() override;

    // Clears all internal buffers and resets state
    void clear() override;

    // Current processing latency in frames and milliseconds
    [[nodiscard]] int latencyFrames() const override;
    [[nodiscard]] float latencyMilliseconds() const;

    // Convenience batch processor for full buffers
    [[nodiscard]] std::vector<float> processBuffer(const float* interleavedSamples, size_t numFrames);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace reals::audio
