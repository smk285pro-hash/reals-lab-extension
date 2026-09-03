#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "../framework/TestRunner.h"
#include "../framework/AudioTestFixtures.h"
#include "../framework/DspQualityAssertions.h"
#include <reals/audio/Engine.h>
#include <reals/audio/SoundTouchProcessor.h>

namespace reals::test {

namespace {

// Helper to create synthetic test signals with various harmonic profiles
std::vector<float> generateHarmonicTestSignal(float durationSec, int sampleRate, bool includeSubBass = true) {
    const size_t totalFrames = static_cast<size_t>(durationSec * static_cast<float>(sampleRate));
    std::vector<float> pcm(totalFrames * 2, 0.0f); // Stereo

    for (size_t i = 0; i < totalFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float sL = 0.0f;
        float sR = 0.0f;

        if (includeSubBass) {
            // 55Hz Sub-bass sine wave
            const float sub = 0.45f * std::sin(2.0f * 3.14159265f * 55.0f * t);
            sL += sub;
            sR += sub;
        }

        // 220Hz Low-mid fundamental
        sL += 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * t);
        sR += 0.25f * std::cos(2.0f * 3.14159265f * 220.0f * t);

        // 440Hz Mid harmonic
        sL += 0.15f * std::sin(2.0f * 3.14159265f * 440.0f * t);
        sR += 0.15f * std::sin(2.0f * 3.14159265f * 440.0f * t + 0.5f);

        // 1200Hz High-mid harmonic
        sL += 0.08f * std::sin(2.0f * 3.14159265f * 1200.0f * t);
        sR += 0.08f * std::sin(2.0f * 3.14159265f * 1200.0f * t);

        pcm[i * 2] = sL;
        pcm[i * 2 + 1] = sR;
    }
    return pcm;
}

// Generate an abrupt step signal (starting at non-zero DC amplitude)
std::vector<float> generateStepDiscontinuitySignal(float durationSec, int sampleRate, float startDc = 0.90f) {
    const size_t totalFrames = static_cast<size_t>(durationSec * static_cast<float>(sampleRate));
    std::vector<float> pcm(totalFrames * 2, 0.0f);

    for (size_t i = 0; i < totalFrames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        // Abrupt jump at start (t=0 -> startDc) followed by smooth continuous tone
        const float s = startDc * std::cos(2.0f * 3.14159265f * 150.0f * t);
        pcm[i * 2] = s;
        pcm[i * 2 + 1] = s;
    }
    return pcm;
}

// Generate realistic 120 BPM 4-on-the-floor EDM kick pattern with sub-bass punch
std::vector<float> generateEdmKickPattern(float durationSec, int sampleRate, float bpm = 120.0f) {
    const size_t totalFrames = static_cast<size_t>(durationSec * static_cast<float>(sampleRate));
    std::vector<float> pcm(totalFrames, 0.0f);

    const int kickInterval = static_cast<int>(static_cast<float>(sampleRate) * 60.0f / bpm);
    for (int k = 0; k * kickInterval < static_cast<int>(totalFrames); ++k) {
        const int start = k * kickInterval;
        const int kickLen = sampleRate / 10; // 100ms
        for (int i = 0; i < kickLen && (start + i) < static_cast<int>(totalFrames); ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const float freq = 130.0f * std::exp(-t * 35.0f) + 48.0f; // 178Hz -> 48Hz pitch drop
            const float env = std::exp(-t * 22.0f);
            pcm[static_cast<size_t>(start + i)] += 0.88f * env * std::sin(2.0f * 3.14159265f * freq * t);
        }
    }
    return pcm;
}

} // namespace

// ============================================================================
// 1. Playback Start Onset Adversarial Stress Tests
// ============================================================================

