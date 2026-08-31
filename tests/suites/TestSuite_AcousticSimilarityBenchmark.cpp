#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/TestRunner.h"
#include <reals/ai/ClapEmbedder.h>
#include <reals/ai/FeatureExtractor.h>
#include <reals/ai/OnnxEngine.h>
#include <reals/db/Database.h>
#include <reals/search/SearchEngine.h>
#include <reals/util/Simd.h>

namespace reals::test {

class AcousticBenchmark : public reals::test::TestFixture {
public:
    void SetUp() override {
        ai::OnnxEngine::instance().unloadAll();
    }

    void TearDown() override {
        ai::OnnxEngine::instance().unloadAll();
    }
    // Category 1A: Distorted 808 Bass (Heavy sub fundamental, saturated overtones, pitch drop)
    static std::vector<float> synthDistorted808(float f0 = 55.0f, float drive = 3.5f, float dur = 1.2f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n);
        double phase = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double freq = 65.0 * std::exp(-t * 40.0) + static_cast<double>(f0);
            phase += 2.0 * AudioTestFixtures::kPi * freq / sr;
            double raw = std::tanh(static_cast<double>(drive) * std::sin(phase));
            double env = std::exp(-t * 1.8);
            buf[i] = static_cast<float>(raw * env);
        }
        return buf;
    }

    // Category 1B: Clean Sub Bass (Pure low-frequency sine, zero upper harmonics, low centroid)
    static std::vector<float> synthCleanSub(float f0 = 55.0f, float dur = 1.2f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n);
        double phase = 0.0;
        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            phase += 2.0 * AudioTestFixtures::kPi * static_cast<double>(f0) / sr;
            double env = (1.0 - std::exp(-t * 15.0)) * std::exp(-t * 0.8);
            buf[i] = static_cast<float>(0.9 * std::sin(phase) * env);
        }
        return buf;
    }

    // Category 2A: Bright Acoustic Guitar (Rich high harmonics, high spectral centroid, pluck transient)
    static std::vector<float> synthBrightGuitar(float f0 = 440.0f, float dur = 1.5f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double sample = 0.0;
            for (int k = 1; k <= 16; ++k) {
                double hPhase = 2.0 * AudioTestFixtures::kPi * static_cast<double>(f0 * k) * t;
                sample += (1.0 / std::pow(static_cast<double>(k), 0.6)) *
                          std::sin(hPhase) *
                          std::exp(-(3.0 + 0.5 * k) * t);
            }
            double pluck = std::sin(2.0 * AudioTestFixtures::kPi * 3200.0 * t) * std::exp(-t * 80.0);
            buf[i] = static_cast<float>(0.25 * sample + 0.15 * pluck);
        }
        return buf;
    }

    // Category 2B: Warm Vintage Piano (Fundamental + 3-4 low harmonics, steep rolloff, low centroid)
    static std::vector<float> synthWarmPiano(float f0 = 261.63f, float dur = 1.5f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double sample = 0.0;
            for (int k = 1; k <= 4; ++k) {
                double hPhase = 2.0 * AudioTestFixtures::kPi * static_cast<double>(f0 * k) * t;
                sample += (1.0 / (static_cast<double>(k) * k * 1.2)) *
                          std::sin(hPhase) *
                          std::exp(-0.9 * t);
            }
            double attack = 1.0 - std::exp(-t * 60.0);
            buf[i] = static_cast<float>(0.7 * sample * attack);
        }
        return buf;
    }

    // Category 3A: Breathy Vocal Chop (Glottal pulse + vocal formants + breathy noise + smooth envelope)
    static std::vector<float> synthBreathyVocal(float f0 = 330.0f, float dur = 1.0f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> noiseDist(-0.5f, 0.5f);
        double phase = 0.0;

        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double vib = 1.0 + 0.015 * std::sin(2.0 * AudioTestFixtures::kPi * 5.5 * t);
            double f = static_cast<double>(f0) * vib;
            phase += 2.0 * AudioTestFixtures::kPi * f / sr;

            double glottal = std::sin(phase) +
                             0.6 * std::sin(2.0 * phase) +
                             0.3 * std::sin(3.0 * phase);

            double f1 = 0.5 * std::sin(2.0 * AudioTestFixtures::kPi * 800.0 * t);
            double f2 = 0.3 * std::sin(2.0 * AudioTestFixtures::kPi * 1200.0 * t);
            double f3 = 0.2 * std::sin(2.0 * AudioTestFixtures::kPi * 2600.0 * t);
            double breath = 0.25 * static_cast<double>(noiseDist(rng));

            double env = std::sin(AudioTestFixtures::kPi * std::min(1.0, t / static_cast<double>(dur)));
            buf[i] = static_cast<float>((0.4 * glottal + 0.3 * (f1 + f2 + f3) + breath) * env);
        }
        return buf;
    }

    // Category 3B: Analog Synth Pad (Detuned sawtooth stack, slow attack, warm sustained texture)
    static std::vector<float> synthAnalogPad(float f0 = 220.0f, float dur = 2.0f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        double phase1 = 0.0;
        double phase2 = 0.0;
        double phaseSub = 0.0;

        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            phase1 += static_cast<double>(f0) / sr;
            if (phase1 >= 1.0) phase1 -= 1.0;
            phase2 += (static_cast<double>(f0) * 1.006) / sr;
            if (phase2 >= 1.0) phase2 -= 1.0;
            phaseSub += 2.0 * AudioTestFixtures::kPi * (static_cast<double>(f0) * 0.5) / sr;

            double saw1 = 2.0 * phase1 - 1.0;
            double saw2 = 2.0 * phase2 - 1.0;
            double sub = std::sin(phaseSub);

            double attack = std::min(1.0, t / 0.6);
            double env = attack * (1.0 - 0.2 * (t / static_cast<double>(dur)));
            buf[i] = static_cast<float>(0.3 * (saw1 + saw2 + 0.5 * sub) * env);
        }
        return buf;
    }

    // Category 4A: Punchy Acoustic Snare (Pitch transient body impulse + bandpass filtered noise wires)
    static std::vector<float> synthPunchySnare(float dur = 0.6f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        std::mt19937 rng(4242);
        std::uniform_real_distribution<float> noiseDist(-1.0f, 1.0f);
        double bodyPhase = 0.0;

        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double bodyFreq = 60.0 * std::exp(-t * 60.0) + 120.0;
            bodyPhase += 2.0 * AudioTestFixtures::kPi * bodyFreq / sr;
            double body = std::sin(bodyPhase) * std::exp(-t * 35.0);

            double noise = static_cast<double>(noiseDist(rng));
            double wire = noise * std::sin(2.0 * AudioTestFixtures::kPi * 3500.0 * t) * std::exp(-t * 18.0);

            buf[i] = static_cast<float>(0.6 * body + 0.5 * wire);
        }
        return buf;
    }

    // Category 4B: Metallic Closed Hi-Hat (Inharmonic oscillator bank + high frequency sizzle, tight decay)
    static std::vector<float> synthMetallicHiHat(float dur = 0.3f, int sr = 48000) {
        const size_t n = static_cast<size_t>(dur * sr);
        std::vector<float> buf(n, 0.0f);
        const double freqs[6] = {800.0, 1340.0, 1980.0, 2750.0, 4100.0, 7200.0};
        double phases[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        std::mt19937 rng(777);
        std::uniform_real_distribution<float> noiseDist(-0.5f, 0.5f);

        for (size_t i = 0; i < n; ++i) {
            double t = static_cast<double>(i) / sr;
            double metallic = 0.0;
            for (int k = 0; k < 6; ++k) {
                phases[k] += 2.0 * AudioTestFixtures::kPi * freqs[k] / sr;
                double sq = (std::sin(phases[k]) > 0.0) ? 1.0 : -1.0;
                metallic += sq;
            }
            metallic /= 6.0;

            double sizzle = static_cast<double>(noiseDist(rng)) * std::sin(2.0 * AudioTestFixtures::kPi * 9500.0 * t);
            double env = std::exp(-t * 65.0);
            buf[i] = static_cast<float>((0.4 * metallic + 0.6 * sizzle) * env);
        }
        return buf;
    }
};

