// Production AI class tests (TempoDetector, KeyDetector, FeatureExtractor,
// Genre/Mood classifiers, ClapEmbedder, OnnxEngine, ModelManager, Hash).
// Consolidated from the former standalone test_ai binary (MIN-05/MIN-06)
// so reals_tests exercises the real production classes, not just mocks.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../framework/AudioTestFixtures.h"
#include "../framework/TestRunner.h"

#include "reals/ai/ClapEmbedder.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/GenreClassifier.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/MoodClassifier.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/ai/TempoDetector.h"
#include "reals/util/Hash.h"

#include <cmath>
#include <iostream>

using namespace reals::ai;
using namespace reals::test;
using namespace reals::util;

// -----------------------------------------------------------------------------
// Suite 1: Hash Utilities
// -----------------------------------------------------------------------------

TEST(HashSuite, Sha256StandardVectors) {
    // Empty string
    EXPECT_EQ(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // "abc"
    EXPECT_EQ(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Long string
    EXPECT_EQ(sha256("The quick brown fox jumps over the lazy dog"),
              "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST(HashSuite, XxHash64Consistency) {
    uint64_t h1 = xxhash64("reals_lab_test_audio_feature");
    uint64_t h2 = xxhash64("reals_lab_test_audio_feature");
    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, 0ULL);

    uint64_t hDiff = xxhash64("reals_lab_test_audio_feature_2");
    EXPECT_NE(h1, hDiff);
}

// -----------------------------------------------------------------------------
// Suite 2: FeatureExtractor (DSP)
// -----------------------------------------------------------------------------

TEST(FeatureExtractorSuite, ResampleMonoDownmix) {
    // 2-channel stereo sine 48kHz -> mono 44.1kHz
    auto stereo = AudioTestFixtures::generateStereoSine(440.0f, 440.0f, 1.0f, 48000);
    EXPECT_EQ(stereo.size(), 48000 * 2);

    auto mono441 = FeatureExtractor::resampleMono(stereo.data(), 48000, 2, 48000, 44100);
    EXPECT_EQ(mono441.size(), 44100);

    // Peak amplitude should be ~0.8
    float maxAmp = 0.0f;
    for (float s : mono441) {
        maxAmp = std::max(maxAmp, std::abs(s));
    }
    EXPECT_NEAR(maxAmp, 0.8f, 0.05f);
}

TEST(FeatureExtractorSuite, FftFrequencyPeak) {
    constexpr int kFftSize = 1024;
    constexpr int kSampleRate = 44100;
    // 440Hz sine
    auto sine = AudioTestFixtures::generateSine(440.0f, 0.1f, kSampleRate, 1.0f);

    std::vector<std::complex<float>> frame(kFftSize);
    for (int i = 0; i < kFftSize; ++i) {
        frame[i] = std::complex<float>(sine[i], 0.0f);
    }

    FeatureExtractor::fft(frame);

    // Bin index for 440Hz: 440 * 1024 / 44100 = ~10.2 -> bin 10
    int maxBin = 0;
    float maxMag = 0.0f;
    for (int k = 0; k < kFftSize / 2; ++k) {
        float mag = std::abs(frame[k]);
        if (mag > maxMag) {
            maxMag = mag;
            maxBin = k;
        }
    }

    EXPECT_EQ(maxBin, 10);
}

TEST(FeatureExtractorSuite, MelFilterbankSanity) {
    auto fb = FeatureExtractor::createMelFilterbank(64, 1024, 44100.0f, 20.0f, 20000.0f);
    EXPECT_EQ(fb.size(), 64);
    EXPECT_EQ(fb[0].size(), 513); // 1024/2 + 1

    // Check non-negative
    for (const auto& row : fb) {
        for (float val : row) {
            EXPECT_GE(val, 0.0f);
        }
    }
}

TEST(FeatureExtractorSuite, LogMelSpectrogramDimensions) {
    auto sine = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    SpectrogramConfig cfg;
    cfg.sampleRate = 44100;
    cfg.nFft = 1024;
    cfg.hopLength = 512;
    cfg.nMels = 40;

    auto logMel = FeatureExtractor::computeLogMel(sine, cfg);
    EXPECT_GT(logMel.size(), 0);
    EXPECT_EQ(logMel[0].size(), 40);

    for (const auto& frame : logMel) {
        for (float val : frame) {
            EXPECT_FALSE(std::isnan(val));
            EXPECT_FALSE(std::isinf(val));
        }
    }
}

TEST(FeatureExtractorSuite, ChromagramChordProfiles) {
    // C Major chord (C=261.63, E=329.63, G=392.00)
    auto cMajor = AudioTestFixtures::generateChordTriad(261.63f, false, 0.5f, 44100);
    auto chromaC = FeatureExtractor::computeGlobalChroma(cMajor, 44100);

    EXPECT_EQ(chromaC.size(), 12);
    // C (0), E (4), G (7) should be prominently higher than non-chord tones (e.g. C# = 1, D# = 3)
    EXPECT_GT(chromaC[0], chromaC[1]); // C > C#
    EXPECT_GT(chromaC[4], chromaC[3]); // E > D#
    EXPECT_GT(chromaC[7], chromaC[6]); // G > F#
}

TEST(FeatureExtractorSuite, OnsetNoveltyCurvePeaks) {
    // 120 BPM kick rhythm (beat every 0.5s)
    auto rhythm = AudioTestFixtures::generateKickRhythm(120.0f, 2.0f, 44100);
    auto onset = FeatureExtractor::computeOnsetEnvelope(rhythm, 44100, 1024, 512);

    EXPECT_GT(onset.size(), 50);
    float maxVal = 0.0f;
    for (float v : onset) {
        maxVal = std::max(maxVal, v);
    }
    EXPECT_NEAR(maxVal, 1.0f, 0.01f);
}

TEST(FeatureExtractorSuite, AudioMetricsDescriptors) {
    auto noise = AudioTestFixtures::generateNoise(1.0f, 44100, 0.5f);
    auto m = FeatureExtractor::computeMetrics(noise, 44100);

    EXPECT_GT(m.rms, 0.1f);
    EXPECT_GT(m.peak, 0.3f);
    EXPECT_GT(m.spectralCentroid, 1000.0f);
    EXPECT_GT(m.zeroCrossingRate, 0.05f);
}

// -----------------------------------------------------------------------------
// Suite 3: OnnxEngine & ModelManager
// -----------------------------------------------------------------------------

TEST(OnnxEngineSuite, InitializationAndSessionManagement) {
    auto& engine = OnnxEngine::instance();
    EXPECT_TRUE(engine.init());
    EXPECT_TRUE(engine.isInitialized());
    EXPECT_FALSE(engine.getModelsDir().empty());

    // Initially no models loaded
    EXPECT_FALSE(engine.isModelLoaded("tempo_cnn"));

    engine.unloadAll();
    EXPECT_FALSE(engine.isModelLoaded("tempo_cnn"));
}

TEST(ModelManagerSuite, RegistryAndPathResolution) {
    auto& mm = ModelManager::instance();
    EXPECT_TRUE(mm.init());
    EXPECT_FALSE(mm.getModelsDir().empty());

    auto models = mm.listModels();
    EXPECT_GE(models.size(), 6); // tempo_cnn, edma_key, discogs_maest, mood_jamendo, clap_audio, clap_text

    bool foundTempo = false;
    for (const auto& m : models) {
        if (m.id == "tempo_cnn") {
            foundTempo = true;
            EXPECT_EQ(m.fileName, "tempo_cnn.onnx");
        }
    }
    EXPECT_TRUE(foundTempo);
}

// -----------------------------------------------------------------------------
// Suite 4: TempoDetector
// -----------------------------------------------------------------------------

TEST(TempoDetectorSuite, Detect120BpmKickRhythm) {
    // Generate 120 BPM pulses for 4 seconds
    auto audio = AudioTestFixtures::generateKickRhythm(120.0f, 4.0f, 44100);
    auto res = TempoDetector::detect(audio.data(), audio.size(), 44100);

    EXPECT_NEAR(res.bpm, 120.0f, 3.0f); // within 3 BPM
    EXPECT_GT(res.confidence, 0.3f);
    EXPECT_GT(res.beatOnsets.size(), 4);
    EXPECT_FALSE(res.method.empty());
}

TEST(TempoDetectorSuite, Detect140BpmKickRhythm) {
    // Generate 140 BPM pulses for 4 seconds
    auto audio = AudioTestFixtures::generateKickRhythm(140.0f, 4.0f, 44100);
    auto res = TempoDetector::detect(audio.data(), audio.size(), 44100);

    EXPECT_NEAR(res.bpm, 140.0f, 3.0f);
    EXPECT_GT(res.confidence, 0.3f);
    EXPECT_GT(res.beatOnsets.size(), 4);
}

TEST(TempoDetectorSuite, SilenceFallback) {
    auto silence = AudioTestFixtures::generateSilent(1.0f, 44100);
    auto res = TempoDetector::detect(silence.data(), silence.size(), 44100);

    EXPECT_GT(res.bpm, 0.0f);
    EXPECT_LE(res.confidence, 0.3f);
}

// -----------------------------------------------------------------------------
// Suite 5: KeyDetector (Ensemble Voting + Camelot/OpenKey)
// -----------------------------------------------------------------------------

TEST(KeyDetectorSuite, DetectCMajorKey) {
    // C Major: C4 (261.63) + E4 (329.63) + G4 (392.00)
    auto cMaj = AudioTestFixtures::generateChordTriad(261.63f, false, 2.0f, 44100);
    auto res = KeyDetector::detect(cMaj.data(), cMaj.size(), 44100);

    EXPECT_EQ(res.key, "C");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_EQ(res.camelot, "8B");
    EXPECT_EQ(res.openKey, "1d");
    EXPECT_GT(res.confidence, 0.5f);
}

TEST(KeyDetectorSuite, DetectAMinorKey) {
    // A Minor: A3 (220.00) + C4 (261.63) + E4 (329.63)
    auto aMin = AudioTestFixtures::generateChordTriad(220.00f, true, 2.0f, 44100);
    auto res = KeyDetector::detect(aMin.data(), aMin.size(), 44100);

    EXPECT_EQ(res.key, "A");
    EXPECT_EQ(res.mode, "Minor");
    EXPECT_EQ(res.camelot, "8A");
    // Standard OpenKey: relative pairs share the number — 1d = C major / 1m = A minor.
    EXPECT_EQ(res.openKey, "1m");
    EXPECT_GT(res.confidence, 0.5f);
}

TEST(KeyDetectorSuite, DetectFSharpMajorKey) {
    // F# Major: F#4 (369.99) + A#4 (466.16) + C#5 (554.37)
    auto fsMaj = AudioTestFixtures::generateChordTriad(369.99f, false, 2.0f, 44100);
    auto res = KeyDetector::detect(fsMaj.data(), fsMaj.size(), 44100);

    EXPECT_EQ(res.key, "F#");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_EQ(res.camelot, "2B");
    EXPECT_EQ(res.openKey, "7d");
    EXPECT_GT(res.confidence, 0.5f);
}

TEST(KeyDetectorSuite, CamelotAndOpenKeyMappings) {
    EXPECT_EQ(KeyDetector::toCamelot("C", "Major"), "8B");
    EXPECT_EQ(KeyDetector::toCamelot("A", "Minor"), "8A");
    EXPECT_EQ(KeyDetector::toCamelot("F#", "Major"), "2B");
    EXPECT_EQ(KeyDetector::toCamelot("D#", "Minor"), "2A");
    EXPECT_EQ(KeyDetector::toCamelot("G#", "Minor"), "1A");
    EXPECT_EQ(KeyDetector::toCamelot("B", "Major"), "1B");

    EXPECT_EQ(KeyDetector::toOpenKey("C", "Major"), "1d");
    // Standard OpenKey wheel: C minor = 10m (relative pairing with Eb major).
    EXPECT_EQ(KeyDetector::toOpenKey("C", "Minor"), "10m");
    EXPECT_EQ(KeyDetector::toOpenKey("A", "Major"), "4d");
    // A minor is the relative minor of C major -> 1m.
    EXPECT_EQ(KeyDetector::toOpenKey("A", "Minor"), "1m");
    EXPECT_EQ(KeyDetector::toOpenKey("F#", "Major"), "7d");

    // Pitch class 11 (B / Cb) OpenKey assertions (Milestone 1 Iteration 2 remediation)
    EXPECT_EQ(KeyDetector::toOpenKey("B", "Major"), "6d");
    EXPECT_EQ(KeyDetector::toOpenKey("B", "Minor"), "3m");
    EXPECT_EQ(KeyDetector::toOpenKey("Cb", "Major"), "6d");
    EXPECT_EQ(KeyDetector::toOpenKey("Cb", "Minor"), "3m");

    // Theoretical enharmonics and fallback consistency
    EXPECT_EQ(KeyDetector::toOpenKey("E#", "Major"), "12d");
    EXPECT_EQ(KeyDetector::toOpenKey("Fb", "Major"), "5d");
    EXPECT_EQ(KeyDetector::toOpenKey("UnknownKey", "Major"), "1d");
    EXPECT_EQ(KeyDetector::toOpenKey("UnknownKey", "Minor"), "10m");
}

TEST(KeyDetectorSuite, DigitalSilenceFallback) {
    auto silence = AudioTestFixtures::generateSilent(1.0f, 44100);
    auto res = KeyDetector::detect(silence.data(), silence.size(), 44100);

    EXPECT_EQ(res.key, "C");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_EQ(res.camelot, "8B");
    EXPECT_EQ(res.openKey, "1d");
    EXPECT_FLOAT_EQ(res.confidence, 0.0f);
}

TEST(KeyDetectorSuite, NullAndZeroFramesFallback) {
    auto resNull = KeyDetector::detect(nullptr, 44100, 44100);
    EXPECT_EQ(resNull.key, "C");
    EXPECT_EQ(resNull.mode, "Major");
    EXPECT_EQ(resNull.camelot, "8B");
    EXPECT_EQ(resNull.openKey, "1d");
    EXPECT_FLOAT_EQ(resNull.confidence, 0.0f);

    float dummy = 0.5f;
    auto resZero = KeyDetector::detect(&dummy, 0, 44100);
    EXPECT_EQ(resZero.key, "C");
    EXPECT_EQ(resZero.mode, "Major");
    EXPECT_FLOAT_EQ(resZero.confidence, 0.0f);
}

TEST(KeyDetectorSuite, DetectBMajorKey) {
    // B Major: B3 (246.94 Hz) triad
    auto bMaj = AudioTestFixtures::generateChordTriad(246.94f, false, 2.0f, 44100);
    auto res = KeyDetector::detect(bMaj.data(), bMaj.size(), 44100);

    EXPECT_EQ(res.key, "B");
    EXPECT_EQ(res.mode, "Major");
    EXPECT_EQ(res.camelot, "1B");
    EXPECT_EQ(res.openKey, "6d");
    EXPECT_GT(res.confidence, 0.5f);
}

TEST(KeyDetectorSuite, DetectBMinorKey) {
    // B Minor: B3 (246.94 Hz) minor triad
    auto bMin = AudioTestFixtures::generateChordTriad(246.94f, true, 2.0f, 44100);
    auto res = KeyDetector::detect(bMin.data(), bMin.size(), 44100);

    EXPECT_EQ(res.key, "B");
    EXPECT_EQ(res.mode, "Minor");
    EXPECT_EQ(res.camelot, "10A");
    EXPECT_EQ(res.openKey, "3m");
    EXPECT_GT(res.confidence, 0.5f);
}

// -----------------------------------------------------------------------------
// Suite 6: GenreClassifier (Discogs-MAEST 400 Styles)
// -----------------------------------------------------------------------------

TEST(GenreClassifierSuite, TaxonomySize) {
    const auto& tax = GenreClassifier::getTaxonomy();
    EXPECT_GE(tax.size(), 400);
}

TEST(GenreClassifierSuite, ClassifyHeavyBassSample) {
    // 140 BPM kick rhythm (Trap/Dubstep profile)
    auto rhythm = AudioTestFixtures::generateKickRhythm(140.0f, 3.0f, 44100);
    auto genres = GenreClassifier::classify(rhythm.data(), rhythm.size(), 44100, 5);

    EXPECT_EQ(genres.size(), 5);
    for (size_t i = 1; i < genres.size(); ++i) {
        EXPECT_GE(genres[i - 1].score, genres[i].score);
    }

    bool matchedElectronicOrUrban = false;
    for (const auto& g : genres) {
        if (g.tag == "Trap-EDM" || g.tag == "Trap" || g.tag == "Dubstep" ||
            g.tag == "Future Bass" || g.tag == "Hip Hop") {
            matchedElectronicOrUrban = true;
            break;
        }
    }
    EXPECT_TRUE(matchedElectronicOrUrban);
}

// -----------------------------------------------------------------------------
// Suite 7: MoodClassifier (Mood-Jamendo 56 Tags)
// -----------------------------------------------------------------------------

TEST(MoodClassifierSuite, MoodTagsCount) {
    const auto& tags = MoodClassifier::getMoodTags();
    EXPECT_EQ(tags.size(), 56);
}

TEST(MoodClassifierSuite, ClassifyEmotionalDimensions) {
    // A Minor slow chord (melancholy/sad/dark profile)
    auto aMin = AudioTestFixtures::generateChordTriad(220.00f, true, 2.0f, 44100, 0.4f);
    auto moods = MoodClassifier::classify(aMin.data(), aMin.size(), 44100, 0.20f);

    EXPECT_GT(moods.size(), 0);
    for (size_t i = 1; i < moods.size(); ++i) {
        EXPECT_GE(moods[i - 1].score, moods[i].score);
    }
}

// -----------------------------------------------------------------------------
// Suite 8: ClapEmbedder (512-dim Audio & Text Embeddings)
// -----------------------------------------------------------------------------

TEST(ClapEmbedderSuite, AudioEmbeddingProperties) {
    auto sine = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    auto emb = ClapEmbedder::embedAudio(sine.data(), sine.size(), 44100);

    EXPECT_EQ(emb.size(), 512);

    // Verify L2 norm is 1.0
    float sumSq = 0.0f;
    for (float v : emb) {
        sumSq += v * v;
    }
    EXPECT_NEAR(std::sqrt(sumSq), 1.0f, 0.001f);
}

TEST(ClapEmbedderSuite, TextEmbeddingProperties) {
    auto emb = ClapEmbedder::embedText("dark ambient warm 808 sub bass");
    EXPECT_EQ(emb.size(), 512);

    float sumSq = 0.0f;
    for (float v : emb) {
        sumSq += v * v;
    }
    EXPECT_NEAR(std::sqrt(sumSq), 1.0f, 0.001f);
}

TEST(ClapEmbedderSuite, CosineSimilarityIdentityAndRange) {
    auto emb1 = ClapEmbedder::embedText("lo-fi chill relaxing beats");
    auto emb2 = ClapEmbedder::embedText("lo-fi chill relaxing beats");
    auto embOther = ClapEmbedder::embedText("aggressive heavy metal screaming guitar");

    float selfSim = ClapEmbedder::cosineSimilarity(emb1, emb2);
    EXPECT_NEAR(selfSim, 1.0f, 0.001f);

    float crossSim = ClapEmbedder::cosineSimilarity(emb1, embOther);
    EXPECT_GE(crossSim, -1.0f);
    EXPECT_LE(crossSim, 1.0f);
    EXPECT_LT(crossSim, 0.95f); // Different concepts should have lower similarity
}

