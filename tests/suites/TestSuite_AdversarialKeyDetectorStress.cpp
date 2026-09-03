// ============================================================================
// TestSuite_AdversarialKeyDetectorStress.cpp
//
// Adversarial Stress & Boundary Challenge Suite for KeyDetector (M1_2)
// Verifies boundary conditions: digital silence, extreme HF (>10kHz),
// sub-bass rumble (20-40Hz), white noise, impulses, NaN/Inf, weird rates,
// concurrency, and memory stability.
// ============================================================================

#include "../framework/AudioTestFixtures.h"
#include "../framework/TestRunner.h"
#include <reals/ai/KeyDetector.h>

#include <cmath>
#include <chrono>
#include <limits>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace reals::test {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Helper: Chirp generator (frequency sweep)
std::vector<float> generateChirp(float startHz, float endHz, float durationSec, int sampleRate = 44100, float amplitude = 0.8f) {
    const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> buffer(totalFrames);
    const double k = (endHz - startHz) / durationSec;
    for (size_t i = 0; i < totalFrames; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double phase = 2.0 * kPi * (startHz * t + 0.5 * k * t * t);
        buffer[i] = static_cast<float>(amplitude * std::sin(phase));
    }
    return buffer;
}

// Helper to get current process memory in bytes
size_t getProcessWorkingSetBytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif
    return 0;
}

} // namespace

// ============================================================================
// Category 1: Digital Silence & Null/Zero Buffers
// ============================================================================

TEST(AdversarialKeyStress, Silence_NullptrAndZeroFrames) {
    // 1. Null PCM pointer
    auto res1 = ai::KeyDetector::detect(nullptr, 44100, 44100);
    EXPECT_EQ(res1.key, "C");
    EXPECT_EQ(res1.mode, "Major");
    EXPECT_EQ(res1.confidence, 0.0f);

    // 2. Zero frames
    float dummy = 0.5f;
    auto res2 = ai::KeyDetector::detect(&dummy, 0, 44100);
    EXPECT_EQ(res2.key, "C");
    EXPECT_EQ(res2.mode, "Major");
    EXPECT_EQ(res2.confidence, 0.0f);

    // 3. Invalid sample rates
    auto res3 = ai::KeyDetector::detect(&dummy, 1024, 0);
    EXPECT_EQ(res3.confidence, 0.0f);
    auto res4 = ai::KeyDetector::detect(&dummy, 1024, -44100);
    EXPECT_EQ(res4.confidence, 0.0f);
}

TEST(AdversarialKeyStress, Silence_PureDigitalZerosVariousDurations) {
    // Test digital silence of various durations: 0.05s, 0.5s, 1.0s, 3.0s, 5.0s
    const std::vector<float> durations = {0.05f, 0.5f, 1.0f, 3.0f, 5.0f};
    for (float sec : durations) {
        auto silent = AudioTestFixtures::generateSilent(sec, 44100);
        auto res = ai::KeyDetector::detect(silent.data(), silent.size(), 44100);
        // Must not crash, key/mode must be valid string
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        EXPECT_FALSE(res.camelot.empty());
        EXPECT_FALSE(res.openKey.empty());
        // For unpitched silence, confidence should reflect lack of distinct musical pitch
        EXPECT_LE(res.confidence, 0.5f);
    }
}

TEST(AdversarialKeyStress, Silence_AcrossDifferentSampleRates) {
    const std::vector<int> rates = {8000, 11025, 22050, 44100, 48000, 88200, 96000};
    for (int rate : rates) {
        auto silent = AudioTestFixtures::generateSilent(1.5f, rate);
        auto res = ai::KeyDetector::detect(silent.data(), silent.size(), rate);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        EXPECT_LE(res.confidence, 0.5f);
    }
}

// ============================================================================
// Category 2: Extreme High Frequencies (>10 kHz)
// ============================================================================