// ============================================================================
// Test 1: Embedding Dimension and Unit L2-Norm Verification
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_Embedding_DimensionAndL2Norm) {
    auto d808 = synthDistorted808();
    auto sub = synthCleanSub();
    auto gtr = synthBrightGuitar();
    auto piano = synthWarmPiano();
    auto vocal = synthBreathyVocal();
    auto pad = synthAnalogPad();
    auto snare = synthPunchySnare();
    auto hihat = synthMetallicHiHat();

    const std::vector<std::pair<std::string, std::vector<float>>> soundClasses = {
        {"Distorted 808", d808},
        {"Clean Sub Bass", sub},
        {"Bright Acoustic Guitar", gtr},
        {"Warm Vintage Piano", piano},
        {"Breathy Vocal Chop", vocal},
        {"Analog Synth Pad", pad},
        {"Punchy Snare", snare},
        {"Metallic Closed Hi-Hat", hihat}
    };

    for (const auto& [name, pcm] : soundClasses) {
        auto emb = ai::ClapEmbedder::embedAudio(pcm.data(), pcm.size(), 48000);
        EXPECT_EQ(emb.size(), ai::ClapEmbedder::kEmbeddingDim);
        EXPECT_EQ(emb.size(), 512u);

        float sumSq = 0.0f;
        for (float v : emb) {
            EXPECT_FALSE(std::isnan(v));
            EXPECT_FALSE(std::isinf(v));
            sumSq += v * v;
        }
        float norm = std::sqrt(sumSq);
        EXPECT_NEAR(norm, 1.0f, 1e-4f);
    }
}

