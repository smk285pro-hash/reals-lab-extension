// SoundTouchProcessor production tests, ported from the former standalone
// test_soundtouch_processor binary (MIN-06) into the reals_tests suite.
// Coverage preserved: init/latency, time-stretch with pitch preservation,
// pitch shifting (+/-12 / +7 semitones), pitch reset, streaming chunk IO.
#include "../framework/TestRunner.h"

#include "reals/audio/SoundTouchProcessor.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<float> generateSine(float freqHz, int sampleRate, float durationSeconds, int channels = 1) {
    size_t numFrames = static_cast<size_t>(sampleRate * durationSeconds);
    std::vector<float> buffer(numFrames * static_cast<size_t>(channels));
    for (size_t i = 0; i < numFrames; ++i) {
        const float sample = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / static_cast<float>(sampleRate));
        for (int c = 0; c < channels; ++c)
            buffer[i * static_cast<size_t>(channels) + static_cast<size_t>(c)] = sample;
    }
    return buffer;
}

// Normalized autocorrelation with lowest-lag peak picking.
float estimateDominantFrequency(const std::vector<float>& samples, int sampleRate, int channels = 1) {
    if (samples.empty()) return 0.0f;
    const size_t numFrames = samples.size() / static_cast<size_t>(channels);
    if (numFrames < 512) return 0.0f;

    std::vector<float> mono(numFrames);
    for (size_t i = 0; i < numFrames; ++i)
        mono[i] = samples[i * static_cast<size_t>(channels)];

    const int minLag = sampleRate / 2000;
    int maxLag = sampleRate / 50;
    if (maxLag >= static_cast<int>(numFrames) / 2) maxLag = static_cast<int>(numFrames) / 2 - 1;
    if (maxLag <= minLag) return 0.0f;

    const size_t testWindow = numFrames / 2;
    std::vector<float> r(static_cast<size_t>(maxLag) + 1, 0.0f);
    float globalMax = -1.0f;
    for (int lag = minLag; lag <= maxLag; ++lag) {
        float num = 0.0f, d0 = 0.0f, d1 = 0.0f;
        for (size_t i = 0; i < testWindow; ++i) {
            const float x0 = mono[i];
            const float x1 = mono[i + static_cast<size_t>(lag)];
            num += x0 * x1;
            d0 += x0 * x0;
            d1 += x1 * x1;
        }
        const float den = std::sqrt(d0 * d1);
        r[static_cast<size_t>(lag)] = (den > 1e-9f) ? (num / den) : 0.0f;
        globalMax = std::max(globalMax, r[static_cast<size_t>(lag)]);
    }
    if (globalMax < 0.25f) return 0.0f;

    const float thresh = globalMax * 0.70f;
    int bestLag = -1;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (r[static_cast<size_t>(lag)] > r[static_cast<size_t>(lag - 1)] &&
            r[static_cast<size_t>(lag)] >= r[static_cast<size_t>(lag + 1)] &&
            r[static_cast<size_t>(lag)] >= thresh) {
            bestLag = lag;
            break;
        }
    }
    if (bestLag < 0) {
        float best = -1.0f;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            if (r[static_cast<size_t>(lag)] > best) {
                best = r[static_cast<size_t>(lag)];
                bestLag = lag;
            }
        }
    }
    if (bestLag <= 0) return 0.0f;

    float fracLag = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        const float alpha = r[static_cast<size_t>(bestLag - 1)];
        const float beta = r[static_cast<size_t>(bestLag)];
        const float gamma = r[static_cast<size_t>(bestLag + 1)];
        const float denom = 2.0f * (2.0f * beta - alpha - gamma);
        if (std::abs(denom) > 1e-6f)
            fracLag += (alpha - gamma) / denom;
    }
    return static_cast<float>(sampleRate) / fracLag;
}

} // namespace

TEST(SoundTouchCore, InitializationAndLatency) {
    reals::audio::SoundTouchProcessor proc(44100, 2, true);

    EXPECT_EQ(proc.getSampleRate(), 44100);
    EXPECT_EQ(proc.getChannels(), 2);
    EXPECT_TRUE(proc.isLowLatencyMode());
    EXPECT_LT(proc.latencyMilliseconds(), 30.0f); // real-time budget (SPEC)

    proc.setSampleRate(48000);
    EXPECT_EQ(proc.getSampleRate(), 48000);
    EXPECT_LT(proc.latencyMilliseconds(), 30.0f);

    proc.setChannels(1);
    EXPECT_EQ(proc.getChannels(), 1);
}