TEST(AdversarialKeyStress, ExtremeHF_PureSinesAbove10kHz) {
    // Frequencies well above standard musical tonality (10kHz, 12kHz, 15kHz, 18kHz, 20kHz)
    // KeyDetector caps peak picking at 4500Hz, so these should yield zero audible musical peaks.
    const std::vector<float> freqs = {10000.0f, 12000.0f, 15000.0f, 18000.0f, 20000.0f, 21500.0f};
    for (float f : freqs) {
        auto hfAudio = AudioTestFixtures::generateSine(f, 2.0f, 44100, 0.8f);
        auto res = ai::KeyDetector::detect(hfAudio.data(), hfAudio.size(), 44100);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        // Since peaks above 4500Hz are ignored by design, confidence must be low
        EXPECT_LE(res.confidence, 0.5f);
    }
}

TEST(AdversarialKeyStress, ExtremeHF_ChirpSweep10kHzTo20kHz) {
    auto chirp = generateChirp(10000.0f, 20000.0f, 2.0f, 44100, 0.8f);
    auto res = ai::KeyDetector::detect(chirp.data(), chirp.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_LE(res.confidence, 0.5f);
}

// ============================================================================
// Category 3: Sub-Bass Rumble (20-40 Hz)
// ============================================================================

TEST(AdversarialKeyStress, SubBass_RumbleFrequencies20To45Hz) {
    // Infrasonic / extreme sub-bass rumble: 20Hz, 25Hz, 30Hz, 35Hz, 40Hz, 45Hz
    // KeyDetector's minimum bin is 70Hz, and fundamental range is 50Hz - 2500Hz.
    const std::vector<float> subFreqs = {20.0f, 25.0f, 30.0f, 35.0f, 40.0f, 45.0f};
    for (float f : subFreqs) {
        auto subAudio = AudioTestFixtures::generateSine(f, 2.0f, 44100, 0.9f);
        auto res = ai::KeyDetector::detect(subAudio.data(), subAudio.size(), 44100);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        // Sub-rumble below 70Hz does not enter peak picking bins, so confidence must be low
        EXPECT_LE(res.confidence, 0.5f);
    }
}

TEST(AdversarialKeyStress, SubBass_ChirpSweep15HzTo45Hz) {
    auto subChirp = generateChirp(15.0f, 45.0f, 2.5f, 44100, 0.9f);
    auto res = ai::KeyDetector::detect(subChirp.data(), subChirp.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_LE(res.confidence, 0.5f);
}

// ============================================================================
// Category 4: White Noise, Pink Noise & Unpitched Percussion
// ============================================================================

TEST(AdversarialKeyStress, Noise_FullScaleWhiteNoise) {
    // Dense flat-spectrum white noise at multiple amplitudes
    const std::vector<float> amps = {1.0f, 0.5f, 0.1f, 0.01f, 1e-4f};
    for (float amp : amps) {
        auto noise = AudioTestFixtures::generateNoise(2.0f, 44100, amp);
        auto res = ai::KeyDetector::detect(noise.data(), noise.size(), 44100);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        // Pure unpitched noise should have low confidence (no clear harmonic structure)
        EXPECT_LE(res.confidence, 0.55f);
    }
}

TEST(AdversarialKeyStress, Noise_BrownianPinkNoise) {
    // 1/f Brownian / Pink noise simulation
    const size_t total = 44100 * 2;
    std::vector<float> brown(total);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
    float val = 0.0f;
    for (size_t i = 0; i < total; ++i) {
        val += dist(rng);
        val = std::clamp(val, -0.9f, 0.9f);
        brown[i] = val;
    }
    auto res = ai::KeyDetector::detect(brown.data(), brown.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_LE(res.confidence, 0.65f);
}

// ============================================================================
// Category 5: Single-Sample Impulses & Dirac Deltas
// ============================================================================

TEST(AdversarialKeyStress, Impulse_SingleSampleAtVariousPositions) {
    const size_t bufSize = 44100 * 2;
    std::vector<size_t> positions = {0, 1, 1024, 2048, 4096, bufSize / 2, bufSize - 1};

    for (size_t pos : positions) {
        std::vector<float> impulse(bufSize, 0.0f);
        impulse[pos] = 1.0f;

        auto res = ai::KeyDetector::detect(impulse.data(), impulse.size(), 44100);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        std::cout << "[IMPULSE pos=" << pos << "] " << res.key << " " << res.mode << " conf=" << res.confidence << "\n";
        // An impulse is wideband/unpitched; confidence should not indicate strong musical tonality (<= 0.70)
        EXPECT_LE(res.confidence, 0.70f);
    }
}

TEST(AdversarialKeyStress, Impulse_SparsePeriodicClicks) {
    // 1 click every 0.25 seconds
    const size_t bufSize = 44100 * 2;
    std::vector<float> clicks(bufSize, 0.0f);
    for (size_t i = 0; i < bufSize; i += 11025) {
        clicks[i] = 0.9f;
    }
    auto res = ai::KeyDetector::detect(clicks.data(), clicks.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    std::cout << "[PERIODIC CLICKS] " << res.key << " " << res.mode << " conf=" << res.confidence << "\n";
    // Clicks form a low-frequency pulse train; confidence should not indicate true tonal certainty (<= 0.75)
    EXPECT_LE(res.confidence, 0.75f);
}

// ============================================================================
// Category 6: NaN, Inf, Subnormal, and Extreme Floats
// ============================================================================

TEST(AdversarialKeyStress, FloatAnomalies_AllNaN) {
    const size_t bufSize = 8192;
    std::vector<float> nanBuf(bufSize, std::numeric_limits<float>::quiet_NaN());

    // Must NOT crash, throw, or hang
    auto res = ai::KeyDetector::detect(nanBuf.data(), nanBuf.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_FALSE(res.mode.empty());
    // Confidence must be valid finite number in [0.0, 1.0]
    EXPECT_FALSE(std::isnan(res.confidence));
    EXPECT_FALSE(std::isinf(res.confidence));
    EXPECT_GE(res.confidence, 0.0f);
    EXPECT_LE(res.confidence, 1.0f);
}

TEST(AdversarialKeyStress, FloatAnomalies_AllPositiveAndNegativeInfinity) {
    const size_t bufSize = 8192;
    std::vector<float> posInfBuf(bufSize, std::numeric_limits<float>::infinity());
    std::vector<float> negInfBuf(bufSize, -std::numeric_limits<float>::infinity());

    auto resPos = ai::KeyDetector::detect(posInfBuf.data(), posInfBuf.size(), 44100);
    EXPECT_FALSE(resPos.key.empty());
    EXPECT_FALSE(std::isnan(resPos.confidence));
    EXPECT_FALSE(std::isinf(resPos.confidence));

    auto resNeg = ai::KeyDetector::detect(negInfBuf.data(), negInfBuf.size(), 44100);
    EXPECT_FALSE(resNeg.key.empty());
    EXPECT_FALSE(std::isnan(resNeg.confidence));
    EXPECT_FALSE(std::isinf(resNeg.confidence));
}

TEST(AdversarialKeyStress, FloatAnomalies_MixedValidWithInjectedNaN) {
    // Valid C Major triad with occasional NaN injected
    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 2.0f, 44100);
    cMaj[0] = std::numeric_limits<float>::quiet_NaN();
    cMaj[1000] = std::numeric_limits<float>::infinity();
    cMaj[5000] = -std::numeric_limits<float>::infinity();

    auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_FALSE(std::isnan(res.confidence));
    EXPECT_FALSE(std::isinf(res.confidence));
}

TEST(AdversarialKeyStress, FloatAnomalies_SubnormalDenormFloats) {
    const size_t bufSize = 44100;
    std::vector<float> subnormalBuf(bufSize, std::numeric_limits<float>::denorm_min());

    auto res = ai::KeyDetector::detect(subnormalBuf.data(), subnormalBuf.size(), 44100);
    EXPECT_FALSE(res.key.empty());
    EXPECT_FALSE(std::isnan(res.confidence));
    EXPECT_LE(res.confidence, 0.5f);
}

// ============================================================================
// Category 7: Buffer Length Boundary Conditions
// ============================================================================

TEST(AdversarialKeyStress, BufferSizes_CriticalThresholds) {
    // Test sizes around kNfft (4096) and kHopLength (1024)
    const std::vector<size_t> sizes = {1, 2, 64, 512, 1023, 1024, 1025, 2048, 4095, 4096, 4097, 8192};

    for (size_t s : sizes) {
        std::vector<float> buf(s, 0.5f);
        auto res = ai::KeyDetector::detect(buf.data(), buf.size(), 44100);
        EXPECT_FALSE(res.key.empty());
        EXPECT_FALSE(res.mode.empty());
        EXPECT_FALSE(std::isnan(res.confidence));
    }
}

// ============================================================================
// Category 8: Sample Rate Resampling Invariants
// ============================================================================

TEST(AdversarialKeyStress, SampleRates_AccuracyAcrossVariedRates) {
    // C Major chord (C4: 261.63Hz) synthesized at non-standard sample rates:
    // 22050, 32000, 48000, 88200, 96000
    const std::vector<int> testRates = {22050, 32000, 48000, 88200, 96000};
    for (int rate : testRates) {
        auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 2.5f, rate);
        auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), rate);
        EXPECT_EQ(res.key, "C");
        EXPECT_EQ(res.mode, "Major");
        EXPECT_EQ(res.camelot, "8B");
        EXPECT_EQ(res.openKey, "1d");
        EXPECT_GT(res.confidence, 0.5f);
    }
}

// ============================================================================
// Category 9: Extreme Dynamic Range & DC Offset
// ============================================================================

TEST(AdversarialKeyStress, DynamicRange_UltraQuietAndUltraLoud) {
    // 1. Extremely quiet signal (-80 dBFS, amplitude ~ 0.0001f)
    auto quietC = AudioTestFixtures::generateChordTriad(261.63f, false, 2.0f, 44100, 0.0001f);
    auto resQuiet = ai::KeyDetector::detect(quietC.data(), quietC.size(), 44100);
    // Relative threshold in KeyDetector is -60dB relative to max peak, so even quiet C should be detected!
    EXPECT_EQ(resQuiet.key, "C");
    EXPECT_EQ(resQuiet.mode, "Major");

    // 2. Ultra-loud / clipped signal (amplitude 20.0f)
    auto loudC = AudioTestFixtures::generateChordTriad(261.63f, false, 2.0f, 44100, 20.0f);
    auto resLoud = ai::KeyDetector::detect(loudC.data(), loudC.size(), 44100);
    EXPECT_EQ(resLoud.key, "C");
    EXPECT_EQ(resLoud.mode, "Major");
}

TEST(AdversarialKeyStress, DynamicRange_MassiveDcOffset) {
    // C Major chord with huge DC offset (+0.95f)
    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 2.0f, 44100, 0.5f);
    for (float& s : cMaj) s += 0.95f;

    auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);
    // DC is bin 0, KeyDetector starts at kMinBin (70Hz), so DC must be completely ignored!
    EXPECT_EQ(res.key, "C");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_GT(res.confidence, 0.5f);
}