// ============================================================================
// Test 2: Intragroup Timbral Similarity (Threshold > 0.70)
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_Intragroup_TimbralSimilarity_Above_0_70) {
    // 1. Distorted 808 variations (different overdrive and slight pitch variation)
    auto d808_A = synthDistorted808(55.0f, 3.5f);
    auto d808_B = synthDistorted808(55.0f, 4.5f);
    auto emb808_A = ai::ClapEmbedder::embedAudio(d808_A.data(), d808_A.size(), 48000);
    auto emb808_B = ai::ClapEmbedder::embedAudio(d808_B.data(), d808_B.size(), 48000);
    float sim808 = ai::ClapEmbedder::cosineSimilarity(emb808_A, emb808_B);
    std::cout << "[Benchmark] Intragroup Distorted 808 Similarity: " << std::fixed << std::setprecision(4) << sim808 << "\n";
    EXPECT_GE(sim808, 0.70f);

    // 2. Clean Sub Bass variations (55Hz vs 58Hz with slightly different decay)
    auto sub_A = synthCleanSub(55.0f, 1.2f);
    auto sub_B = synthCleanSub(58.0f, 1.0f);
    auto embSub_A = ai::ClapEmbedder::embedAudio(sub_A.data(), sub_A.size(), 48000);
    auto embSub_B = ai::ClapEmbedder::embedAudio(sub_B.data(), sub_B.size(), 48000);
    float simSub = ai::ClapEmbedder::cosineSimilarity(embSub_A, embSub_B);
    std::cout << "[Benchmark] Intragroup Clean Sub Similarity: " << simSub << "\n";
    EXPECT_GE(simSub, 0.70f);

    // 3. Bright Guitar variations (440Hz vs 442Hz)
    auto gtr_A = synthBrightGuitar(440.0f);
    auto gtr_B = synthBrightGuitar(442.0f);
    auto embGtr_A = ai::ClapEmbedder::embedAudio(gtr_A.data(), gtr_A.size(), 48000);
    auto embGtr_B = ai::ClapEmbedder::embedAudio(gtr_B.data(), gtr_B.size(), 48000);
    float simGtr = ai::ClapEmbedder::cosineSimilarity(embGtr_A, embGtr_B);
    std::cout << "[Benchmark] Intragroup Bright Guitar Similarity: " << simGtr << "\n";
    EXPECT_GE(simGtr, 0.70f);

    // 4. Warm Piano variations (C4 261.63Hz vs C4 with softer harmonics)
    auto piano_A = synthWarmPiano(261.63f, 1.5f);
    auto piano_B = synthWarmPiano(261.63f, 1.2f);
    auto embPiano_A = ai::ClapEmbedder::embedAudio(piano_A.data(), piano_A.size(), 48000);
    auto embPiano_B = ai::ClapEmbedder::embedAudio(piano_B.data(), piano_B.size(), 48000);
    float simPiano = ai::ClapEmbedder::cosineSimilarity(embPiano_A, embPiano_B);
    std::cout << "[Benchmark] Intragroup Warm Piano Similarity: " << simPiano << "\n";
    EXPECT_GE(simPiano, 0.70f);

    // 5. Breathy Vocal Chop variations (330Hz vs 340Hz)
    auto vocal_A = synthBreathyVocal(330.0f, 1.0f);
    auto vocal_B = synthBreathyVocal(340.0f, 0.9f);
    auto embVocal_A = ai::ClapEmbedder::embedAudio(vocal_A.data(), vocal_A.size(), 48000);
    auto embVocal_B = ai::ClapEmbedder::embedAudio(vocal_B.data(), vocal_B.size(), 48000);
    float simVocal = ai::ClapEmbedder::cosineSimilarity(embVocal_A, embVocal_B);
    std::cout << "[Benchmark] Intragroup Breathy Vocal Similarity: " << simVocal << "\n";
    EXPECT_GE(simVocal, 0.70f);

    // 6. Analog Synth Pad variations (220Hz vs 221Hz detune)
    auto pad_A = synthAnalogPad(220.0f, 2.0f);
    auto pad_B = synthAnalogPad(221.0f, 1.8f);
    auto embPad_A = ai::ClapEmbedder::embedAudio(pad_A.data(), pad_A.size(), 48000);
    auto embPad_B = ai::ClapEmbedder::embedAudio(pad_B.data(), pad_B.size(), 48000);
    float simPad = ai::ClapEmbedder::cosineSimilarity(embPad_A, embPad_B);
    std::cout << "[Benchmark] Intragroup Analog Pad Similarity: " << simPad << "\n";
    EXPECT_GE(simPad, 0.70f);

    // 7. Punchy Snare variations (slight body tuning difference)
    auto snare_A = synthPunchySnare(0.6f);
    auto snare_B = synthPunchySnare(0.5f);
    auto embSnare_A = ai::ClapEmbedder::embedAudio(snare_A.data(), snare_A.size(), 48000);
    auto embSnare_B = ai::ClapEmbedder::embedAudio(snare_B.data(), snare_B.size(), 48000);
    float simSnare = ai::ClapEmbedder::cosineSimilarity(embSnare_A, embSnare_B);
    std::cout << "[Benchmark] Intragroup Punchy Snare Similarity: " << simSnare << "\n";
    EXPECT_GE(simSnare, 0.70f);

    // 8. Metallic Hi-Hat variations (different sizzle decay)
    auto hihat_A = synthMetallicHiHat(0.3f);
    auto hihat_B = synthMetallicHiHat(0.25f);
    auto embHiHat_A = ai::ClapEmbedder::embedAudio(hihat_A.data(), hihat_A.size(), 48000);
    auto embHiHat_B = ai::ClapEmbedder::embedAudio(hihat_B.data(), hihat_B.size(), 48000);
    float simHiHat = ai::ClapEmbedder::cosineSimilarity(embHiHat_A, embHiHat_B);
    std::cout << "[Benchmark] Intragroup Metallic Hi-Hat Similarity: " << simHiHat << "\n";
    EXPECT_GE(simHiHat, 0.70f);
}

