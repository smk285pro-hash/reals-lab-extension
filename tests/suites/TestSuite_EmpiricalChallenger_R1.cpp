#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/audio/Engine.h>
#include <reals/bridge/Bridge.h>
#include <reals/platform/Path.h>

namespace reals::test {

namespace {

// Mathematical oracle for playhead phase synchronization
struct PhaseSyncOracle {
    static double computeLoopBeats(double durationSec, double bpm) {
        if (bpm <= 0.0 || durationSec < 1.2) return 4.0;
        const double raw = (durationSec * bpm) / 60.0;
        return std::max(1.0, std::round(raw));
    }

    static double computeStartFraction(double fullBeats, double loopBeats) {
        if (loopBeats <= 0.0) return 0.0;
        double beatInLoop = std::fmod(fullBeats, loopBeats);
        if (beatInLoop < 0.0) beatInLoop += loopBeats;
        return std::clamp(beatInLoop / loopBeats, 0.0, 0.999);
    }
};

} // namespace

// ============================================================================
// Adversarial Challenge Suite: R1 Playhead Phase Synchronization
// ============================================================================

TEST(ChallengerR1, MathOracle_LoopLengths_1_2_4_8_16_Bars) {
    // Test BPMs commonly found in modern music production
    const std::vector<double> testBpms = {60.0, 85.0, 90.0, 120.0, 128.0, 140.0, 150.0, 174.0, 128.5, 174.25};
    const std::vector<int> barLengths = {1, 2, 4, 8, 16};

    for (double bpm : testBpms) {
        for (int bars : barLengths) {
            const int expectedBeats = bars * 4;
            const double perfectDurationSec = (static_cast<double>(expectedBeats) / bpm) * 60.0;
            
            // 1. Exact loop duration
            double calculatedBeats = PhaseSyncOracle::computeLoopBeats(perfectDurationSec, bpm);
            EXPECT_NEAR(calculatedBeats, static_cast<double>(expectedBeats), 1e-4);

            // 2. Loop duration with minor tail jitter (+/- 25ms export margin)
            double jitterPlus = PhaseSyncOracle::computeLoopBeats(perfectDurationSec + 0.025, bpm);
            EXPECT_NEAR(jitterPlus, static_cast<double>(expectedBeats), 1e-4);

            double jitterMinus = PhaseSyncOracle::computeLoopBeats(perfectDurationSec - 0.025, bpm);
            EXPECT_NEAR(jitterMinus, static_cast<double>(expectedBeats), 1e-4);
        }
    }
}

TEST(ChallengerR1, MathOracle_PlayheadPositions_Start_Mid_Offbeat_Negative) {
    const double loopBeats16 = 16.0; // 4 bars in 4/4

    // 1. Bar starts (Bar 1, 2, 3, 4, 5...)
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(0.0, loopBeats16), 0.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(4.0, loopBeats16), 0.25, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(8.0, loopBeats16), 0.50, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(12.0, loopBeats16), 0.75, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(16.0, loopBeats16), 0.0, 1e-5); // Wrap to 0.0
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(32.0, loopBeats16), 0.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(64.0, loopBeats16), 0.0, 1e-5);

    // 2. Mid-bar positions
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(2.0, loopBeats16), 2.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(6.0, loopBeats16), 6.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(10.0, loopBeats16), 10.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(14.0, loopBeats16), 14.0 / 16.0, 1e-5);

    // 3. Off-beat 8th, 16th, triplet, and swing positions
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(0.5, loopBeats16), 0.5 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(1.25, loopBeats16), 1.25 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(3.333333, loopBeats16), 3.333333 / 16.0, 1e-4);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(7.875, loopBeats16), 7.875 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(15.5, loopBeats16), 15.5 / 16.0, 1e-5);

    // 4. Boundary wrap edge cases
    EXPECT_LE(PhaseSyncOracle::computeStartFraction(15.9999, loopBeats16), 0.999);
    EXPECT_GE(PhaseSyncOracle::computeStartFraction(15.9999, loopBeats16), 0.990);

    // 5. Negative pre-roll / count-in positions
    // -1.0 beat in a 16-beat cycle corresponds to 15.0 beats (15.0 / 16.0 = 0.9375)
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(-1.0, loopBeats16), 15.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(-4.0, loopBeats16), 12.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(-8.0, loopBeats16), 8.0 / 16.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(-16.0, loopBeats16), 0.0, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(-17.5, loopBeats16), 14.5 / 16.0, 1e-5);

    // 6. Very large project beat counters (e.g. after hours of playback)
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(16004.0, loopBeats16), 0.25, 1e-5);
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(32008.5, loopBeats16), 8.5 / 16.0, 1e-5);
}

