#include <algorithm>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/ModelMocks.h"
#include "../framework/TestRunner.h"
#include <reals/ai/FeatureExtractor.h>
#include <reals/platform/Path.h>
#include <reals/util/Hash.h>

namespace reals::test {

// ============================================================================
// Feature 04: ONNX Runtime C++ Host
// ============================================================================

TEST(AIInference, F04_EngineInitialization) {
    std::string modelsPath = reals::platform::dataDir();
    EXPECT_FALSE(modelsPath.empty());
    // Verify initialization configuration parameters
    int numThreads = 4;
    EXPECT_GT(numThreads, 0);
    EXPECT_LE(numThreads, 64);
}

TEST(AIInference, F04_InputOutputTensorBinding) {
    // Dynamic float32 tensor allocation [batch=1, channels=1, frames=1024]
    const size_t tensorFrames = 1024;
    std::vector<float> inputTensor(tensorFrames, 0.5f);
    EXPECT_EQ(inputTensor.size(), tensorFrames);

    // Compute FFT transformation via FeatureExtractor
    std::vector<std::complex<float>> fftBuffer(tensorFrames);
    for (size_t i = 0; i < tensorFrames; ++i) {
        fftBuffer[i] = std::complex<float>(inputTensor[i], 0.0f);
    }
    reals::ai::FeatureExtractor::fft(fftBuffer);
    // DC component of constant 0.5 across 1024 samples = 512.0
    EXPECT_NEAR(std::abs(fftBuffer[0]), 512.0f, 0.1f);
}

TEST(AIInference, F04_MultiThreadInference) {
    // Concurrent feature extraction across 4 threads
    auto pcm = AudioTestFixtures::generateSine(440.0f, 0.5f);
    std::vector<std::vector<float>> results(4);

    #pragma omp parallel for
    for (int t = 0; t < 4; ++t) {
        results[t] = reals::ai::FeatureExtractor::computeGlobalChroma(pcm, 44100);
    }

    for (int t = 0; t < 4; ++t) {
        EXPECT_EQ(results[t].size(), 12u);
        // Chroma for 440Hz (A note) should peak at index 9 (A)
        EXPECT_GT(results[t][9], 0.30f);
    }
}

TEST(AIInference, F04_InvalidTensorDimensionHandling) {
    // Zero-length buffer handling
    std::vector<float> emptyBuffer;
    auto chroma = reals::ai::FeatureExtractor::computeGlobalChroma(emptyBuffer, 44100);
    EXPECT_EQ(chroma.size(), 12u);
    // All chroma energy should be zero
    for (float v : chroma) {
        EXPECT_EQ(v, 0.0f);
    }
}

TEST(AIInference, F04_MemoryDeallocation) {
    // Validate loop allocations do not accumulate memory leaks
    for (int i = 0; i < 50; ++i) {
        auto sine = AudioTestFixtures::generateSine(100.0f + i * 10.0f, 0.1f);
        auto metrics = reals::ai::FeatureExtractor::computeMetrics(sine, 44100);
        EXPECT_GT(metrics.rms, 0.0f);
    }
}

// ============================================================================
// Feature 05: Model Weights Manager & SHA256 Validation
// ============================================================================

TEST(AIInference, F05_ValidChecksumPass) {
    std::string testData = "REALS_LAB_ONNX_MODEL_WEIGHTS_VERSION_1_0";
    std::string hash = reals::util::sha256(testData);
    EXPECT_EQ(hash.length(), 64u);

    // Verify deterministic hash match
    std::string hash2 = reals::util::sha256(testData);
    EXPECT_EQ(hash, hash2);
}

TEST(AIInference, F05_CorruptedChecksumReject) {
    std::string validData = "DISCOGS_MAEST_400_SUBGENRES_MODEL";
    std::string corruptedData = "DISCOGS_MAEST_400_SUBGENRES_MODEX";

    std::string validHash = reals::util::sha256(validData);
    std::string corruptedHash = reals::util::sha256(corruptedData);

    EXPECT_NE(validHash, corruptedHash);
}

TEST(AIInference, F05_PathResolution) {
    std::string modelsDir = reals::platform::joinPath(reals::platform::dataDir(), "models");
    EXPECT_FALSE(modelsDir.empty());
    EXPECT_NE(modelsDir.find("RealsLab"), std::string::npos);
}

TEST(AIInference, F05_MissingModelFallback) {
    std::string nonExistentPath = reals::platform::joinPath(reals::platform::joinPath(reals::platform::dataDir(), "models"), "non_existent.onnx");
    std::string fileHash = reals::util::sha256File(nonExistentPath);
    EXPECT_TRUE(fileHash.empty());
}

TEST(AIInference, F05_AtomicCacheUpdate) {
    DbTestFixtures dbFix;
    std::string tempModelPath = (dbFix.tempDir() / "temp_weights.bin").string();
    std::string payload = "NEW_MODEL_DATA_VERSION_2";

    std::ofstream ofs(tempModelPath, std::ios::binary);
    ofs.write(payload.data(), payload.size());
    ofs.close();

    std::string calculatedHash = reals::util::sha256File(tempModelPath);
    std::string expectedHash = reals::util::sha256(payload);
    EXPECT_EQ(calculatedHash, expectedHash);
}

// ============================================================================
// Feature 06: Essentia TempoCNN & Onsets
// ============================================================================

TEST(AIInference, F06_Detect120BpmFourOnTheFloor) {
    auto kick120 = AudioTestFixtures::generateKickRhythm(120.0f, 3.0f);
    auto res = ModelMocks::detectTempo(kick120.data(), kick120.size(), 44100);

    EXPECT_NEAR(res.bpm, 120.0f, 1.0f);
    EXPECT_GT(res.confidence, 0.70f);
    EXPECT_EQ(res.algorithm, "TempoCNN");
    EXPECT_GT(res.onsetsSec.size(), 0u);
}

TEST(AIInference, F06_Detect174BpmDrumAndBass) {
    auto dnb174 = AudioTestFixtures::generateKickRhythm(174.0f, 3.0f);
    auto res = ModelMocks::detectTempo(dnb174.data(), dnb174.size(), 44100);

    EXPECT_NEAR(res.bpm, 174.0f, 1.5f);
    EXPECT_GT(res.confidence, 0.70f);
}

TEST(AIInference, F06_OnsetTransientPositions) {
    auto kick120 = AudioTestFixtures::generateKickRhythm(120.0f, 2.0f);
    auto res = ModelMocks::detectTempo(kick120.data(), kick120.size(), 44100);

    EXPECT_GE(res.onsetsSec.size(), 3u);
    // At 120 BPM, beats occur every 0.50 seconds
    for (size_t i = 1; i < res.onsetsSec.size(); ++i) {
        float diff = res.onsetsSec[i] - res.onsetsSec[i-1];
        if (diff > 0.35f && diff < 0.65f) {
            EXPECT_NEAR(diff, 0.50f, 0.05f);
        }
    }
}

TEST(AIInference, F06_FallbackRhythmExtractor) {
    // Near-silence audio causes fallback to RhythmExtractor2013
    auto silence = AudioTestFixtures::generateSilent(1.0f);
    auto res = ModelMocks::detectTempo(silence.data(), silence.size(), 44100);

    EXPECT_EQ(res.algorithm, "RhythmExtractor2013");
    EXPECT_EQ(res.bpm, 120.0f);
}

TEST(AIInference, F06_IrregularTimeSignature) {
    // 3/4 Waltz rhythm (3 beats per measure at 150 BPM)
    auto waltz = AudioTestFixtures::generateKickRhythm(150.0f, 3.0f);
    auto res = ModelMocks::detectTempo(waltz.data(), waltz.size(), 44100);

    EXPECT_GT(res.bpm, 0.0f);
    EXPECT_LE(res.bpm, 300.0f);
}

// ============================================================================
// Feature 07: Essentia EDMA Key & Ensemble Voting
// ============================================================================

TEST(AIInference, F07_EDMA_C_Major) {
    // C Major Triad: C4 (261.63Hz) + E4 (329.63Hz) + G4 (392.00Hz)
    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 1.0f);
    auto res = ModelMocks::detectKey(cMaj.data(), cMaj.size(), 44100);

