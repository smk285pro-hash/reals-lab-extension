// ============================================================================
// TestSuite_AdversarialHPCPKeyChallenger.cpp
//
// Empirical Challenger Verification Suite for M1 HPCP Key Detection Engine.
// Stress-tests:
// 1. 24 Chromatic Keys (12 Major, 12 Minor) at A440 reference.
// 2. Detuned Audio (A4 = 432 Hz, 436 Hz, 444 Hz, 448 Hz reference compensation).
// 3. Absence of Systematic Bias toward F Major (Silence, White Noise, Pink Noise).
// 4. Harmonic Polyphony: Chord progressions, rich saw/square waveforms, kick+sub mixes.
// 5. Extreme sample rates (22.05kHz, 48kHz, 96kHz) and sub-Nfft buffer sizes (<4096).
// ============================================================================

#include "../framework/TestRunner.h"
#include <reals/ai/KeyDetector.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace reals::test {

namespace {

constexpr float kPi = 3.14159265358979323846f;

const std::array<std::string, 12> kChromaticNotes = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Synthesize tonal audio at arbitrary A4 reference tuning (e.g. 432Hz, 440Hz, 444Hz)
std::vector<float> synthesizeTunedKeyAudio(
    int rootPitchClass,
    bool isMajor,
    double a4Freq = 440.0,
    float durationSec = 2.5f,
    int sampleRate = 44100)
{
    const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> pcm(totalFrames, 0.0f);

    // Calculate root frequency in octave 4 based on custom A4 reference
    const double rootFreq = a4Freq * std::pow(2.0, (rootPitchClass - 9) / 12.0);

    // Diatonic scale chord weights
    std::vector<int> intervals;
    std::vector<double> weights;
    if (isMajor) {
        intervals = {0, 4, 7, 2, 9, 11};
        weights   = {1.0, 0.8, 0.9, 0.4, 0.4, 0.3};
    } else {
        intervals = {0, 3, 7, 5, 8, 10};
        weights   = {1.0, 0.85, 0.9, 0.5, 0.6, 0.4};
    }

    for (size_t i = 0; i < totalFrames; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double sample = 0.0;
        double weightSum = 0.0;

        for (size_t k = 0; k < intervals.size(); ++k) {
            double noteFreq = rootFreq * std::pow(2.0, intervals[k] / 12.0);
            double w = weights[k];
            // Fundamental
            sample += w * std::sin(2.0 * kPi * noteFreq * t);
            // 2nd harmonic (octave)
            sample += (w * 0.5) * std::sin(2.0 * kPi * (noteFreq * 2.0) * t);
            // 3rd harmonic (octave + 5th)
            sample += (w * 0.25) * std::sin(2.0 * kPi * (noteFreq * 3.0) * t);
            // 4th harmonic (2 octaves)
            sample += (w * 0.125) * std::sin(2.0 * kPi * (noteFreq * 4.0) * t);
            weightSum += w * 1.875;
        }

        if (weightSum > 0.0) {
            sample /= weightSum;
        }
        pcm[i] = static_cast<float>(0.8 * sample);
    }

    return pcm;
}

// Synthesize 4-chord progression: I - V - vi - IV (Major) or i - VI - III - VII (Minor)
std::vector<float> synthesizeChordProgression(
    int rootPitchClass,
    bool isMajor,
    float durationSec = 3.2f,
    int sampleRate = 44100)
{
    const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> pcm(totalFrames, 0.0f);

    const double baseA4 = 440.0;
    const double rootFreq = baseA4 * std::pow(2.0, (rootPitchClass - 9) / 12.0);

    // 4 chords, each playing for 1/4 of the duration
    struct ChordDef {
        int rootOffset;
        std::vector<int> triadIntervals; // semitones relative to chord root
    };

    std::vector<ChordDef> chords;
    if (isMajor) {
        // I (0, maj), V (7, maj), vi (9, min), IV (5, maj)
        chords = {
            {0, {0, 4, 7}},
            {7, {0, 4, 7}},
            {9, {0, 3, 7}},
            {5, {0, 4, 7}}
        };
    } else {
        // i (0, min), VI (8, maj), III (3, maj), VII (10, maj)
        chords = {
            {0, {0, 3, 7}},
            {8, {0, 4, 7}},
            {3, {0, 4, 7}},
            {10, {0, 4, 7}}
        };
    }

    const size_t chordFrames = totalFrames / chords.size();

    for (size_t c = 0; c < chords.size(); ++c) {
        const size_t start = c * chordFrames;
        const size_t end = (c == chords.size() - 1) ? totalFrames : start + chordFrames;
        const double chordRoot = rootFreq * std::pow(2.0, chords[c].rootOffset / 12.0);

        for (size_t i = start; i < end; ++i) {
            double t = static_cast<double>(i - start) / sampleRate;
            double chordSample = 0.0;

            for (int interval : chords[c].triadIntervals) {
                double noteF = chordRoot * std::pow(2.0, interval / 12.0);
                chordSample += 0.33 * std::sin(2.0 * kPi * noteF * t);
                chordSample += 0.15 * std::sin(2.0 * kPi * (noteF * 2.0) * t);
                chordSample += 0.08 * std::sin(2.0 * kPi * (noteF * 3.0) * t);
            }

            // Envelope to avoid click at transition
            double env = 1.0;
            if (i - start < 500) env = static_cast<double>(i - start) / 500.0;
            if (end - i < 500) env = static_cast<double>(end - i) / 500.0;

            pcm[i] = static_cast<float>(chordSample * env);
        }
    }

    return pcm;
}

// Synthesize saw or square harmonic audio with rich overtones up to 16th harmonic
std::vector<float> synthesizeHarmonicWaveform(
    int rootPitchClass,
    bool isMajor,
    bool isSquare,
    float durationSec = 2.0f,
    int sampleRate = 44100)
{
    const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
    std::vector<float> pcm(totalFrames, 0.0f);

    const double rootFreq = 440.0 * std::pow(2.0, (rootPitchClass - 9) / 12.0);
    const std::vector<int> chord = isMajor ? std::vector<int>{0, 4, 7} : std::vector<int>{0, 3, 7};

    for (size_t i = 0; i < totalFrames; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double sample = 0.0;

        for (int interval : chord) {
            double f0 = rootFreq * std::pow(2.0, interval / 12.0);
            for (int h = 1; h <= 12; ++h) {
                if (isSquare && (h % 2 == 0)) continue; // square waves only have odd harmonics
                double hFreq = f0 * h;
                if (hFreq > 10000.0) break;
                double hAmp = 1.0 / static_cast<double>(h);
                sample += 0.15 * hAmp * std::sin(2.0 * kPi * hFreq * t);
            }
        }
        pcm[i] = static_cast<float>(std::clamp(sample, -1.0, 1.0));
    }

    return pcm;
}

} // namespace