TEST(ChallengerR1, MathOracle_OddTimeSignatures) {
    // 3/4 Meter (3 beats per bar)
    // 4-bar loop = 12 beats
    const double loop34_4bar = 12.0;
    // Bar 3 Beat 2 = (2 * 3) + 1 = beat 7.0
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(7.0, loop34_4bar), 7.0 / 12.0, 1e-5);

    // 5/4 Meter (5 beats per bar)
    // 2-bar loop = 10 beats
    const double loop54_2bar = 10.0;
    // Bar 2 Beat 4 = (1 * 5) + 3 = beat 8.0
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(8.0, loop54_2bar), 8.0 / 10.0, 1e-5);

    // 7/8 Meter (7 eighth notes per bar)
    // 2-bar loop = 14 eighth notes
    const double loop78_2bar = 14.0;
    EXPECT_NEAR(PhaseSyncOracle::computeStartFraction(5.0, loop78_2bar), 5.0 / 14.0, 1e-5);
}

TEST(ChallengerR1, BridgeRPC_ComprehensivePhaseSyncExecution) {
    BridgeTestHarness harness(128.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_challenger_rpc");
    platform::ensureDir(tmpDir);

    // Generate 1-bar, 2-bar, 4-bar, 8-bar, 16-bar test samples at 128 BPM
    // 1 beat at 128 BPM = 60 / 128 = 0.46875s
    const double beatDur = 60.0 / 128.0;

    struct TestCase {
        int bars;
        int beats;
        double dawFullBeats;
        double expectedFraction;
    };

    const std::vector<TestCase> cases = {
        {1, 4, 0.0, 0.0},
        {1, 4, 1.0, 0.25},
        {1, 4, 2.5, 0.625},
        {2, 8, 4.0, 0.50},
        {2, 8, 6.0, 0.75},
        {4, 16, 0.0, 0.0},
        {4, 16, 4.0, 0.25},
        {4, 16, 9.0, 0.5625},
        {4, 16, 12.0, 0.75},
        {8, 32, 16.0, 0.50},
        {8, 32, 24.0, 0.75},
        {16, 64, 16.0, 0.25},
        {16, 64, 48.0, 0.75}
    };

    for (size_t i = 0; i < cases.size(); ++i) {
        const auto& tc = cases[i];
        const std::string path = platform::joinPath(tmpDir, "loop_case_" + std::to_string(i) + "_" + std::to_string(tc.bars) + "bars.wav");
        const double duration = tc.beats * beatDur;
        auto pcm = AudioTestFixtures::generateSine(440.0f, static_cast<float>(duration), 44100);
        EXPECT_TRUE(AudioTestFixtures::writeWavFile(path, pcm, 1, 44100));

        // Transport Playing at tc.dawFullBeats
        harness.host().setHostTransport(1, tc.dawFullBeats * beatDur, tc.dawFullBeats, 128.0);

        auto res = harness.call("audio.play", {
            {"path", path},
            {"syncBpm", true},
            {"sampleBpm", 128.0f},
            {"loop", true},
            // Engine is a process-wide singleton: earlier suites may have
            // left a pitch shift active, which would engage the DSP path
            // (and its latency compensation). Pin the pitch so this test
            // verifies the pure phase-sync math deterministically.
            {"pitchSemitones", 0.0}
        });

        EXPECT_TRUE(res.value("ok", false));
        EXPECT_TRUE(res["data"].value("phaseSynced", false));
        EXPECT_NEAR(res["data"].value("loopBeats", 0.0), static_cast<double>(tc.beats), 0.1);
        EXPECT_NEAR(res["data"].value("startFraction", 0.0), tc.expectedFraction, 0.01);

        harness.call("audio.stop", json::object());
    }
}

TEST(ChallengerR1, Engine_SeekingAndSampleExactPositionVerification) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_challenger");
    platform::ensureDir(tmpDir);
    const std::string testWav = platform::joinPath(tmpDir, "seek_test_exact.wav");

    const int sampleRate = 44100;
    const float durationSec = 4.0f; // 4.0 seconds = 176400 frames
    auto pcm = AudioTestFixtures::generateSine(440.0f, durationSec, sampleRate);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(testWav, pcm, 1, sampleRate));

    auto& engine = reals::audio::Engine::instance();

    // 1. Play with startFraction = 0.5 (starts at exactly 2.0s)
    EXPECT_TRUE(engine.playFile(testWav, true, 0.5));
    EXPECT_TRUE(engine.isPlaying());
    EXPECT_NEAR(engine.positionFraction(), 0.5, 0.05);

    // 2. Play with startFraction = 0.75
    EXPECT_TRUE(engine.playFile(testWav, true, 0.75));
    EXPECT_TRUE(engine.isPlaying());
    EXPECT_NEAR(engine.positionFraction(), 0.75, 0.05);

    // 3. Dynamic seekFraction while running
    engine.seekFraction(0.25);
    EXPECT_NEAR(engine.positionFraction(), 0.25, 0.05);

    engine.seekFraction(0.0);
    EXPECT_NEAR(engine.positionFraction(), 0.0, 0.05);

    engine.stop();
    EXPECT_FALSE(engine.isPlaying());
}