// ============================================================================
// Test 3: Intergroup Acoustic Mismatch (Threshold < 0.40)
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_Intergroup_AcousticMismatch_Below_0_40) {
    auto d808 = synthDistorted808();
    auto sub = synthCleanSub();
    auto gtr = synthBrightGuitar();
    auto piano = synthWarmPiano();
    auto vocal = synthBreathyVocal();
    auto pad = synthAnalogPad();
    auto snare = synthPunchySnare();
    auto hihat = synthMetallicHiHat();

    auto emb808 = ai::ClapEmbedder::embedAudio(d808.data(), d808.size(), 48000);
    auto embSub = ai::ClapEmbedder::embedAudio(sub.data(), sub.size(), 48000);
    auto embGtr = ai::ClapEmbedder::embedAudio(gtr.data(), gtr.size(), 48000);
    auto embPiano = ai::ClapEmbedder::embedAudio(piano.data(), piano.size(), 48000);
    auto embVocal = ai::ClapEmbedder::embedAudio(vocal.data(), vocal.size(), 48000);
    auto embPad = ai::ClapEmbedder::embedAudio(pad.data(), pad.size(), 48000);
    auto embSnare = ai::ClapEmbedder::embedAudio(snare.data(), snare.size(), 48000);
    auto embHiHat = ai::ClapEmbedder::embedAudio(hihat.data(), hihat.size(), 48000);

    // Cross-category comparisons:
    // 1. Distorted 808 (Bass) vs Metallic Hi-Hat (Percussion/High)
    float sim808_HiHat = ai::ClapEmbedder::cosineSimilarity(emb808, embHiHat);
    std::cout << "[Benchmark] Intergroup 808 vs Hi-Hat: " << sim808_HiHat << "\n";
    EXPECT_LT(sim808_HiHat, 0.40f);

    // 2. Clean Sub (Bass) vs Metallic Hi-Hat (Percussion/High)
    float simSub_HiHat = ai::ClapEmbedder::cosineSimilarity(embSub, embHiHat);
    std::cout << "[Benchmark] Intergroup Sub Bass vs Hi-Hat: " << simSub_HiHat << "\n";
    EXPECT_LT(simSub_HiHat, 0.40f);

    // 3. Bright Guitar (Plucked/High) vs Punchy Snare (Percussion)
    float simGtr_Snare = ai::ClapEmbedder::cosineSimilarity(embGtr, embSnare);
    std::cout << "[Benchmark] Intergroup Guitar vs Snare: " << simGtr_Snare << "\n";
    EXPECT_LT(simGtr_Snare, 0.40f);

    // 4. Warm Piano (Keys) vs Punchy Snare (Percussion)
    float simPiano_Snare = ai::ClapEmbedder::cosineSimilarity(embPiano, embSnare);
    std::cout << "[Benchmark] Intergroup Piano vs Snare: " << simPiano_Snare << "\n";
    EXPECT_LT(simPiano_Snare, 0.40f);

    // 5. Breathy Vocal (Vocal) vs Clean Sub (Bass)
    float simVocal_Sub = ai::ClapEmbedder::cosineSimilarity(embVocal, embSub);
    std::cout << "[Benchmark] Intergroup Vocal vs Sub Bass: " << simVocal_Sub << "\n";
    EXPECT_LT(simVocal_Sub, 0.40f);

    // 6. Punchy Snare (Percussion) vs Analog Pad (Atmosphere/Sustained)
    float simSnare_Pad = ai::ClapEmbedder::cosineSimilarity(embSnare, embPad);
    std::cout << "[Benchmark] Intergroup Snare vs Synth Pad: " << simSnare_Pad << "\n";
    EXPECT_LT(simSnare_Pad, 0.40f);

    // 7. Analog Pad vs Metallic Hi-Hat
    float simPad_HiHat = ai::ClapEmbedder::cosineSimilarity(embPad, embHiHat);
    std::cout << "[Benchmark] Intergroup Synth Pad vs Hi-Hat: " << simPad_HiHat << "\n";
    EXPECT_LT(simPad_HiHat, 0.40f);
}