// ============================================================================
// Test 1: Full Chromatic 24-Key Accuracy Verification at Standard A440
// ============================================================================

TEST(AdversarialHPCP, Benchmark_All24Keys_ExactVerification) {
    int passed = 0;

    for (int modeIdx = 0; modeIdx < 2; ++modeIdx) {
        const bool isMajor = (modeIdx == 0);
        const std::string modeStr = isMajor ? "Major" : "Minor";

        for (int r = 0; r < 12; ++r) {
            const std::string rootName = kChromaticNotes[r];
            auto pcm = synthesizeTunedKeyAudio(r, isMajor, 440.0, 3.0f, 44100);
            auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), 44100);

            if (res.key == rootName && res.mode == modeStr) {
                ++passed;
            } else {
                std::cout << "[CHALLENGER ADVERSARIAL] Mismatch on " << rootName << " " << modeStr
                          << " -> Detected: " << res.key << " " << res.mode
                          << " (conf: " << res.confidence << ")\n";
            }
        }
    }

    double accuracyPct = (static_cast<double>(passed) / 24.0) * 100.0;
    std::cout << "[CHALLENGER VERDICT] A440 Standard 24-Key Accuracy: "
              << passed << "/24 (" << std::fixed << std::setprecision(1) << accuracyPct << "%)\n";

    EXPECT_GE(accuracyPct, 85.0);
}

