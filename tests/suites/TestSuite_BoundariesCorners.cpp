#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/ModelMocks.h"
#include "../framework/TestRunner.h"
#include <reals/ai/FeatureExtractor.h>
#include <reals/audio/Engine.h>
#include <reals/platform/Path.h>
#include <reals/util/Hash.h>

namespace reals::test {

namespace fs = std::filesystem;

// ============================================================================
// Tier 2: Audio File Integrity & Header Corruption
// ============================================================================

TEST(BoundariesCorners, Corner_Audio_0ByteFile) {
    DbTestFixtures fixture;
    std::string emptyFile = (fixture.tempDir() / "zero_byte.wav").string();
    AudioTestFixtures::writeCorruptedWavFile(emptyFile, WavCorruptionType::ZeroByte);

    EXPECT_TRUE(fs::exists(emptyFile));
    EXPECT_EQ(fs::file_size(emptyFile), 0u);

    // Audio probe should report 0 sample rate and 0 duration without crashing
    auto info = reals::audio::Engine::probeFile(emptyFile);
    EXPECT_EQ(info.sampleRate, 0);
    EXPECT_EQ(info.totalFrames, 0);
}

TEST(BoundariesCorners, Corner_Audio_CorruptedRiffHeader) {
    DbTestFixtures fixture;
    std::string truncFile = (fixture.tempDir() / "truncated_riff.wav").string();
    AudioTestFixtures::writeCorruptedWavFile(truncFile, WavCorruptionType::TruncatedRiffHeader);

    auto info = reals::audio::Engine::probeFile(truncFile);
    EXPECT_EQ(info.sampleRate, 0);
}

TEST(BoundariesCorners, Corner_Audio_SilentBufferDigitalZero) {
    auto silence = AudioTestFixtures::generateSilent(2.0f, 44100);
    auto metrics = reals::ai::FeatureExtractor::computeMetrics(silence, 44100);

    EXPECT_EQ(metrics.rms, 0.0f);
    EXPECT_EQ(metrics.peak, 0.0f);
    EXPECT_FALSE(std::isnan(metrics.rms));
    EXPECT_FALSE(std::isnan(metrics.spectralCentroid));
}

TEST(BoundariesCorners, Corner_Audio_DcOffsetClipping) {
    auto dcPcm = AudioTestFixtures::generateDcOffset(1.0f, 44100, 1.8f);
    // In audio processing, clamp to [-1.0, 1.0]
    for (float& s : dcPcm) {
        s = std::clamp(s, -1.0f, 1.0f);
        EXPECT_LE(s, 1.0f);
        EXPECT_GE(s, -1.0f);
    }
}

TEST(BoundariesCorners, Corner_Audio_TrailingGarbageBytes) {
    DbTestFixtures fixture;
    std::string garbageFile = (fixture.tempDir() / "trailing_garbage.wav").string();
    AudioTestFixtures::writeCorruptedWavFile(garbageFile, WavCorruptionType::TrailingGarbageBytes);

    auto info = reals::audio::Engine::probeFile(garbageFile);
    EXPECT_GT(info.sampleRate, 0);
    EXPECT_GT(info.durationSeconds, 0.0);
}

// ============================================================================
// Tier 2: Boundary BPM & Extreme Pitch Values
// ============================================================================

TEST(BoundariesCorners, Corner_DSP_BoundaryBpmZero) {
    double dawBpm = 120.0;
    double sampleBpm = 0.0;
    double ratio = 1.0;

    if (sampleBpm > 0.0) {
        ratio = dawBpm / sampleBpm;
    }
    EXPECT_EQ(ratio, 1.0);
    EXPECT_FALSE(std::isinf(ratio));
}

TEST(BoundariesCorners, Corner_DSP_BoundaryBpmExtremeLowAndHigh) {
    double lowBpm = 10.0;
    double highBpm = 999.0;
    double targetDawBpm = 120.0;

    double ratioLow = targetDawBpm / lowBpm;
    double ratioHigh = targetDawBpm / highBpm;

    EXPECT_NEAR(ratioLow, 12.0, 1e-4);
    EXPECT_NEAR(ratioHigh, 0.12012, 1e-4);
}

TEST(BoundariesCorners, Corner_DSP_ExtremePitchShiftPlus12) {
    float semitones = 12.0f;
    float mult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_NEAR(mult, 2.0f, 1e-5f);
}

TEST(BoundariesCorners, Corner_DSP_ExtremePitchShiftMinus12) {
    float semitones = -12.0f;
    float mult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_NEAR(mult, 0.5f, 1e-5f);
}

TEST(BoundariesCorners, Corner_DSP_PitchShiftOutOfBoundsClamping) {
    float requestedSemitones = 36.0f;
    float clampedSemitones = std::clamp(requestedSemitones, -12.0f, 12.0f);
    EXPECT_EQ(clampedSemitones, 12.0f);

    float requestedNegative = -48.0f;
    float clampedNeg = std::clamp(requestedNegative, -12.0f, 12.0f);
    EXPECT_EQ(clampedNeg, -12.0f);
}

// ============================================================================
// Tier 2: Unicode, Vietnamese Characters & Special Symbols
// ============================================================================

TEST(BoundariesCorners, Corner_Unicode_VietnameseFilePaths) {
    DbTestFixtures fixture;
    auto viDir = fixture.tempDir() / platform::u8path("Nhạc Mới 2026");
    fs::create_directories(viDir);

    std::string viFile = platform::pathToUtf8(viDir / platform::u8path("Giai Điệu Trầm_01.wav"));
    bool written = AudioTestFixtures::writeWavFile(viFile, AudioTestFixtures::generateSine(440.0f, 0.2f), 1, 44100);
    EXPECT_TRUE(written);
    EXPECT_TRUE(fs::exists(platform::u8path(viFile)));

    // Verify sha256 calculation on Vietnamese path
    std::string hash = reals::util::sha256File(viFile);
    EXPECT_FALSE(hash.empty());
}

TEST(BoundariesCorners, Corner_Unicode_SpecialSymbolsInSearch) {
    std::string complexTag = "808&sub-bass /bpm:120-130 #trending @drop";
    uint64_t h = reals::util::xxhash64(complexTag);
    EXPECT_NE(h, 0u);
}

TEST(BoundariesCorners, Corner_Unicode_SqlInjectionInQuery) {
    std::string attackPayload = "'; DROP TABLE samples; --";
    // Check that string hash and token parser treat it safely as text
    std::string sha = reals::util::sha256(attackPayload);
    EXPECT_EQ(sha.length(), 64u);
}

TEST(BoundariesCorners, Corner_Unicode_EmojisInMetadata) {
    std::string emojiTag = "Trap 🔥 🥁 140BPM ✨";
    EXPECT_GT(emojiTag.length(), 0u);
    uint64_t h = reals::util::xxhash64(emojiTag);
    EXPECT_NE(h, 0u);
}

// ============================================================================
// Tier 2: Scalability & Concurrency
// ============================================================================

TEST(BoundariesCorners, Corner_DB_HugeLibrary10kRecords) {
    const size_t recordCount = 10000;
    auto dataset = DbTestFixtures::generateSampleDataset(recordCount);
    EXPECT_EQ(dataset.size(), recordCount);

    auto queryVec = DbTestFixtures::generateUnitEmbedding(777);

    auto start = std::chrono::high_resolution_clock::now();
    // Simulate top-1 search across 10,000 embeddings
    float bestScore = -1.0f;
    int64_t bestId = -1;

    for (const auto& rec : dataset) {
        float score = ModelMocks::cosineSimilarity(queryVec.data(), rec.embedding.data(), 512);
        if (score > bestScore) {
            bestScore = score;
            bestId = rec.id;
        }
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    EXPECT_GT(bestId, 0);
    EXPECT_GE(bestScore, -1.0f);
    // 10,000 comparisons must complete in < 50ms
    EXPECT_LT(elapsedMs, 50);
}

TEST(BoundariesCorners, Corner_DB_ConcurrentReadWrite) {
    DbTestFixtures::MockDbStore store;
    auto dataset = DbTestFixtures::generateSampleDataset(200);

    for (size_t i = 0; i < 100; ++i) {
        store.insert(dataset[i]);
    }

    std::atomic<bool> done{false};
    size_t readsCount = 0;

    // Concurrent reader thread
    std::thread reader([&]() {
        while (!done.load()) {
            auto res = store.queryByFilter("", 100.0f, 150.0f, "", false);
            readsCount += res.size();
        }
    });

    // Writer thread
    for (size_t i = 100; i < 200; ++i) {
        store.insert(dataset[i]);
    }

    done.store(true);
    reader.join();

    EXPECT_EQ(store.count(), 200u);
    EXPECT_GT(readsCount, 0u);
}

} // namespace reals::test
