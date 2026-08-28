// Audio Engine production tests, ported from the former standalone
// test_audio_engine binary (MIN-06) into the reals_tests suite.
// These also regress the lock-free parameter publication refactor (CRIT-03):
// setTimeRatio/setPitchSemitones/setLoop now publish through atomics and are
// exercised mid-playback.
#include "../framework/TestRunner.h"

#include "reals/audio/Engine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

namespace {

// Minimal 16-bit PCM WAV writer (RIFF) for engine tests.
bool createTestWav(const std::string& filename, int sampleRate, int channels, float durationSec, float freqHz) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) return false;

    const int numSamples = static_cast<int>(sampleRate * durationSec * channels);
    const int subchunk2Size = numSamples * static_cast<int>(sizeof(int16_t));
    const int chunkSize = 36 + subchunk2Size;
    const int byteRate = sampleRate * channels * static_cast<int>(sizeof(int16_t));
    const int blockAlign = channels * static_cast<int>(sizeof(int16_t));

    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&chunkSize), 4);
    file.write("WAVE", 4);

    file.write("fmt ", 4);
    const int subchunk1Size = 16;
    const int16_t audioFormat = 1; // PCM
    const int16_t numChannels = static_cast<int16_t>(channels);
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    file.write(reinterpret_cast<const char*>(&sampleRate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    const int16_t bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&subchunk2Size), 4);

    constexpr float kPi = 3.14159265358979323846f;
    const int frames = static_cast<int>(sampleRate * durationSec);
    for (int i = 0; i < frames; ++i) {
        const float s = std::sin(2.0f * kPi * freqHz * static_cast<float>(i) / static_cast<float>(sampleRate));
        const int16_t pcm = static_cast<int16_t>(s * 30000.0f);
        for (int c = 0; c < channels; ++c)
            file.write(reinterpret_cast<const char*>(&pcm), sizeof(int16_t));
    }
    return file.good();
}

} // namespace

TEST(AudioEngineCore, GettersAndSetters) {
    auto& eng = reals::audio::Engine::instance();

    eng.setTimeRatio(1.25f);
    EXPECT_NEAR(eng.getTimeRatio(), 1.25f, 0.001f);
    eng.setTimeRatio(0.75f);
    EXPECT_NEAR(eng.getTimeRatio(), 0.75f, 0.001f);
    eng.setTimeRatio(1.0f);
    EXPECT_NEAR(eng.getTimeRatio(), 1.0f, 0.001f);

    eng.setPitchSemitones(5.0f);
    EXPECT_NEAR(eng.getPitchSemitones(), 5.0f, 0.001f);
    eng.setPitchSemitones(-7.0f);
    EXPECT_NEAR(eng.getPitchSemitones(), -7.0f, 0.001f);

    eng.resetPitch();
    EXPECT_NEAR(eng.getPitchSemitones(), 0.0f, 0.001f);

    eng.setPitchSemitones(8.0f);
    eng.setOriginalKey();
    EXPECT_NEAR(eng.getPitchSemitones(), 0.0f, 0.001f);

    eng.resetPitch();
}

TEST(AudioEngineCore, PlaybackPipelineWithLiveParameterChanges) {
    const std::string testWav = "test_engine_core.wav";
    EXPECT_TRUE(createTestWav(testWav, 44100, 2, 0.5f, 440.0f));

    auto& eng = reals::audio::Engine::instance();
    EXPECT_TRUE(eng.init());
    EXPECT_TRUE(eng.isReady());

    const auto info = reals::audio::Engine::probeFile(testWav);
    EXPECT_EQ(info.sampleRate, 44100);
    EXPECT_EQ(info.channels, 2);
    EXPECT_NEAR(static_cast<float>(info.durationSeconds), 0.5f, 0.05f);

    const auto env = reals::audio::Engine::computeEnvelope(testWav);
    EXPECT_FALSE(env.empty());

    // Play with DSP active from the start.
    eng.setTimeRatio(1.2f);
    eng.setPitchSemitones(4.0f);
    EXPECT_TRUE(eng.playFile(testWav, false));
    EXPECT_TRUE(eng.isPlaying());

    // CRIT-03 regression: change parameters live during playback. These used
    // to take dspMutex on the UI thread; they now publish through atomics.
    eng.setPitchSemitones(-3.0f);
    eng.setTimeRatio(0.9f);
    eng.setLoop(true);
    eng.seekFraction(0.2);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    eng.stop();
    EXPECT_FALSE(eng.isPlaying());
    eng.setLoop(false);

    std::remove(testWav.c_str());
}