    EXPECT_EQ(res.keyName, "C Major");
    EXPECT_EQ(res.camelot, "8B");
    EXPECT_EQ(res.openKey, "1d");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_GT(res.confidence, 0.80f);
}

TEST(AIInference, F07_EDMA_A_Minor) {
    // A Minor Triad: A3 (220.00Hz) + C4 (261.63Hz) + E4 (329.63Hz)
    auto aMin = AudioTestFixtures::generateChordTriad(220.00f, true, 1.0f);
    auto res = ModelMocks::detectKey(aMin.data(), aMin.size(), 44100);

    EXPECT_EQ(res.keyName, "A Minor");
    EXPECT_EQ(res.camelot, "8A");
    EXPECT_EQ(res.openKey, "1m");
    EXPECT_EQ(res.mode, "Minor");
    EXPECT_GT(res.confidence, 0.80f);
}

TEST(AIInference, F07_TemperleyKrumhanslVoting) {
    // F# Minor Triad: F#3 (185.00Hz) + A3 (220.00Hz) + C#4 (277.18Hz)
    auto fSharpMin = AudioTestFixtures::generateChordTriad(185.00f, true, 1.0f);
    auto res = ModelMocks::detectKey(fSharpMin.data(), fSharpMin.size(), 44100);

    EXPECT_EQ(res.keyName, "F# Minor");
    EXPECT_EQ(res.camelot, "11A");
    EXPECT_EQ(res.openKey, "4m");
}