TEST(AdversarialDSPChallenger, OnsetMicroFade_MultiConditionMatrix) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string testWav = "adv_onset_test.wav";

    // Matrix of test conditions
    const struct Condition {
        float startDc;
        float timeRatio;
        float pitchSemitones;
        double startFraction;
    } conditions[] = {
        { 0.95f, 1.00f,  0.0f, 0.00 }, // Bypass Heaviside DC step at start
        { 0.85f, 0.50f,  0.0f, 0.00 }, // Decelerated 0.5x
        { 0.85f, 0.75f, -5.0f, 0.00 }, // Decelerated 0.75x + pitch shift down
        { 0.85f, 1.25f, +3.0f, 0.00 }, // Accelerated 1.25x + pitch shift up
        { 0.85f, 1.50f, +7.0f, 0.00 }, // Accelerated 1.50x + fifth shift
        { 0.85f, 2.00f, +12.0f, 0.00 }, // Extreme 2.0x + octave shift
        { 0.90f, 1.00f,  0.0f, 0.33 }, // Bypass with non-zero startFraction
        { 0.90f, 1.25f,  0.0f, 0.67 }  // DSP mode with non-zero startFraction
    };

    for (const auto& cond : conditions) {
        auto pcm = generateStepDiscontinuitySignal(2.0f, sampleRate, cond.startDc);
        AudioTestFixtures::writeWavFile(testWav, pcm, 2, sampleRate, true);

        engine.setTimeRatio(cond.timeRatio);
        engine.setPitchSemitones(cond.pitchSemitones);
        engine.setVolume(1.0f);

        bool ok = engine.playFile(testWav, false, cond.startFraction);
        EXPECT_TRUE(ok);

        constexpr size_t kBlock = 512;
        std::vector<float> outL(kBlock, 0.0f);
        std::vector<float> outR(kBlock, 0.0f);
        engine.renderFrames(outL.data(), outR.data(), kBlock);
        engine.stop();

        // 1. First sample must be smoothly faded (w[0] = 0.0, output near 0.0)
        EXPECT_LT(std::abs(outL[0]), 0.05f);
        EXPECT_LT(std::abs(outR[0]), 0.05f);

        // 2. Discontinuity and Derivative Metrics
        std::vector<float> interleaved(kBlock * 2);
        for (size_t i = 0; i < kBlock; ++i) {
            interleaved[i * 2] = outL[i];
            interleaved[i * 2 + 1] = outR[i];
        }

        auto rep = analyzeBufferDiscontinuities(interleaved.data(), kBlock, 2, 0.40f, 0.25f);
        EXPECT_EQ(rep.stepDiscontinuities, 0u);
        EXPECT_EQ(rep.popGlitches, 0u);
        EXPECT_LT(rep.maxFirstDerivative, 0.40f);
        EXPECT_LT(rep.maxSecondDerivative, 0.80f);
    }

    std::filesystem::remove(testWav);
}

// ============================================================================
// 2. 100 Rapid Random Seeks Adversarial Stress Test
// ============================================================================

TEST(AdversarialDSPChallenger, Stress_100_RapidRandomSeeks_BypassMode) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string testWav = "adv_100_seeks_bypass.wav";

    auto pcm = generateHarmonicTestSignal(10.0f, sampleRate, true);
    AudioTestFixtures::writeWavFile(testWav, pcm, 2, sampleRate, true);

    engine.setTimeRatio(1.0f);
    engine.setPitchSemitones(0.0f);
    engine.setVolume(1.0f);

    bool ok = engine.playFile(testWav, true, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 256;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    std::vector<float> recordedStream;
    recordedStream.reserve(100 * kBlock * 2);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> seekDist(0.01, 0.98);

    for (int seekIdx = 0; seekIdx < 100; ++seekIdx) {
        // Trigger rapid random seek
        double frac = seekDist(rng);
        engine.seekFraction(frac);

        // Render block immediately across seek transition
        engine.renderFrames(outL.data(), outR.data(), kBlock);

        for (size_t i = 0; i < kBlock; ++i) {
            recordedStream.push_back(outL[i]);
            recordedStream.push_back(outR[i]);
        }
    }

    engine.stop();
    std::filesystem::remove(testWav);

    const size_t totalFrames = recordedStream.size() / 2;
    auto rep = analyzeBufferDiscontinuities(recordedStream.data(), totalFrames, 2, 0.40f, 0.25f);

    EXPECT_EQ(rep.stepDiscontinuities, 0u);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_LT(rep.maxFirstDerivative, 0.40f);
}