// ============================================================================
// Test 2: Detuned Audio Stress Test (A4 = 432 Hz, -31.8 Cents Deviation)
// ============================================================================

TEST(AdversarialHPCP, DetunedAudio_A432Hz_All24Keys) {
    int passed = 0;

    for (int modeIdx = 0; modeIdx < 2; ++modeIdx) {
        const bool isMajor = (modeIdx == 0);
        const std::string modeStr = isMajor ? "Major" : "Minor";

        for (int r = 0; r < 12; ++r) {
            const std::string rootName = kChromaticNotes[r];
            // Detune to 432 Hz (-31.77 cents)
            auto pcm = synthesizeTunedKeyAudio(r, isMajor, 432.0, 2.5f, 44100);
            auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), 44100);

            if (res.key == rootName && res.mode == modeStr) {
                ++passed;
            } else {
                std::cout << "[CHALLENGER DETUNED 432Hz] Mismatch on " << rootName << " " << modeStr
                          << " -> Detected: " << res.key << " " << res.mode
                          << " (conf: " << res.confidence << ")\n";
            }
        }
    }

    double accuracyPct = (static_cast<double>(passed) / 24.0) * 100.0;
    std::cout << "[CHALLENGER VERDICT] A432 Detuned 24-Key Accuracy: "
              << passed << "/24 (" << std::fixed << std::setprecision(1) << accuracyPct << "%)\n";

    EXPECT_GE(accuracyPct, 85.0);
}

// ============================================================================
// Test 3: Detuned Audio Stress Test (A4 = 444 Hz, +15.7 Cents Deviation)
// ============================================================================

TEST(AdversarialHPCP, DetunedAudio_A444Hz_All24Keys) {
    int passed = 0;

    for (int modeIdx = 0; modeIdx < 2; ++modeIdx) {
        const bool isMajor = (modeIdx == 0);
        const std::string modeStr = isMajor ? "Major" : "Minor";

        for (int r = 0; r < 12; ++r) {
            const std::string rootName = kChromaticNotes[r];
            // Detune to 444 Hz (+15.67 cents)
            auto pcm = synthesizeTunedKeyAudio(r, isMajor, 444.0, 2.5f, 44100);
            auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), 44100);

            if (res.key == rootName && res.mode == modeStr) {
                ++passed;
            } else {
                std::cout << "[CHALLENGER DETUNED 444Hz] Mismatch on " << rootName << " " << modeStr
                          << " -> Detected: " << res.key << " " << res.mode
                          << " (conf: " << res.confidence << ")\n";
            }
        }
    }

    double accuracyPct = (static_cast<double>(passed) / 24.0) * 100.0;
    std::cout << "[CHALLENGER VERDICT] A444 Detuned 24-Key Accuracy: "
              << passed << "/24 (" << std::fixed << std::setprecision(1) << accuracyPct << "%)\n";

    EXPECT_GE(accuracyPct, 85.0);
}

// ============================================================================
// Test 4: Systematic Bias Evaluation — Silence, White Noise, Pink Noise
// ============================================================================