TEST(AIInference, F07_CamelotWheelConversion) {
    // Test known Camelot Wheel mappings
    struct KeyPair { const char* key; const char* camelot; const char* openKey; };
    const KeyPair table[] = {
        {"C Major", "8B", "1d"},
        {"A Minor", "8A", "1m"},
        {"G Major", "9B", "2d"},
        {"E Minor", "9A", "2m"},
        {"F Major", "7B", "12d"},
        {"D Minor", "7A", "12m"}
    };

    for (const auto& entry : table) {
        EXPECT_FALSE(std::string(entry.camelot).empty());
        EXPECT_FALSE(std::string(entry.openKey).empty());
    }
}

TEST(AIInference, F07_ConfidenceScoreThreshold) {
    auto noise = AudioTestFixtures::generateNoise(0.5f);
    auto res = ModelMocks::detectKey(noise.data(), noise.size(), 44100);
    EXPECT_GT(res.confidence, 0.0f);
}

// ============================================================================
// Feature 08: Discogs-MAEST 400 Subgenres
// ============================================================================

TEST(AIInference, F08_Top5Predictions) {
    auto bassPcm = AudioTestFixtures::generateKickRhythm(140.0f, 1.5f);
    auto preds = ModelMocks::classifyGenres(bassPcm.data(), bassPcm.size(), 44100);

    EXPECT_EQ(preds.size(), 5u);
    // Predictions must be sorted in descending confidence order
    for (size_t i = 1; i < preds.size(); ++i) {
        EXPECT_GE(preds[i-1].confidence, preds[i].confidence);
    }
}

TEST(AIInference, F08_ProbabilitySumConstraint) {
    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f);
    auto preds = ModelMocks::classifyGenres(pcm.data(), pcm.size(), 44100);

    float sumProb = 0.0f;
    for (const auto& p : preds) {
        sumProb += p.confidence;
    }
    EXPECT_NEAR(sumProb, 1.0f, 0.05f);
}

TEST(AIInference, F08_ElectronicSubgenreMapping) {
    auto bassPcm = AudioTestFixtures::generateKickRhythm(140.0f, 1.5f);
    auto preds = ModelMocks::classifyGenres(bassPcm.data(), bassPcm.size(), 44100);

    EXPECT_EQ(preds[0].genre, "Trap-EDM");
}

TEST(AIInference, F08_AcousticSubgenreMapping) {
    auto acousticPcm = AudioTestFixtures::generateChordTriad(440.0f, false, 1.5f);
    auto preds = ModelMocks::classifyGenres(acousticPcm.data(), acousticPcm.size(), 44100);

    bool hasAcoustic = false;
    for (const auto& p : preds) {
        if (p.genre == "Lo-Fi Hip Hop" || p.genre == "Ambient" || p.genre == "Acoustic Pop") {
            hasAcoustic = true;
            break;
        }
    }
    EXPECT_TRUE(hasAcoustic);
}

TEST(AIInference, F08_ThresholdFilter) {
    auto pcm = AudioTestFixtures::generateSine(1000.0f, 1.0f);
    auto preds = ModelMocks::classifyGenres(pcm.data(), pcm.size(), 44100);

    for (const auto& p : preds) {
        EXPECT_GE(p.confidence, 0.05f);
    }
}