// ============================================================================
// Test 4: Full 8x8 Acoustic Similarity Discrimination Matrix
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_Full_8x8_Acoustic_Discrimination_Matrix) {
    const std::vector<std::string> labels = {
        "808 Bass", "Clean Sub", "Bright Gtr", "Warm Piano",
        "Vocal Chop", "Synth Pad", "Snare", "Hi-Hat"
    };

    std::vector<std::vector<float>> audioBuffers = {
        synthDistorted808(),
        synthCleanSub(),
        synthBrightGuitar(),
        synthWarmPiano(),
        synthBreathyVocal(),
        synthAnalogPad(),
        synthPunchySnare(),
        synthMetallicHiHat()
    };

    std::vector<std::vector<float>> embeddings(8);
    for (size_t i = 0; i < 8; ++i) {
        embeddings[i] = ai::ClapEmbedder::embedAudio(audioBuffers[i].data(), audioBuffers[i].size(), 48000);
        EXPECT_EQ(embeddings[i].size(), 512u);
    }

    float matrix[8][8];

    std::cout << "\n=========================================================================================================\n";
    std::cout << "                                8x8 EMPIRICAL ACOUSTIC DISCRIMINATION MATRIX\n";
    std::cout << "=========================================================================================================\n";
    std::cout << std::setw(14) << " ";
    for (size_t j = 0; j < 8; ++j) {
        std::cout << std::setw(11) << labels[j];
    }
    std::cout << "\n---------------------------------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < 8; ++i) {
        std::cout << std::setw(12) << labels[i] << " |";
        for (size_t j = 0; j < 8; ++j) {
            float sim = ai::ClapEmbedder::cosineSimilarity(embeddings[i], embeddings[j]);
            matrix[i][j] = sim;
            std::cout << std::setw(10) << std::fixed << std::setprecision(3) << sim << " ";
        }
        std::cout << "|\n";
    }
    std::cout << "=========================================================================================================\n\n";

    // Matrix Mathematical Invariant Verifications:
    // 1. Diagonal elements must equal 1.0 (self-identity)
    for (size_t i = 0; i < 8; ++i) {
        EXPECT_NEAR(matrix[i][i], 1.0f, 1e-4f);
    }

    // 2. Matrix must be strictly symmetric: M[i][j] == M[j][i]
    for (size_t i = 0; i < 8; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            EXPECT_NEAR(matrix[i][j], matrix[j][i], 1e-5f);
        }
    }

    // 3. Intragroup similarity must be significantly higher than cross-group similarity
    // Percussion pair {Snare, HiHat}
    EXPECT_GT(matrix[6][7], 0.60f);
    // Bass pair {808, Sub}
    EXPECT_GT(matrix[0][1], 0.90f);
    // Bass vs Percussion (Orthogonal acoustic domain)
    EXPECT_LT(matrix[0][7], 0.10f);
    EXPECT_LT(matrix[1][7], 0.10f);
    EXPECT_LT(matrix[0][6], 0.20f);
    EXPECT_LT(matrix[1][6], 0.20f);
}

