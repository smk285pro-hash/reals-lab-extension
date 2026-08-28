#include "reals/audio/SoundTouchProcessor.h"

#include <cmath>
#include <complex>
#include <iostream>
#include <numeric>
#include <vector>

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Generate pure sine wave at a specific frequency
std::vector<float> generateSine(float freqHz, int sampleRate, float durationSeconds, int channels = 1) {
    size_t numFrames = static_cast<size_t>(sampleRate * durationSeconds);
    std::vector<float> buffer(numFrames * channels);
    for (size_t i = 0; i < numFrames; ++i) {
        float sample = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / static_cast<float>(sampleRate));
        for (int c = 0; c < channels; ++c) {
            buffer[i * channels + c] = sample;
        }
    }
    return buffer;
}

// Estimate dominant frequency using normalized autocorrelation with lowest-lag peak picking
float estimateDominantFrequency(const std::vector<float>& samples, int sampleRate, int channels = 1) {
    if (samples.empty()) return 0.0f;
    size_t numFrames = samples.size() / channels;
    if (numFrames < 512) return 0.0f;

    // Use mono channel 0 for pitch estimation
    std::vector<float> mono(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        mono[i] = samples[i * channels];
    }

    int minLag = sampleRate / 2000;
    int maxLag = sampleRate / 50;
    if (maxLag >= static_cast<int>(numFrames) / 2) maxLag = static_cast<int>(numFrames) / 2 - 1;

    size_t testWindow = numFrames / 2;
    std::vector<float> r(maxLag + 1, 0.0f);
    float globalMax = -1.0f;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float num = 0.0f, d0 = 0.0f, d1 = 0.0f;
        for (size_t i = 0; i < testWindow; ++i) {
            float x0 = mono[i];
            float x1 = mono[i + lag];
            num += x0 * x1;
            d0 += x0 * x0;
            d1 += x1 * x1;
        }
        float den = std::sqrt(d0 * d1);
        r[lag] = (den > 1e-9f) ? (num / den) : 0.0f;
        globalMax = std::max(globalMax, r[lag]);
    }

    if (globalMax < 0.25f) return 0.0f;

    const float thresh = globalMax * 0.70f;
    int bestLag = -1;
    for (int lag = minLag + 1; lag < maxLag; ++lag) {
        if (r[lag] > r[lag - 1] && r[lag] >= r[lag + 1] && r[lag] >= thresh) {
            bestLag = lag;
            break;
        }
    }
    if (bestLag < 0) {
        float best = -1.0f;
        for (int lag = minLag; lag <= maxLag; ++lag) {
            if (r[lag] > best) { best = r[lag]; bestLag = lag; }
        }
    }

    if (bestLag <= 0) return 0.0f;
    float fracLag = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        float alpha = r[bestLag - 1];
        float beta = r[bestLag];
        float gamma = r[bestLag + 1];
        float denom = 2.0f * (2.0f * beta - alpha - gamma);
        if (std::abs(denom) > 1e-6f) {
            fracLag += (alpha - gamma) / denom;
        }
    }

    return static_cast<float>(sampleRate) / fracLag;
}

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion FAILED: " << msg << " (" #cond ") at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