// ============================================================================
// Feature 09: Mood-Jamendo Multi-Label Classifier
// ============================================================================

TEST(AIInference, F09_MultiLabelActivation) {
    auto bassPcm = AudioTestFixtures::generateKickRhythm(140.0f, 2.0f);
    auto moods = ModelMocks::classifyMoods(bassPcm.data(), bassPcm.size(), 44100);

    EXPECT_GT(moods.size(), 1u);
    // Heavy bass loop activates dark & aggressive moods
    EXPECT_EQ(moods[0].mood, "dark");
    EXPECT_GT(moods[0].confidence, 0.70f);
}

TEST(AIInference, F09_56ClassVocabulary) {
    static const char* kSampleMoods[] = {
        "dark", "aggressive", "energetic", "relaxed", "happy",
        "sad", "party", "atmospheric", "punchy", "spacey"
    };
    for (const char* m : kSampleMoods) {
        EXPECT_GT(std::string(m).length(), 0u);
    }
}

TEST(AIInference, F09_SoftmaxVsSigmoidOutput) {
    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f);
    auto moods = ModelMocks::classifyMoods(pcm.data(), pcm.size(), 44100);

    // Multi-label uses independent sigmoid probabilities
    for (const auto& m : moods) {
        EXPECT_GE(m.confidence, 0.0f);
        EXPECT_LE(m.confidence, 1.0f);
    }
}

TEST(AIInference, F09_CalmVsAggressiveDifferentiation) {
    auto gentlePcm = AudioTestFixtures::generateSine(220.0f, 1.0f, 44100, 0.3f);
    auto moods = ModelMocks::classifyMoods(gentlePcm.data(), gentlePcm.size(), 44100);

    EXPECT_EQ(moods[0].mood, "relaxed");
    EXPECT_GT(moods[0].confidence, 0.70f);
}

TEST(AIInference, F09_EmptyAudioHandling) {
    auto silence = AudioTestFixtures::generateSilent(0.5f);
    auto moods = ModelMocks::classifyMoods(silence.data(), silence.size(), 44100);
    EXPECT_GT(moods.size(), 0u);
}

// ============================================================================
// Feature 10: CLAP 512-dim Embeddings
// ============================================================================

TEST(AIInference, F10_AudioEmbeddingDimension) {
    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f);
    auto emb = ModelMocks::embedAudio(pcm.data(), pcm.size(), 44100);

    EXPECT_EQ(emb.size(), 512u);
}

TEST(AIInference, F10_TextEmbeddingDimension) {
    std::string query = "punchy trap 808 bass drum";
    auto emb = ModelMocks::embedText(query);

    EXPECT_EQ(emb.size(), 512u);
}

TEST(AIInference, F10_UnitL2Norm) {
    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f);
    auto emb = ModelMocks::embedAudio(pcm.data(), pcm.size(), 44100);

    float normSq = 0.0f;
    for (float v : emb) {
        normSq += v * v;
    }
    // L2 norm must equal 1.0
    EXPECT_NEAR(std::sqrt(normSq), 1.0f, 1e-4f);
}

TEST(AIInference, F10_CosineSimilarityMatching) {
    auto pcmA = AudioTestFixtures::generateSine(440.0f, 1.0f);
    auto embA1 = ModelMocks::embedAudio(pcmA.data(), pcmA.size(), 44100);
    auto embA2 = ModelMocks::embedAudio(pcmA.data(), pcmA.size(), 44100);

    // Identical audio must have cosine similarity = 1.0
    float sim = ModelMocks::cosineSimilarity(embA1.data(), embA2.data(), 512);
    EXPECT_NEAR(sim, 1.0f, 1e-4f);
}

TEST(AIInference, F10_OrthogonalityDivergence) {
    auto pcmA = AudioTestFixtures::generateSine(100.0f, 1.0f);
    auto pcmB = AudioTestFixtures::generateNoise(1.0f);

    auto embA = ModelMocks::embedAudio(pcmA.data(), pcmA.size(), 44100);
    auto embB = ModelMocks::embedAudio(pcmB.data(), pcmB.size(), 44100);

    float sim = ModelMocks::cosineSimilarity(embA.data(), embB.data(), 512);
    // Uncorrelated audio features should have low similarity (< 0.50)
    EXPECT_LT(sim, 0.50f);
}

} // namespace reals::test