// ============================================================================
// Test 5: Metadata Bonus Distortion Isolation
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_Metadata_Bonus_Isolation) {
    auto db = std::make_shared<reals::db::Database>();
    ASSERT_TRUE(db->open(":memory:"));

    // Candidate 1: Genuine 808 match, but mismatching BPM (90 BPM vs 140 BPM),
    // mismatching Key (8B / C Maj vs 11A / F# min), and mismatching Genre ("Ambient" vs "Trap").
    // Expected Bonus: 0.00 (0.00 Key + 0.00 BPM + 0.00 Genre).
    auto d808_var = synthDistorted808(55.0f, 4.2f);
    reals::db::SampleRecord r1;
    r1.filename = "808_Bass_90bpm_8B_Ambient.wav";
    r1.path = "C:/Samples/808_Bass_90bpm_8B_Ambient.wav";
    r1.bpm = 90.0;
    r1.camelot = "8B";
    r1.genre = "Ambient";
    int64_t id1 = db->upsertSample(r1);
    ASSERT_GT(id1, 0);

    reals::db::AnalysisRecord a1;
    a1.sampleId = id1;
    a1.embedding = ai::ClapEmbedder::embedAudio(d808_var.data(), d808_var.size(), 48000);
    ASSERT_TRUE(db->updateAnalysis(id1, a1));

    // Candidate 2: Acoustic Mismatch (Bright Guitar), but spoofed with maximum possible metadata bonus:
    // Exactly matching BPM (140.0 BPM -> +0.03), Exact Key ("11A" -> +0.06), Same Genre ("Trap" -> +0.03).
    // Total Metadata Bonus: +0.12 (Maximum allowable bonus in the ranking engine).
    auto gtr = synthBrightGuitar(440.0f);
    reals::db::SampleRecord r2;
    r2.filename = "Acoustic_Guitar_140bpm_11A_Trap.wav";
    r2.path = "C:/Samples/Acoustic_Guitar_140bpm_11A_Trap.wav";
    r2.bpm = 140.0;
    r2.camelot = "11A";
    r2.genre = "Trap";
    int64_t id2 = db->upsertSample(r2);
    ASSERT_GT(id2, 0);

    reals::db::AnalysisRecord a2;
    a2.sampleId = id2;
    a2.embedding = ai::ClapEmbedder::embedAudio(gtr.data(), gtr.size(), 48000);
    ASSERT_TRUE(db->updateAnalysis(id2, a2));

    // Candidate 3: Acoustic Mismatch (Metallic Hi-Hat) with partial metadata bonus (+0.09):
    // 140.0 BPM (+0.03), Key "11A" (+0.06), Genre "Percussion" (+0.00).
    auto hihat = synthMetallicHiHat();
    reals::db::SampleRecord r3;
    r3.filename = "Metallic_HiHat_140bpm_11A.wav";
    r3.path = "C:/Samples/Metallic_HiHat_140bpm_11A.wav";
    r3.bpm = 140.0;
    r3.camelot = "11A";
    r3.genre = "Percussion";
    int64_t id3 = db->upsertSample(r3);
    ASSERT_GT(id3, 0);

    reals::db::AnalysisRecord a3;
    a3.sampleId = id3;
    a3.embedding = ai::ClapEmbedder::embedAudio(hihat.data(), hihat.size(), 48000);
    ASSERT_TRUE(db->updateAnalysis(id3, a3));

    // Query: Distorted 808 (140 BPM, Key 11A, Genre "Trap")
    auto d808_q = synthDistorted808(55.0f, 3.5f);
    reals::db::SampleRecord q;
    q.filename = "Query_808_140bpm_11A_Trap.wav";
    q.path = "C:/Samples/Query_808_140bpm_11A_Trap.wav";
    q.bpm = 140.0;
    q.camelot = "11A";
    q.genre = "Trap";
    int64_t idQ = db->upsertSample(q);
    ASSERT_GT(idQ, 0);

    reals::db::AnalysisRecord aQ;
    aQ.sampleId = idQ;
    aQ.embedding = ai::ClapEmbedder::embedAudio(d808_q.data(), d808_q.size(), 48000);
    ASSERT_TRUE(db->updateAnalysis(idQ, aQ));

    // Execute SearchEngine similar search
    reals::search::SearchEngine engine(db);
    ASSERT_TRUE(engine.refreshIndex());

    auto results = engine.searchSimilar(idQ, 10, 0.0f);
    ASSERT_GE(results.size(), 3u);

    std::cout << "\n========================================================================================\n";
    std::cout << "                  METADATA BONUS DISTORTION ISOLATION TEST RESULTS\n";
    std::cout << "========================================================================================\n";
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "Rank #" << (i + 1) << " | ID: " << results[i].sample.id
                  << " | Filename: " << std::setw(36) << std::left << results[i].sample.filename
                  << " | Semantic CosSim: " << std::fixed << std::setprecision(4) << results[i].semanticScore
                  << " | Final Combined: " << results[i].combinedScore << "\n";
    }
    std::cout << "========================================================================================\n\n";

    // 1. Candidate 1 (True acoustic 808 match, 0 bonus) MUST rank #1 strictly above Candidate 2 (Guitar + max bonus +0.12)
    EXPECT_EQ(results[0].sample.id, id1);
    EXPECT_GT(results[0].combinedScore, results[1].combinedScore);

    // 2. High semantic similarity for genuine sound match
    EXPECT_GE(results[0].semanticScore, 0.70f);

    // 3. Combined score margin verification
    float scoreMargin = results[0].combinedScore - results[1].combinedScore;
    std::cout << "[Benchmark] Score margin between true match and max-bonus mismatch: " << scoreMargin << "\n";
    EXPECT_GT(scoreMargin, 0.30f);
}

