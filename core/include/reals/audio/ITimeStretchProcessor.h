#pragma once

#include <cstddef>

namespace reals::audio {

/**
 * @brief Abstract audio processor interface for time-stretching and pitch-shifting.
 * 
 * Allows core::Engine to use either SoundTouch (standalone/fallback)
 * or REAPER's native élastique 3 Pro (IReaperPitchShift in extension shell)
 * without any compile-time coupling between core and REAPER SDK.
 */
class ITimeStretchProcessor {
public:
    virtual ~ITimeStretchProcessor() = default;

    virtual void setSampleRate(int sampleRate) = 0;
    virtual void setSampleRates(int inSampleRate, int outSampleRate) { (void)inSampleRate; setSampleRate(outSampleRate); }
    [[nodiscard]] virtual int getSampleRate() const = 0;

    virtual void setChannels(int channels) = 0;
    [[nodiscard]] virtual int getChannels() const = 0;

    virtual void setTimeRatio(float ratio) = 0;
    [[nodiscard]] virtual float getTimeRatio() const = 0;

    virtual void setPitchSemitones(float semitones) = 0;
    [[nodiscard]] virtual float getPitchSemitones() const = 0;

    virtual void putSamples(const float* interleavedSamples, size_t numFrames) = 0;
    [[nodiscard]] virtual size_t receiveSamples(float* outInterleavedSamples, size_t maxFrames) = 0;

    [[nodiscard]] virtual size_t numSamplesAvailable() const = 0;
    virtual void clear() = 0;
    virtual void flush() = 0;

    [[nodiscard]] virtual int latencyFrames() const = 0;
};

} // namespace reals::audio