TEST(SoundTouchCore, TimeStretchCompressionPreservesPitch) {
    constexpr int sampleRate = 44100;
    constexpr float freqHz = 440.0f;
    const auto inputAudio = generateSine(freqHz, sampleRate, 1.0f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setTimeRatio(1.5f); // faster tempo: 120 BPM sample in a 180 BPM project
    EXPECT_NEAR(proc.getTimeRatio(), 1.5f, 0.001f);

    const auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
    const size_t outFrames = outputAudio.size() / 2;
    const float expectedFrames = static_cast<float>(inputFrames) / 1.5f;
    EXPECT_NEAR(static_cast<float>(outFrames), expectedFrames, expectedFrames * 0.15f);

    // Pitch must remain unaffected during time-stretch.
    EXPECT_NEAR(estimateDominantFrequency(outputAudio, sampleRate, 2), freqHz, 25.0f);
}

TEST(SoundTouchCore, TimeStretchExpansionPreservesPitch) {
    constexpr int sampleRate = 44100;
    constexpr float freqHz = 440.0f;
    const auto inputAudio = generateSine(freqHz, sampleRate, 1.0f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setTimeRatio(0.8f); // slower tempo: 150 BPM sample in a 120 BPM project
    EXPECT_NEAR(proc.getTimeRatio(), 0.8f, 0.001f);

    const auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
    const size_t outFrames = outputAudio.size() / 2;
    const float expectedFrames = static_cast<float>(inputFrames) / 0.8f;
    EXPECT_NEAR(static_cast<float>(outFrames), expectedFrames, expectedFrames * 0.15f);
    EXPECT_NEAR(estimateDominantFrequency(outputAudio, sampleRate, 2), freqHz, 25.0f);
}

TEST(SoundTouchCore, PitchShiftUpOctave) {
    constexpr int sampleRate = 44100;
    const auto inputAudio = generateSine(440.0f, sampleRate, 1.0f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setPitchSemitones(12.0f); // A4 -> A5 (880 Hz)
    EXPECT_NEAR(proc.getPitchSemitones(), 12.0f, 0.001f);
    EXPECT_NEAR(proc.getPitchRatio(), 2.0f, 0.001f);

    const auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
    const size_t outFrames = outputAudio.size() / 2;
    // Duration must be preserved during pitch shift.
    EXPECT_NEAR(static_cast<float>(outFrames), static_cast<float>(inputFrames),
                static_cast<float>(inputFrames) * 0.15f);
    EXPECT_NEAR(estimateDominantFrequency(outputAudio, sampleRate, 2), 880.0f, 40.0f);
}

TEST(SoundTouchCore, PitchShiftDownOctave) {
    constexpr int sampleRate = 44100;
    const auto inputAudio = generateSine(440.0f, sampleRate, 1.0f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setPitchSemitones(-12.0f); // A4 -> A3 (220 Hz)
    EXPECT_NEAR(proc.getPitchSemitones(), -12.0f, 0.001f);
    EXPECT_NEAR(proc.getPitchRatio(), 0.5f, 0.001f);

    const auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
    const size_t outFrames = outputAudio.size() / 2;
    EXPECT_NEAR(static_cast<float>(outFrames), static_cast<float>(inputFrames),
                static_cast<float>(inputFrames) * 0.15f);
    EXPECT_NEAR(estimateDominantFrequency(outputAudio, sampleRate, 2), 220.0f, 25.0f);
}

TEST(SoundTouchCore, PitchShiftPerfectFifth) {
    constexpr int sampleRate = 44100;
    const auto inputAudio = generateSine(440.0f, sampleRate, 1.0f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setPitchSemitones(7.0f); // A4 -> E5 (~659.25 Hz)
    const auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
    const float expectedFreq = 440.0f * std::pow(2.0f, 7.0f / 12.0f);
    EXPECT_NEAR(estimateDominantFrequency(outputAudio, sampleRate, 2), expectedFreq, 30.0f);
}

TEST(SoundTouchCore, ResetPitchAndOriginalKey) {
    reals::audio::SoundTouchProcessor proc(44100, 2, true);

    proc.setPitchSemitones(5.5f);
    EXPECT_NEAR(proc.getPitchSemitones(), 5.5f, 0.001f);
    proc.resetPitch();
    EXPECT_NEAR(proc.getPitchSemitones(), 0.0f, 0.001f);
    EXPECT_NEAR(proc.getPitchRatio(), 1.0f, 0.001f);

    proc.setPitchSemitones(-8.0f);
    proc.setOriginalKey();
    EXPECT_NEAR(proc.getPitchSemitones(), 0.0f, 0.001f);
    EXPECT_NEAR(proc.getPitchRatio(), 1.0f, 0.001f);
}

TEST(SoundTouchCore, StreamingChunkIO) {
    constexpr int sampleRate = 44100;
    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setPitchSemitones(3.0f);
    proc.setTimeRatio(1.2f);

    const auto inputAudio = generateSine(440.0f, sampleRate, 0.5f, 2);
    const size_t inputFrames = inputAudio.size() / 2;

    std::vector<float> streamedOutput;
    constexpr size_t kChunkSize = 256;
    size_t framesFed = 0;
    while (framesFed < inputFrames) {
        const size_t chunk = std::min(kChunkSize, inputFrames - framesFed);
        proc.putSamples(inputAudio.data() + framesFed * 2, chunk);
        framesFed += chunk;
        std::vector<float> outChunk(kChunkSize * 2);
        const size_t received = proc.receiveSamples(outChunk.data(), kChunkSize);
        if (received > 0)
            streamedOutput.insert(streamedOutput.end(), outChunk.begin(),
                                  outChunk.begin() + static_cast<std::ptrdiff_t>(received * 2));
    }

    proc.flush();
    while (true) {
        std::vector<float> outChunk(kChunkSize * 2);
        const size_t received = proc.receiveSamples(outChunk.data(), kChunkSize);
        if (received == 0) break;
        streamedOutput.insert(streamedOutput.end(), outChunk.begin(),
                              outChunk.begin() + static_cast<std::ptrdiff_t>(received * 2));
    }

    EXPECT_FALSE(streamedOutput.empty());
}