#define ASSERT_NEAR(val, target, tolerance, msg) \
    do { \
        float diff = std::fabs((val) - (target)); \
        if (diff > (tolerance)) { \
            std::cerr << "Assertion FAILED: " << msg << " | value: " << (val) << ", target: " << (target) \
                      << ", diff: " << diff << ", tol: " << (tolerance) << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

} // namespace

bool testInitializationAndLatency() {
    std::cout << "[TEST] SoundTouchProcessor: Initialization and Latency..." << std::endl;
    reals::audio::SoundTouchProcessor proc(44100, 2, true);

    ASSERT_TRUE(proc.getSampleRate() == 44100, "Sample rate should be 44100");
    ASSERT_TRUE(proc.getChannels() == 2, "Channels should be 2");
    ASSERT_TRUE(proc.isLowLatencyMode(), "Low latency mode should be active");

    const float latencyMs = proc.latencyMilliseconds();
    std::cout << "  Calculated latency: " << latencyMs << " ms (latency frames: " << proc.latencyFrames() << ")" << std::endl;
    ASSERT_TRUE(latencyMs < 30.0f, "Latency MUST be < 30ms for real-time responsiveness");

    // Test changing sample rate and channels
    proc.setSampleRate(48000);
    ASSERT_TRUE(proc.getSampleRate() == 48000, "Sample rate update to 48000");
    ASSERT_TRUE(proc.latencyMilliseconds() < 30.0f, "Latency at 48kHz must be < 30ms");

    proc.setChannels(1);
    ASSERT_TRUE(proc.getChannels() == 1, "Channel update to 1");

    return true;
}

bool testTimeStretchingDawBpmSync() {
    std::cout << "[TEST] SoundTouchProcessor: Time-Stretching (DAW BPM Sync)..." << std::endl;
    constexpr int sampleRate = 44100;
    constexpr float freqHz = 440.0f;
    constexpr float durationSec = 1.0f;

    auto inputAudio = generateSine(freqHz, sampleRate, durationSec, 2);
    size_t inputFrames = inputAudio.size() / 2;

    // Case 1: Faster tempo (ratio = 1.5x, e.g. 120 BPM sample in 180 BPM project)
    {
        reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
        proc.setTimeRatio(1.5f);
        ASSERT_NEAR(proc.getTimeRatio(), 1.5f, 0.001f, "Time ratio getter");

        auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
        size_t outFrames = outputAudio.size() / 2;
        float expectedFrames = static_cast<float>(inputFrames) / 1.5f;

        std::cout << "  Ratio 1.5x: input frames = " << inputFrames << ", output frames = " << outFrames 
                  << " (expected approx " << expectedFrames << ")" << std::endl;
        
        // Allow minor boundary tolerance
        ASSERT_NEAR(static_cast<float>(outFrames), expectedFrames, expectedFrames * 0.15f, 
                    "Output length should be compressed proportionally to tempo ratio 1.5x");

        // Verify pitch is unchanged (pitch-neutral time stretch)
        float outFreq = estimateDominantFrequency(outputAudio, sampleRate, 2);
        std::cout << "  Dominant frequency: " << outFreq << " Hz (original 440 Hz)" << std::endl;
        ASSERT_NEAR(outFreq, freqHz, 25.0f, "Pitch must remain unaffected during time-stretch");
    }

    // Case 2: Slower tempo (ratio = 0.8x, e.g. 150 BPM sample in 120 BPM project)
    {
        reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
        proc.setTimeRatio(0.8f);
        ASSERT_NEAR(proc.getTimeRatio(), 0.8f, 0.001f, "Time ratio getter");

        auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
        size_t outFrames = outputAudio.size() / 2;
        float expectedFrames = static_cast<float>(inputFrames) / 0.8f;

        std::cout << "  Ratio 0.8x: input frames = " << inputFrames << ", output frames = " << outFrames 
                  << " (expected approx " << expectedFrames << ")" << std::endl;

        ASSERT_NEAR(static_cast<float>(outFrames), expectedFrames, expectedFrames * 0.15f, 
                    "Output length should be expanded proportionally to tempo ratio 0.8x");

        float outFreq = estimateDominantFrequency(outputAudio, sampleRate, 2);
        std::cout << "  Dominant frequency: " << outFreq << " Hz (original 440 Hz)" << std::endl;
        ASSERT_NEAR(outFreq, freqHz, 25.0f, "Pitch must remain unaffected during time-stretch");
    }

    return true;
}

bool testPitchShiftingRealtimeKeyTransposition() {
    std::cout << "[TEST] SoundTouchProcessor: Pitch-Shifting (±12 Semitones)..." << std::endl;
    constexpr int sampleRate = 44100;
    constexpr float baseFreq = 440.0f; // A4
    constexpr float durationSec = 1.0f;

    auto inputAudio = generateSine(baseFreq, sampleRate, durationSec, 2);
    size_t inputFrames = inputAudio.size() / 2;

    // Test +12 semitones (1 Octave Up: 440Hz -> 880Hz)
    {
        reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
        proc.setPitchSemitones(12.0f);
        ASSERT_NEAR(proc.getPitchSemitones(), 12.0f, 0.001f, "Pitch semitones getter");
        ASSERT_NEAR(proc.getPitchRatio(), 2.0f, 0.001f, "Pitch ratio getter");

        auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
        size_t outFrames = outputAudio.size() / 2;

        std::cout << "  +12 semitones: input frames = " << inputFrames << ", output frames = " << outFrames << std::endl;
        ASSERT_NEAR(static_cast<float>(outFrames), static_cast<float>(inputFrames), static_cast<float>(inputFrames) * 0.15f,
                    "Duration must be preserved during pitch shift");

        float outFreq = estimateDominantFrequency(outputAudio, sampleRate, 2);
        std::cout << "  Shifted frequency: " << outFreq << " Hz (expected ~880 Hz)" << std::endl;
        ASSERT_NEAR(outFreq, 880.0f, 40.0f, "Frequency should be doubled (+12 semitones)");
    }

    // Test -12 semitones (1 Octave Down: 440Hz -> 220Hz)
    {
        reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
        proc.setPitchSemitones(-12.0f);
        ASSERT_NEAR(proc.getPitchSemitones(), -12.0f, 0.001f, "Pitch semitones getter");
        ASSERT_NEAR(proc.getPitchRatio(), 0.5f, 0.001f, "Pitch ratio getter");

        auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
        size_t outFrames = outputAudio.size() / 2;

        std::cout << "  -12 semitones: input frames = " << inputFrames << ", output frames = " << outFrames << std::endl;
        ASSERT_NEAR(static_cast<float>(outFrames), static_cast<float>(inputFrames), static_cast<float>(inputFrames) * 0.15f,
                    "Duration must be preserved during pitch shift");

        float outFreq = estimateDominantFrequency(outputAudio, sampleRate, 2);
        std::cout << "  Shifted frequency: " << outFreq << " Hz (expected ~220 Hz)" << std::endl;
        ASSERT_NEAR(outFreq, 220.0f, 25.0f, "Frequency should be halved (-12 semitones)");
    }

    // Test +7 semitones (Perfect Fifth: 440Hz * 2^(7/12) ~= 659.25Hz)
    {
        reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
        proc.setPitchSemitones(7.0f);
        auto outputAudio = proc.processBuffer(inputAudio.data(), inputFrames);
        float outFreq = estimateDominantFrequency(outputAudio, sampleRate, 2);
        float expectedFreq = 440.0f * std::pow(2.0f, 7.0f / 12.0f);
        std::cout << "  +7 semitones: " << outFreq << " Hz (expected ~" << expectedFreq << " Hz)" << std::endl;
        ASSERT_NEAR(outFreq, expectedFreq, 30.0f, "Frequency shift +7 semitones");
    }

    return true;
}

bool testResetPitchAndOriginalKey() {
    std::cout << "[TEST] SoundTouchProcessor: Reset Pitch & Original Key..." << std::endl;
    reals::audio::SoundTouchProcessor proc(44100, 2, true);

    proc.setPitchSemitones(5.5f);
    ASSERT_NEAR(proc.getPitchSemitones(), 5.5f, 0.001f, "Pitch set to 5.5");

    proc.resetPitch();
    ASSERT_NEAR(proc.getPitchSemitones(), 0.0f, 0.001f, "Reset pitch back to 0.0");
    ASSERT_NEAR(proc.getPitchRatio(), 1.0f, 0.001f, "Pitch ratio back to 1.0");

    proc.setPitchSemitones(-8.0f);
    proc.setOriginalKey();
    ASSERT_NEAR(proc.getPitchSemitones(), 0.0f, 0.001f, "Original key back to 0.0");
    ASSERT_NEAR(proc.getPitchRatio(), 1.0f, 0.001f, "Pitch ratio back to 1.0");

    return true;
}

bool testStreamingIO() {
    std::cout << "[TEST] SoundTouchProcessor: Streaming Chunk IO..." << std::endl;
    constexpr int sampleRate = 44100;
    reals::audio::SoundTouchProcessor proc(sampleRate, 2, true);
    proc.setPitchSemitones(3.0f);
    proc.setTimeRatio(1.2f);

    auto inputAudio = generateSine(440.0f, sampleRate, 0.5f, 2);
    size_t inputFrames = inputAudio.size() / 2;

    std::vector<float> streamedOutput;
    constexpr size_t kChunkSize = 256;
    size_t framesFed = 0;

    while (framesFed < inputFrames) {
        size_t chunk = std::min(kChunkSize, inputFrames - framesFed);
        proc.putSamples(inputAudio.data() + framesFed * 2, chunk);
        framesFed += chunk;

        std::vector<float> outChunk(kChunkSize * 2);
        size_t received = proc.receiveSamples(outChunk.data(), kChunkSize);
        if (received > 0) {
            streamedOutput.insert(streamedOutput.end(), outChunk.begin(), outChunk.begin() + received * 2);
        }
    }

    proc.flush();
    while (true) {
        std::vector<float> outChunk(kChunkSize * 2);
        size_t received = proc.receiveSamples(outChunk.data(), kChunkSize);
        if (received == 0) break;
        streamedOutput.insert(streamedOutput.end(), outChunk.begin(), outChunk.begin() + received * 2);
    }

    std::cout << "  Streamed frames out: " << (streamedOutput.size() / 2) << " from input: " << inputFrames << std::endl;
    ASSERT_TRUE(!streamedOutput.empty(), "Streaming should output samples");

    return true;
}

int main() {
    std::cout << "=== Reals Lab: SoundTouchProcessor Unit Tests ===" << std::endl;
    int failed = 0;

    if (!testInitializationAndLatency()) failed++;
    if (!testTimeStretchingDawBpmSync()) failed++;
    if (!testPitchShiftingRealtimeKeyTransposition()) failed++;
    if (!testResetPitchAndOriginalKey()) failed++;
    if (!testStreamingIO()) failed++;

    if (failed == 0) {
        std::cout << "=== ALL SOUNDTOUCH PROCESSOR TESTS PASSED ===" << std::endl;
        return 0;
    } else {
        std::cerr << "=== " << failed << " TESTS FAILED ===" << std::endl;
        return 1;
    }
}