TEST(AdversarialDSPChallenger, Stress_100_RapidRandomSeeks_DSPMode) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string testWav = "adv_100_seeks_dsp.wav";

    auto pcm = generateHarmonicTestSignal(10.0f, sampleRate, true);
    AudioTestFixtures::writeWavFile(testWav, pcm, 2, sampleRate, true);

    // Active DSP mode with tempo stretch 1.20x and pitch shift +3 semitones
    engine.setTimeRatio(1.20f);
    engine.setPitchSemitones(3.0f);
    engine.setVolume(1.0f);

    bool ok = engine.playFile(testWav, true, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    std::vector<float> recordedStream;
    recordedStream.reserve(100 * kBlock * 2);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> seekDist(0.01, 0.98);

    for (int seekIdx = 0; seekIdx < 100; ++seekIdx) {
        // Rapid random seek
        double frac = seekDist(rng);
        engine.seekFraction(frac);

        // Render block immediately across seek transition
        engine.renderFrames(outL.data(), outR.data(), kBlock);

        for (size_t i = 0; i < kBlock; ++i) {
            recordedStream.push_back(outL[i]);
            recordedStream.push_back(outR[i]);
        }
    }

    engine.stop();
    std::filesystem::remove(testWav);

    const size_t totalFrames = recordedStream.size() / 2;
    auto rep = analyzeBufferDiscontinuities(recordedStream.data(), totalFrames, 2, 0.40f, 0.25f);

    EXPECT_EQ(rep.stepDiscontinuities, 0u);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_LT(rep.maxFirstDerivative, 0.40f);
}

// ============================================================================
// 3. Extreme Tempo Ratios Stress Matrix (0.5x, 0.75x, 1.0x, 1.25x, 1.5x, 2.0x)
// ============================================================================

TEST(AdversarialDSPChallenger, ExtremeTempoRatios_DiscontinuityAndSubBassPreservation) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string kickWav = "adv_extreme_tempo_kick.wav";

    // 120 BPM 4-on-the-floor EDM kick pattern (4 seconds, 8 kicks)
    auto kickPcmMono = generateEdmKickPattern(4.0f, sampleRate, 120.0f);
    std::vector<float> kickPcmStereo(kickPcmMono.size() * 2, 0.0f);
    for (size_t i = 0; i < kickPcmMono.size(); ++i) {
        kickPcmStereo[i * 2] = kickPcmMono[i];
        kickPcmStereo[i * 2 + 1] = kickPcmMono[i];
    }
    AudioTestFixtures::writeWavFile(kickWav, kickPcmStereo, 2, sampleRate, true);

    const std::vector<float> extremeRatios = {0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 2.00f};

    for (float ratio : extremeRatios) {
        engine.setTimeRatio(ratio);
        engine.setPitchSemitones(0.0f);
        engine.setVolume(1.0f);

        bool ok = engine.playFile(kickWav, false, 0.0);
        EXPECT_TRUE(ok);

        constexpr size_t kBlock = 512;
        std::vector<float> outL(kBlock, 0.0f);
        std::vector<float> outR(kBlock, 0.0f);

        std::vector<float> renderedAudioMono;
        std::vector<float> renderedAudioInterleaved;

        const size_t totalStretchedFrames = static_cast<size_t>(static_cast<float>(kickPcmMono.size()) / ratio);
        size_t framesRendered = 0;

        while (framesRendered < totalStretchedFrames) {
            engine.renderFrames(outL.data(), outR.data(), kBlock);
            for (size_t i = 0; i < kBlock; ++i) {
                renderedAudioMono.push_back(0.5f * (outL[i] + outR[i]));
                renderedAudioInterleaved.push_back(outL[i]);
                renderedAudioInterleaved.push_back(outR[i]);
            }
            framesRendered += kBlock;
        }
        engine.stop();

        // 1. Derivative and Discontinuity Check
        const size_t frames = renderedAudioInterleaved.size() / 2;
        auto rep = analyzeBufferDiscontinuities(renderedAudioInterleaved.data(), frames, 2, 0.40f, 0.25f);
        EXPECT_EQ(rep.stepDiscontinuities, 0u);
        EXPECT_EQ(rep.popGlitches, 0u);

        // 2. Sub-Bass (20-150Hz) & Kick Punch Preservation
        auto metric = measureKickAndSubBassPreservation(kickPcmMono, renderedAudioMono, ratio, sampleRate);
        EXPECT_GT(metric.kickCount, 0u);
        EXPECT_EQ(metric.droppedKicks, 0u);
        EXPECT_GE(metric.avgPunchRatio, 0.45f);
        EXPECT_GE(metric.avgSubBassEnergyRatio, 0.40f);
    }

    std::filesystem::remove(kickWav);
}