TEST(ChallengerR1, Engine_AdversarialBoundarySeeking_NoCrashNoOverflow) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_challenger");
    platform::ensureDir(tmpDir);
    const std::string testWav = platform::joinPath(tmpDir, "adversarial_bounds.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.5f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(testWav, pcm, 1, 44100));

    auto& engine = reals::audio::Engine::instance();

    // Test negative fraction
    EXPECT_TRUE(engine.playFile(testWav, false, -5.0));
    EXPECT_TRUE(engine.isPlaying());
    EXPECT_GE(engine.positionFraction(), 0.0);

    // Test fraction >= 1.0 (should clamp safely to 0.999 without EOF freeze)
    EXPECT_TRUE(engine.playFile(testWav, false, 1.0));
    EXPECT_TRUE(engine.isPlaying());

    EXPECT_TRUE(engine.playFile(testWav, false, 99.9));
    EXPECT_TRUE(engine.isPlaying());

    // Test dynamic seek bounds
    engine.seekFraction(-10.0);
    EXPECT_GE(engine.positionFraction(), 0.0);

    engine.seekFraction(2.0);
    EXPECT_LE(engine.positionFraction(), 1.0);

    engine.stop();
}

TEST(ChallengerR1, StressHarness_RapidPlaybackSeekingUnderDivergentBpmAndPitch) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_challenger");
    platform::ensureDir(tmpDir);
    const std::string testWav = platform::joinPath(tmpDir, "stress_seek_loop.wav");

    auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 3.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(testWav, pcm, 2, 44100));

    auto& engine = reals::audio::Engine::instance();
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> fracDist(0.0, 0.999);
    std::uniform_real_distribution<float> ratioDist(0.5f, 2.0f);
    std::uniform_real_distribution<float> pitchDist(-12.0f, 12.0f);

    // Rapidly seek and start playback 50 times under varied time stretch & pitch shift parameters
    for (int iter = 0; iter < 50; ++iter) {
        const double frac = fracDist(rng);
        const float ratio = ratioDist(rng);
        const float pitch = pitchDist(rng);

        engine.setTimeRatio(ratio);
        engine.setPitchSemitones(pitch);

        const bool ok = engine.playFile(testWav, true, frac);
        EXPECT_TRUE(ok);
        EXPECT_TRUE(engine.isPlaying());

        // Perform in-flight seek
        const double dynamicFrac = fracDist(rng);
        engine.seekFraction(dynamicFrac);

        // Query metering levels to confirm no NaN / Inf output
        auto lvl = engine.level();
        EXPECT_FALSE(std::isnan(lvl.peak));
        EXPECT_FALSE(std::isinf(lvl.peak));
        EXPECT_GE(lvl.peak, 0.0f);
        EXPECT_LE(lvl.peak, 1.0f);
    }

    engine.stop();
    EXPECT_FALSE(engine.isPlaying());
}

} // namespace reals::test