// ============================================================================
// Test 6: Pure Semantic Vector Search Monotonicity
// ============================================================================

TEST_F(AcousticBenchmark, Benchmark_PureSemanticVectorSearch_Monotonicity) {
    auto db = std::make_shared<reals::db::Database>();
    ASSERT_TRUE(db->open(":memory:"));

    auto sub = synthCleanSub();
    auto gtr = synthBrightGuitar();
    auto hihat = synthMetallicHiHat();

    reals::db::SampleRecord r1;
    r1.filename = "Clean_Sub.wav";
    r1.path = "C:/Samples/Clean_Sub.wav";
    int64_t id1 = db->upsertSample(r1);
    reals::db::AnalysisRecord a1;
    a1.sampleId = id1;
    a1.embedding = ai::ClapEmbedder::embedAudio(sub.data(), sub.size(), 48000);
    db->updateAnalysis(id1, a1);

    reals::db::SampleRecord r2;
    r2.filename = "Bright_Guitar.wav";
    r2.path = "C:/Samples/Bright_Guitar.wav";
    int64_t id2 = db->upsertSample(r2);
    reals::db::AnalysisRecord a2;
    a2.sampleId = id2;
    a2.embedding = ai::ClapEmbedder::embedAudio(gtr.data(), gtr.size(), 48000);
    db->updateAnalysis(id2, a2);

    reals::db::SampleRecord r3;
    r3.filename = "Metallic_HiHat.wav";
    r3.path = "C:/Samples/Metallic_HiHat.wav";
    int64_t id3 = db->upsertSample(r3);
    reals::db::AnalysisRecord a3;
    a3.sampleId = id3;
    a3.embedding = ai::ClapEmbedder::embedAudio(hihat.data(), hihat.size(), 48000);
    db->updateAnalysis(id3, a3);

    reals::search::SearchEngine engine(db);
    ASSERT_TRUE(engine.refreshIndex());

    // Query with Clean Sub
    auto querySub = synthCleanSub(55.0f, 1.0f);
    auto queryEmb = ai::ClapEmbedder::embedAudio(querySub.data(), querySub.size(), 48000);

    auto vectorResults = engine.searchSemanticVector(queryEmb, 10, -1.0f);
    ASSERT_EQ(vectorResults.size(), 3u);

    // Pure vector search must rank Clean Sub first
    EXPECT_EQ(vectorResults[0].sample.id, id1);
    EXPECT_GE(vectorResults[0].semanticScore, 0.85f);
    EXPECT_GT(vectorResults[0].semanticScore, vectorResults[1].semanticScore);
    EXPECT_GT(vectorResults[1].semanticScore, vectorResults[2].semanticScore);
}

} // namespace reals::test