// ============================================================================
// 4. Dynamic Continuous Tempo Modulation Sweep Stress Test
// ============================================================================

TEST(AdversarialDSPChallenger, DynamicContinuousTempoSweep_ZeroGlitches) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string testWav = "adv_dynamic_sweep.wav";

    auto pcm = generateHarmonicTestSignal(6.0f, sampleRate, true);
    AudioTestFixtures::writeWavFile(testWav, pcm, 2, sampleRate, true);

    engine.setTimeRatio(0.50f);
    engine.setPitchSemitones(0.0f);
    engine.setVolume(1.0f);

    bool ok = engine.playFile(testWav, true, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    std::vector<float> recordedStream;
    recordedStream.reserve(60 * kBlock * 2);

    // Continuous sweep: 0.5x -> 2.0x -> 0.5x across 60 render blocks
    for (int step = 0; step < 60; ++step) {
        float dynamicRatio = (step < 30)
            ? (0.50f + (1.50f * static_cast<float>(step) / 30.0f))   // 0.5x -> 2.0x
            : (2.00f - (1.50f * static_cast<float>(step - 30) / 30.0f)); // 2.0x -> 0.5x
        
        engine.setTimeRatio(dynamicRatio);
        engine.renderFrames(outL.data(), outR.data(), kBlock);

        for (size_t i = 0; i < kBlock; ++i) {
            recordedStream.push_back(outL[i]);
            recordedStream.push_back(outR[i]);
        }
    }

    engine.stop();
    std::filesystem::remove(testWav);

    const size_t totalFrames = recordedStream.size() / 2;
    auto rep = analyzeBufferDiscontinuities(recordedStream.data(), totalFrames, 2, 0.40f, 0.25f);

    EXPECT_EQ(rep.stepDiscontinuities, 0u);
    EXPECT_EQ(rep.popGlitches, 0u);
    EXPECT_LT(rep.maxFirstDerivative, 0.40f);
}

// ============================================================================
// 5. Boundary & Extreme Seek Operations
// ============================================================================

TEST(AdversarialDSPChallenger, BoundaryConditions_ZeroAndEofSeeks) {
    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    const int sampleRate = 48000;
    const std::string testWav = "adv_boundary_seeks.wav";

    auto pcm = generateHarmonicTestSignal(3.0f, sampleRate, true);
    AudioTestFixtures::writeWavFile(testWav, pcm, 2, sampleRate, true);

    engine.setTimeRatio(1.10f);
    engine.setPitchSemitones(1.0f);
    engine.setVolume(1.0f);

    bool ok = engine.playFile(testWav, true, 0.0);
    EXPECT_TRUE(ok);

    constexpr size_t kBlock = 512;
    std::vector<float> outL(kBlock, 0.0f);
    std::vector<float> outR(kBlock, 0.0f);

    // 1. Rapid back-to-back seeks to exact 0.0 (head)
    for (int i = 0; i < 5; ++i) {
        engine.seekFraction(0.0);
        engine.renderFrames(outL.data(), outR.data(), kBlock);
    }

    // 2. Rapid seek to 1.0 (EOF / loop wrap boundary)
    engine.seekFraction(1.0);
    engine.renderFrames(outL.data(), outR.data(), kBlock);

    // 3. Negative / clamped out-of-range seeks
    engine.seekFraction(-0.50);
    engine.renderFrames(outL.data(), outR.data(), kBlock);

    engine.seekFraction(1.50);
    engine.renderFrames(outL.data(), outR.data(), kBlock);

    engine.stop();
    std::filesystem::remove(testWav);

    EXPECT_TRUE(true); // Reached here with zero crashes, zero deadlocks, zero exceptions
}

} // namespace reals::test