// ============================================================================
// Category 10: Multi-Threaded Concurrency Stress
// ============================================================================

TEST(AdversarialKeyStress, Concurrency_MultiThreadedStressCalls) {
    const int numThreads = 8;
    const int callsPerThread = 25;
    std::atomic<int> successCount{0};

    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 1.5f, 44100);
    auto aMin = AudioTestFixtures::generateChordTriad(220.00f, true, 1.5f, 44100);

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < callsPerThread; ++i) {
                if ((t + i) % 2 == 0) {
                    auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);
                    if (res.key == "C" && res.mode == "Major") successCount++;
                } else {
                    auto res = ai::KeyDetector::detect(aMin.data(), aMin.size(), 44100);
                    if (res.key == "A" && res.mode == "Minor") successCount++;
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * callsPerThread);
}

// ============================================================================
// Category 11: Memory Stability & Leak Verification
// ============================================================================

TEST(AdversarialKeyStress, Stability_100IterationsMemoryLeakAudit) {
    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 1.0f, 44100);

    // Warm up
    for (int i = 0; i < 10; ++i) {
        auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);
        (void)res;
    }

    size_t memBefore = getProcessWorkingSetBytes();

    // Execute 100 iterations
    for (int i = 0; i < 100; ++i) {
        auto res = ai::KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);
        if (res.key != "C") {
            failTest(__FILE__, __LINE__, "Detection failed during stability loop");
        }
    }

    size_t memAfter = getProcessWorkingSetBytes();

    // Memory growth should be negligible (< 4 MB working set change)
    if (memBefore > 0 && memAfter > 0) {
        int64_t diff = static_cast<int64_t>(memAfter) - static_cast<int64_t>(memBefore);
        // Allow up to 4MB for heap fragmentation / OS paging
        EXPECT_LT(diff, 4 * 1024 * 1024);
    }
}

} // namespace reals::test
