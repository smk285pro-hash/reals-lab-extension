#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "../framework/TestRunner.h"
#include "../framework/AudioTestFixtures.h"
#include "../framework/DspQualityAssertions.h"
#include <reals/audio/Engine.h>
#include <reals/audio/SoundTouchProcessor.h>

namespace reals::test {

TEST(AudioDSPQuality, TestPlaybackOnsetMicroFade) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    // Create a synthetic audio file with non-zero initial DC amplitude (0.85f)
    const int sampleRate = 48000;
    const int numSamples = sampleRate * 2;
    std::vector<float> pcm(static_cast<size_t>(numSamples) * 2, 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        // Step function starting at 0.85f with 100Hz sine
        float s = 0.85f * std::sin(2.0f * 3.14159265f * 100.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
        if (i < 200) s = 0.80f; // Abrupt Heaviside step at start
        pcm[static_cast<size_t>(i) * 2] = s;
        pcm[static_cast<size_t>(i) * 2 + 1] = s;
    }

    const std::string tempPath = "test_onset_step.wav";
    AudioTestFixtures::writeWavFile(tempPath, pcm, 2, sampleRate, true);

    engine.setTimeRatio(1.0f);
    engine.setPitchSemitones(0.0f);
    engine.setVolume(1.0f);
    bool ok = engine.playFile(tempPath, false, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);
    engine.renderFrames(outL.data(), outR.data(), kBlock);
    engine.stop();

    std::filesystem::remove(tempPath);

    // Analyze onset discontinuity
    std::vector<float> interleaved(kBlock * 2);
    for (size_t i = 0; i < kBlock; ++i) {
        interleaved[i * 2] = outL[i];
        interleaved[i * 2 + 1] = outR[i];
    }

    auto rep = analyzeBufferDiscontinuities(interleaved.data(), kBlock, 2, 0.40f, 0.25f);
    // Onset frame 0 must start softly (raised-cosine window w[0]=0)
    EXPECT_LT(std::abs(outL[0]), 0.02f);
    EXPECT_LT(std::abs(outR[0]), 0.02f);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_EQ(rep.stepDiscontinuities, 0u);
}

TEST(AudioDSPQuality, TestTransportSeekContinuity) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    // Create 4-second stereo tone
    const int sampleRate = 48000;
    const int numSamples = sampleRate * 4;
    std::vector<float> pcm(static_cast<size_t>(numSamples) * 2, 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        float sL = 0.70f * std::sin(2.0f * 3.14159265f * 220.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
        float sR = 0.70f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
        pcm[static_cast<size_t>(i) * 2] = sL;
        pcm[static_cast<size_t>(i) * 2 + 1] = sR;
    }

    const std::string tempPath = "test_seek_tone.wav";
    AudioTestFixtures::writeWavFile(tempPath, pcm, 2, sampleRate, true);

    engine.setTimeRatio(1.0f);
    engine.setPitchSemitones(0.0f);
    engine.setVolume(1.0f);
    bool ok = engine.playFile(tempPath, false, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    // Render 1st block
    engine.renderFrames(outL.data(), outR.data(), kBlock);

    // Trigger seek to 0.50 (2 seconds in)
    engine.seekFraction(0.50);

    // Render 2nd block across the seek boundary
    engine.renderFrames(outL.data(), outR.data(), kBlock);
    engine.stop();

    std::filesystem::remove(tempPath);

    std::vector<float> interleaved(kBlock * 2);
    for (size_t i = 0; i < kBlock; ++i) {
        interleaved[i * 2] = outL[i];
        interleaved[i * 2 + 1] = outR[i];
    }

    auto rep = analyzeBufferDiscontinuities(interleaved.data(), kBlock, 2, 0.40f, 0.25f);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_EQ(rep.stepDiscontinuities, 0u);
}

TEST(AudioDSPQuality, TestSoundTouchPreRollPriming) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const int numSamples = sampleRate * 3;
    std::vector<float> pcm(static_cast<size_t>(numSamples) * 2, 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        float s = 0.80f * std::sin(2.0f * 3.14159265f * 330.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
        pcm[static_cast<size_t>(i) * 2] = s;
        pcm[static_cast<size_t>(i) * 2 + 1] = s;
    }

    const std::string tempPath = "test_preroll_tone.wav";
    AudioTestFixtures::writeWavFile(tempPath, pcm, 2, sampleRate, true);

    // DSP Mode: time-stretch 1.15x and pitch shift +2 semitones
    engine.setTimeRatio(1.15f);
    engine.setPitchSemitones(2.0f);
    engine.setVolume(1.0f);

    bool ok = engine.playFile(tempPath, false, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    engine.renderFrames(outL.data(), outR.data(), kBlock);
    engine.stop();

    std::filesystem::remove(tempPath);

    // Verify block contains valid audio energy without silence padding hole
    float maxAmp = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) {
        maxAmp = std::max(maxAmp, std::abs(outL[i]));
    }
    EXPECT_GT(maxAmp, 0.30f);
}

TEST(AudioDSPQuality, TestC1SoftLimiterContinuity) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    // Generate high-amplitude 100Hz sine wave exceeding 0 dBFS (peak 1.50)
    const int sampleRate = 48000;
    const int numSamples = sampleRate;
    std::vector<float> pcm(static_cast<size_t>(numSamples) * 2, 0.0f);
    for (int i = 0; i < numSamples; ++i) {
        float s = 1.50f * std::sin(2.0f * 3.14159265f * 100.0f * static_cast<float>(i) / static_cast<float>(sampleRate));
        pcm[static_cast<size_t>(i) * 2] = s;
        pcm[static_cast<size_t>(i) * 2 + 1] = s;
    }

    const std::string tempPath = "test_limiter_sine.wav";
    AudioTestFixtures::writeWavFile(tempPath, pcm, 2, sampleRate, true);

    engine.setTimeRatio(1.08f);
    engine.setPitchSemitones(0.0f);
    engine.setVolume(1.0f);
    bool ok = engine.playFile(tempPath, false, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 2048;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);
    engine.renderFrames(outL.data(), outR.data(), kBlock);
    engine.stop();

    std::filesystem::remove(tempPath);

    // Assert that output peaks are strictly compressed within < 1.0f
    float maxOut = 0.0f;
    for (size_t i = 0; i < kBlock; ++i) {
        maxOut = std::max(maxOut, std::abs(outL[i]));
    }
    EXPECT_LT(maxOut, 1.0f);
    EXPECT_GT(maxOut, 0.90f);

    // Verify derivative continuity
    std::vector<float> interleaved(kBlock * 2);
    for (size_t i = 0; i < kBlock; ++i) {
        interleaved[i * 2] = outL[i];
        interleaved[i * 2 + 1] = outR[i];
    }
    auto rep = analyzeBufferDiscontinuities(interleaved.data(), kBlock, 2, 0.40f, 0.25f);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_EQ(rep.stepDiscontinuities, 0u);
}

TEST(AudioDSPQuality, TestSyntheticKickPreservationUnderTempoScaling) {
    const int sampleRate = 48000;
    const int numSamples = sampleRate * 4; // 4 seconds (8 kicks at 120 BPM)
    std::vector<float> original(static_cast<size_t>(numSamples), 0.0f);

    // Synthesize 120 BPM 4-on-the-floor kick pattern
    const int kickInterval = sampleRate * 60 / 120;
    for (int k = 0; k < 8; ++k) {
        const int start = k * kickInterval;
        for (int i = 0; i < sampleRate / 10; ++i) { // 100ms kick
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float freq = 120.0f * std::exp(-t * 35.0f) + 50.0f; // Pitch envelope 170Hz -> 50Hz
            const float env = std::exp(-t * 25.0f);
            if (start + i < numSamples) {
                original[static_cast<size_t>(start + i)] += 0.90f * env * std::sin(2.0f * 3.14159265f * freq * t);
            }
        }
    }

    reals::audio::SoundTouchProcessor st(sampleRate, 1, false);

    const std::vector<float> testRatios = {1.08f, 1.15f, 1.25f};
    for (float ratio : testRatios) {
        st.setTimeRatio(ratio);
        st.setPitchSemitones(0.0f);
        auto stretched = st.processBuffer(original.data(), original.size());

        auto metric = measureKickAndSubBassPreservation(original, stretched, ratio, sampleRate);
        EXPECT_GT(metric.kickCount, 0u);
        EXPECT_EQ(metric.droppedKicks, 0u);
        EXPECT_GE(metric.avgPunchRatio, 0.65f);
        EXPECT_GE(metric.avgSubBassEnergyRatio, 0.55f);
    }
}

} // namespace reals::test