TEST(AdversarialHPCP, SystematicBias_NoiseAndSilence_NoFMajorArtifact) {
    // 1. Digital silence: MUST return low confidence (default fallback) and NOT falsely claim F Major
    std::vector<float> silence(44100 * 2, 0.0f);
    auto resSilence = ai::KeyDetector::detect(silence.data(), silence.size(), 44100);
    EXPECT_NE(resSilence.key, "F"); // Must NOT be F Major artifact!
    EXPECT_LE(resSilence.confidence, 0.5f);

    // 2. White noise across 10 distinct pseudo-random seeds
    int fMajorWhiteCount = 0;
    std::map<std::string, int> whiteKeyDistribution;

    for (uint32_t seed = 100; seed < 110; ++seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        std::vector<float> whiteNoise(44100 * 2);
        for (float& s : whiteNoise) s = dist(rng);

        auto res = ai::KeyDetector::detect(whiteNoise.data(), whiteNoise.size(), 44100);
        whiteKeyDistribution[res.key + " " + res.mode]++;
        if (res.key == "F" && res.mode == "Major") {
            fMajorWhiteCount++;
        }
    }

    std::cout << "[CHALLENGER BIAS] White noise detections across 10 random seeds:\n";
    for (const auto& [k, count] : whiteKeyDistribution) {
        std::cout << "  - " << k << ": " << count << "\n";
    }
    // With 10 seeds, F Major must not dominate (threshold <= 2)
    EXPECT_LE(fMajorWhiteCount, 2);

    // 3. Pink noise (1/f filter approximation)
    int fMajorPinkCount = 0;
    for (uint32_t seed = 200; seed < 205; ++seed) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        std::vector<float> pinkNoise(44100 * 2, 0.0f);
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
        for (float& s : pinkNoise) {
            float white = dist(rng);
            b0 = 0.99765f * b0 + white * 0.0990460f;
            b1 = 0.96300f * b1 + white * 0.1606432f;
            b2 = 0.57000f * b2 + white * 0.4426590f;
            s = (b0 + b1 + b2 + white * 0.5362f) * 0.15f;
        }

        auto res = ai::KeyDetector::detect(pinkNoise.data(), pinkNoise.size(), 44100);
        if (res.key == "F" && res.mode == "Major") {
            fMajorPinkCount++;
        }
    }

    std::cout << "[CHALLENGER BIAS] Pink noise F Major count: " << fMajorPinkCount << " / 5\n";
    EXPECT_LE(fMajorPinkCount, 1);
}

// ============================================================================
// Test 5: Polyphonic Harmonic Progression Handling (I-V-vi-IV and i-VI-III-VII)
// ============================================================================

TEST(AdversarialHPCP, PolyphonicProgressions_MajorAndMinor) {
    // Test realistic chord progressions across varied musical keys
    const std::vector<std::pair<int, bool>> testKeys = {
        {0, true},   // C Major (C - G - Am - F)
        {7, true},   // G Major (G - D - Em - C)
        {2, true},   // D Major (D - A - Bm - G)
        {9, false},  // A Minor (Am - F - C - G) -> Relative: C Major
        {4, false},  // E Minor (Em - C - G - D) -> Relative: G Major
        {0, false},  // C Minor (Cm - Ab - Eb - Bb) -> Relative: D# Major
    };

    int exactMatches = 0;
    int harmonicallyCompatible = 0;

    for (const auto& [root, isMajor] : testKeys) {
        const std::string expectedRoot = kChromaticNotes[root];
        const std::string expectedMode = isMajor ? "Major" : "Minor";

        // Relative major/minor: +3 semitones for minor -> relative major; -3 semitones for major -> relative minor
        const int relPitch = isMajor ? (root - 3 + 12) % 12 : (root + 3) % 12;
        const std::string relRoot = kChromaticNotes[relPitch];
        const std::string relMode = isMajor ? "Minor" : "Major";

        auto pcm = synthesizeChordProgression(root, isMajor, 3.2f, 44100);
        auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), 44100);

        const bool isExact = (res.key == expectedRoot && res.mode == expectedMode);
        const bool isRelative = (res.key == relRoot && res.mode == relMode);

        if (isExact) ++exactMatches;
        if (isExact || isRelative) ++harmonicallyCompatible;

        std::cout << "[CHALLENGER PROGRESSION] Key: " << expectedRoot << " " << expectedMode
                  << " -> Detected: " << res.key << " " << res.mode
                  << (isExact ? " [EXACT MATCH]" : (isRelative ? " [RELATIVE KEY MATCH]" : " [MISMATCH]"))
                  << " (conf: " << std::fixed << std::setprecision(2) << res.confidence << ")\n";
    }

    double exactPct = (static_cast<double>(exactMatches) / testKeys.size()) * 100.0;
    double compatPct = (static_cast<double>(harmonicallyCompatible) / testKeys.size()) * 100.0;

    std::cout << "[CHALLENGER VERDICT] Chord Progression Exact Accuracy: "
              << exactMatches << "/" << testKeys.size() << " (" << exactPct << "%)\n";
    std::cout << "[CHALLENGER VERDICT] Chord Progression MIREX Harmonic Compatibility: "
              << harmonicallyCompatible << "/" << testKeys.size() << " (" << compatPct << "%)\n";

    // All major progressions must match exactly; all minor progressions must match tonic or relative major
    EXPECT_GE(compatPct, 100.0);
    EXPECT_GE(exactMatches, 3); // 100% of major progressions match exact root
}

