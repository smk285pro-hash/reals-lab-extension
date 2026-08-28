#include "reals/audio/Engine.h"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace {

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

// Helper to write a basic 16-bit PCM WAV file for engine testing
bool createTestWav(const std::string& filename, int sampleRate, int channels, float durationSec, float freqHz) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    int numSamples = static_cast<int>(sampleRate * durationSec * channels);
    int subchunk2Size = numSamples * sizeof(int16_t);
    int chunkSize = 36 + subchunk2Size;
    int byteRate = sampleRate * channels * sizeof(int16_t);
    int blockAlign = channels * sizeof(int16_t);

    // RIFF header
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

    // fmt subchunk
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    int16_t audioFormat = 1; // PCM
    int16_t numChannels = static_cast<int16_t>(channels);
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    int16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&subchunk2Size), 4);

    constexpr float kPi = 3.14159265358979323846f;
    int frames = static_cast<int>(sampleRate * durationSec);
    for (int i = 0; i < frames; ++i) {
        float s = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / static_cast<float>(sampleRate));
        int16_t pcm = static_cast<int16_t>(s * 30000.0f);
        for (int c = 0; c < channels; ++c) {
            file.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
        }
    }
    return true;
}

} // namespace

bool testEngineGettersAndSetters() {
    std::cout << "[TEST] Audio Engine: Getters & Setters..." << std::endl;
    auto& eng = reals::audio::Engine::instance();

    // Time ratio tests
    eng.setTimeRatio(1.25f);
    ASSERT_NEAR(eng.getTimeRatio(), 1.25f, 0.001f, "Time ratio set/get 1.25");
    eng.setTimeRatio(0.75f);
    ASSERT_NEAR(eng.getTimeRatio(), 0.75f, 0.001f, "Time ratio set/get 0.75");
    eng.setTimeRatio(1.0f);
    ASSERT_NEAR(eng.getTimeRatio(), 1.0f, 0.001f, "Time ratio set/get 1.0");

    // Pitch semitones tests
    eng.setPitchSemitones(5.0f);
    ASSERT_NEAR(eng.getPitchSemitones(), 5.0f, 0.001f, "Pitch semitones set/get 5.0");

    eng.setPitchSemitones(-7.0f);
    ASSERT_NEAR(eng.getPitchSemitones(), -7.0f, 0.001f, "Pitch semitones set/get -7.0");

    // Reset pitch and original key
    eng.resetPitch();
    ASSERT_NEAR(eng.getPitchSemitones(), 0.0f, 0.001f, "Reset pitch to 0.0");

    eng.setPitchSemitones(8.0f);
    eng.setOriginalKey();
    ASSERT_NEAR(eng.getPitchSemitones(), 0.0f, 0.001f, "Original key reset to 0.0");

    eng.resetPitch();
    return true;
}

bool testEnginePlaybackPipeline() {
    std::cout << "[TEST] Audio Engine: Playback Pipeline..." << std::endl;
    const std::string testWav = "test_synth.wav";
    ASSERT_TRUE(createTestWav(testWav, 44100, 2, 0.5f, 440.0f), "Create test WAV file");

    auto& eng = reals::audio::Engine::instance();
    ASSERT_TRUE(eng.init(), "Engine initialization");
    ASSERT_TRUE(eng.isReady(), "Engine is ready");

    // Probe file
    auto info = reals::audio::Engine::probeFile(testWav);
    ASSERT_TRUE(info.sampleRate == 44100, "Probe sample rate");
    ASSERT_TRUE(info.channels == 2, "Probe channels");
    ASSERT_NEAR(static_cast<float>(info.durationSeconds), 0.5f, 0.05f, "Probe duration");

    // Envelope calculation
    auto env = reals::audio::Engine::computeEnvelope(testWav);
    ASSERT_TRUE(!env.empty(), "Envelope non-empty");

    // Play file with time ratio and pitch shift
    eng.setTimeRatio(1.2f);
    eng.setPitchSemitones(4.0f);
    bool playOk = eng.playFile(testWav, false);
    ASSERT_TRUE(playOk, "Play test WAV file");
    ASSERT_TRUE(eng.isPlaying(), "Engine is playing");

    // Change parameters in real-time during playback
    eng.setPitchSemitones(-3.0f);
    eng.setTimeRatio(0.9f);
    eng.seekFraction(0.2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    eng.stop();
    ASSERT_TRUE(!eng.isPlaying(), "Engine stopped");

    // Clean up temporary WAV
    std::remove(testWav.c_str());
    return true;
}

int main() {
    std::cout << "=== Reals Lab: Audio Engine Unit Tests ===" << std::endl;
    int failed = 0;

    if (!testEngineGettersAndSetters()) failed++;
    if (!testEnginePlaybackPipeline()) failed++;

    if (failed == 0) {
        std::cout << "=== ALL AUDIO ENGINE TESTS PASSED ===" << std::endl;
        return 0;
    } else {
        std::cerr << "=== " << failed << " TESTS FAILED ===" << std::endl;
        return 1;
    }
}