// ============================================================================
// Test 6: Dense Harmonic Waveforms (Square & Sawtooth Overtones)
// ============================================================================

TEST(AdversarialHPCP, DenseHarmonicWaveforms_SawAndSquare) {
    // Tests rich harmonic spectra with up to 12 overtones
    const std::vector<std::pair<int, bool>> testKeys = {
        {0, true},  // C Major
        {5, true},  // F Major
        {9, false}, // A Minor
        {2, false}, // D Minor
    };

    int correct = 0;
    int total = 0;

    for (bool isSquare : {false, true}) {
        for (const auto& [root, isMajor] : testKeys) {
            ++total;
            const std::string expectedRoot = kChromaticNotes[root];
            const std::string expectedMode = isMajor ? "Major" : "Minor";

            auto pcm = synthesizeHarmonicWaveform(root, isMajor, isSquare, 2.0f, 44100);
            auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), 44100);

            if (res.key == expectedRoot && res.mode == expectedMode) {
                ++correct;
            } else {
                std::cout << "[CHALLENGER WAVEFORM " << (isSquare ? "SQUARE" : "SAW") << "] Expected: "
                          << expectedRoot << " " << expectedMode
                          << " -> Detected: " << res.key << " " << res.mode
                          << " (conf: " << res.confidence << ")\n";
            }
        }
    }

    double pct = (static_cast<double>(correct) / total) * 100.0;
    std::cout << "[CHALLENGER VERDICT] Dense Harmonic Waveform Accuracy: "
              << correct << "/" << total << " (" << pct << "%)\n";

    EXPECT_GE(pct, 85.0);
}

// ============================================================================
// Test 7: Non-Standard Sample Rates & Buffer Edge Cases
// ============================================================================

TEST(AdversarialHPCP, ExtremeSampleRatesAndShortDurations) {
    // 1. Resampling checks across 48kHz, 96kHz, 22.05kHz
    const std::vector<int> sampleRates = {22050, 48000, 96000};
    for (int sr : sampleRates) {
        // C Major (root 0)
        auto pcm = synthesizeTunedKeyAudio(0, true, 440.0, 2.0f, sr);
        auto res = ai::KeyDetector::detect(pcm.data(), pcm.size(), sr);
        EXPECT_EQ(res.key, "C");
        EXPECT_EQ(res.mode, "Major");
    }

    // 2. Very short audio clips (smaller than kNfft = 4096 frames)
    // 0.05 seconds at 44.1kHz = 2205 frames < 4096
    auto shortPcm = synthesizeTunedKeyAudio(9, false, 440.0, 0.05f, 44100); // A Minor
    auto resShort = ai::KeyDetector::detect(shortPcm.data(), shortPcm.size(), 44100);
    // Detector should zero-pad gracefully without crashing or returning NaN
    EXPECT_FALSE(std::isnan(resShort.confidence));
    EXPECT_FALSE(resShort.key.empty());
    EXPECT_FALSE(resShort.mode.empty());
    EXPECT_FALSE(resShort.camelot.empty());

    // 3. Null / empty safety
    auto resNull = ai::KeyDetector::detect(nullptr, 0, 44100);
    EXPECT_EQ(resNull.confidence, 0.0f);
    EXPECT_EQ(resNull.key, "C");
    EXPECT_EQ(resNull.mode, "Major");
}

} // namespace reals::test
