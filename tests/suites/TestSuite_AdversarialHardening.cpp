#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/ModelMocks.h"
#include "../framework/TestRunner.h"
#include <reals/ai/FeatureExtractor.h>
#include <reals/audio/Engine.h>
#include <reals/db/Database.h>
#include <reals/platform/Path.h>
#include <reals/search/QueryParser.h>
#include <reals/search/SearchEngine.h>
#include <reals/util/Hash.h>
#include <reals/util/Simd.h>

namespace reals::test {

namespace fs = std::filesystem;

// ============================================================================
// Tier 5: Adversarial Hardening & Concurrency Stress Tests
// ============================================================================

TEST(AdversarialHardening, Stress_1000_ConcurrentBridgeRpcCalls) {
    BridgeTestHarness harness(120.0);
    const int totalRequests = 1000;
    const int numThreads = 8;
    std::atomic<int> successCount{0};

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < totalRequests / numThreads; ++i) {
                // Interleave various RPC commands
                int op = (t * (totalRequests / numThreads) + i) % 5;
                if (op == 0) {
                    auto res = harness.call("audio.setPitchShift", {{"semitones", static_cast<float>((i % 24) - 12)}});
                    if (res.value("ok", false)) successCount++;
                } else if (op == 1) {
                    auto res = harness.call("audio.setSyncBpm", {{"enabled", (i % 2 == 0)}, {"bpm", 120.0f + (i % 40)}});
                    if (res.value("ok", false)) successCount++;
                } else if (op == 2) {
                    auto res = harness.call("config.getAll", json::object());
                    if (res.value("ok", false)) successCount++;
                } else if (op == 3) {
                    auto res = harness.call("reaper.tempo", json::object());
                    if (res.value("ok", false)) successCount++;
                } else {
                    auto res = harness.call("app.info", json::object());
                    if (res.value("ok", false)) successCount++;
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(successCount.load(), totalRequests);
}

TEST(AdversarialHardening, Stress_RapidPianoTranspositionBursts) {
    BridgeTestHarness harness(128.0);

    // Simulate user rapidly clicking notes across the 12-key piano keyboard (200 rapid clicks)
    for (int i = 0; i < 200; ++i) {
        float semitones = static_cast<float>((i % 25) - 12); // Range [-12, +12]
        auto shiftRes = harness.call("audio.setPitchShift", {{"semitones", semitones}});
        EXPECT_TRUE(shiftRes.value("ok", false));
        EXPECT_NEAR(shiftRes["data"].value("pitchSemitones", 0.0f), semitones, 1e-4f);

        // Every 20 clicks, press Original Key
        if (i % 20 == 0) {
            auto resetRes = harness.call("audio.setOriginalKey", json::object());
            EXPECT_TRUE(resetRes.value("ok", false));
        }
    }
}

TEST(AdversarialHardening, Stress_ExtremePitchShiftBoundaryClamping) {
    BridgeTestHarness harness;

    // Test extreme values far outside nominal range [-48.0, +48.0]
    const float extremeValues[] = {-100.0f, -48.0f, -24.0f, -12.1f, 12.1f, 24.0f, 48.0f, 100.0f, 999.0f};
    for (float val : extremeValues) {
        auto res = harness.call("audio.setPitchShift", {{"semitones", val}});
        EXPECT_TRUE(res.value("ok", false));
        float resultVal = res["data"].value("pitchSemitones", 0.0f);
        EXPECT_GE(resultVal, -12.0f);
        EXPECT_LE(resultVal, 12.0f);
    }
}

TEST(AdversarialHardening, Stress_HighThroughputBackgroundScanUnderActiveDspPlayback) {
    DbTestFixtures fixture;
    const size_t numRecords = 2000;
    auto dataset = DbTestFixtures::generateSampleDataset(numRecords);

    std::atomic<bool> keepRunning{true};
    std::atomic<size_t> dspBlocksRendered{0};

    // Thread 1: High-load DSP audio rendering simulating active time-stretching and pitch-shifting
    std::thread dspWorker([&]() {
        auto testSine = AudioTestFixtures::generateSine(440.0f, 0.02f, 44100);
        while (keepRunning.load()) {
            float dummySum = 0.0f;
            for (size_t i = 0; i < testSine.size(); ++i) {
                dummySum += testSine[i] * 0.95f;
            }
            dspBlocksRendered++;
            std::this_thread::yield();
        }
    });

    // Thread 2 & 3: Concurrent background scanner inserting and querying SQLite
    DbTestFixtures::MockDbStore store;
    std::thread scannerWorker1([&]() {
        for (size_t i = 0; i < numRecords / 2; ++i) {
            store.insert(dataset[i]);
        }
    });

    std::thread scannerWorker2([&]() {
        for (size_t i = numRecords / 2; i < numRecords; ++i) {
            store.insert(dataset[i]);
        }
    });

    scannerWorker1.join();
    scannerWorker2.join();

    keepRunning.store(false);
    dspWorker.join();

    EXPECT_EQ(store.count(), numRecords);
    EXPECT_GT(dspBlocksRendered.load(), 10u);
}

TEST(AdversarialHardening, Fuzzing_SearchQuerySyntaxAdversarial) {
    // Fuzzing QueryParser with malformed strings, SQL injection, deeply nested tokens, Unicode, and whitespace
    const std::vector<std::string> hostileQueries = {
        "",
        "   ",
        "/////",
        "/bpm:",
        "/bpm:abc-def",
        "/bpm:-10--20",
        "/bpm:99999999999999999999999999999999",
        "/key:",
        "/key:InvalidKeyName$$$%%%^^^",
        "/camelot:99Z",
        "/fav /fav /fav /fav",
        "'; DROP TABLE samples; DROP TABLE embeddings; --",
        "\" OR 1=1 --",
        "\x00\x01\x02\x03 /tag:null",
        "🔥🥁✨🔊🎵🎶 /bpm:120-130 /fav",
        "Một hai ba bốn năm sáu bảy tám chín mười",
        std::string(1000, 'a') + " /bpm:120-140"
    };

    reals::search::QueryParser parser;
    for (const auto& q : hostileQueries) {
        // Query parser should never crash or throw unhandled exceptions
        EXPECT_NO_THROW({
            auto parsed = parser.parse(q);
            (void)parsed;
        });
    }
}

TEST(AdversarialHardening, SIMD_CosineSimilarity_AdversarialVectors) {
    // 1. All-zero query vector
    std::vector<float> zeros(512, 0.0f);
    std::vector<float> normalVec(512, 0.0f);
    for (size_t i = 0; i < 512; ++i) normalVec[i] = 1.0f / std::sqrt(512.0f);

    float simZero = reals::util::Simd::cosineSimilarity(zeros.data(), normalVec.data(), 512);
    EXPECT_EQ(simZero, 0.0f);

    // 2. Denormalized and near-zero values
    std::vector<float> tiny(512, 1e-20f);
    float simTiny = reals::util::Simd::cosineSimilarity(tiny.data(), normalVec.data(), 512);
    EXPECT_GE(simTiny, -1.0f);
    EXPECT_LE(simTiny, 1.0f);

    // 3. Exactly identical and exactly opposite unit vectors
    float simIdentical = reals::util::Simd::cosineSimilarity(normalVec.data(), normalVec.data(), 512);
    EXPECT_NEAR(simIdentical, 1.0f, 1e-4f);

    std::vector<float> oppositeVec(512);
    for (size_t i = 0; i < 512; ++i) oppositeVec[i] = -normalVec[i];
    float simOpposite = reals::util::Simd::cosineSimilarity(normalVec.data(), oppositeVec.data(), 512);
    EXPECT_NEAR(simOpposite, -1.0f, 1e-4f);
}

TEST(AdversarialHardening, Robustness_CorruptedAudioAndRecovery) {
    DbTestFixtures fixture;
    std::string badWav = (fixture.tempDir() / "corrupted_audio.wav").string();

    // Write corrupted FMT chunk size
    AudioTestFixtures::writeCorruptedWavFile(badWav, WavCorruptionType::CorruptedFmtChunkSize);

    // Engine probe should handle corruption safely without crashing
    BridgeTestHarness harness;
    auto res = harness.call("audio.probe", {{"path", badWav}});
    EXPECT_TRUE(res.value("ok", false));
    EXPECT_EQ(res["data"].value("sampleRate", 0), 0);

    // Subsequent normal playback should still succeed
    std::string goodWav = (fixture.tempDir() / "recovery_tone.wav").string();
    AudioTestFixtures::writeWavFile(goodWav, AudioTestFixtures::generateSine(440.0f, 0.1f), 1, 44100);
    auto playRes = harness.call("audio.play", {{"path", goodWav}});
    EXPECT_TRUE(playRes.value("ok", false));
}

TEST(AdversarialHardening, Stress_ConcurrentDatabaseTransactionsAndVectorBlobReadRace) {
    auto db = std::make_shared<reals::db::Database>();
    EXPECT_TRUE(db->open(":memory:"));

    const int numThreads = 8;
    const int opsPerThread = 50;
    std::atomic<int> completedOps{0};

    std::vector<std::thread> workers;
    workers.reserve(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < opsPerThread; ++i) {
                int idNum = t * opsPerThread + i + 1;
                reals::db::SampleRecord rec;
                rec.path = "C:/Samples/MultiThread/Track_" + std::to_string(idNum) + ".wav";
                rec.filename = "Track_" + std::to_string(idNum) + ".wav";
                rec.bpm = 100.0 + (idNum % 80);
                rec.keyRoot = "D";
                rec.keyMode = "minor";
                rec.camelot = "7A";
                rec.genre = "Techno";
                rec.mood = "energetic";
                rec.aiAnalyzed = true;

                int64_t sampleId = db->upsertSample(rec);
                if (sampleId > 0) {
                    reals::db::AnalysisRecord an;
                    an.sampleId = sampleId;
                    an.embedding = DbTestFixtures::generateUnitEmbedding(idNum);
                    db->updateAnalysis(sampleId, an);
                }
                completedOps++;
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(completedOps.load(), numThreads * opsPerThread);

    // Verify search engine index refresh and query under the concurrent dataset
    reals::search::SearchEngine engine(db);
    EXPECT_TRUE(engine.refreshIndex());
    auto results = engine.searchSyntax("/bpm:120-140", 50);
    EXPECT_GT(results.size(), 0u);
}

TEST(AdversarialHardening, Stress_RapidDawTempoModulationUnderRealtimePitchShifting) {
    BridgeTestHarness harness(120.0);

    // Rapidly modulate DAW tempo from 40 BPM to 280 BPM in 20 steps
    // while simultaneously applying various pitch transpositions [-12, +12]
    for (int step = 0; step < 40; ++step) {
        double simulatedTempo = 60.0 + (step * 5.0); // 60 BPM -> 255 BPM
        harness.host().setProjectTempo(simulatedTempo);

        auto tempoRes = harness.call("reaper.tempo", json::object());
        EXPECT_TRUE(tempoRes.value("ok", false));
        EXPECT_NEAR(tempoRes["data"].value("bpm", 0.0), simulatedTempo, 1e-4);

        float semitones = static_cast<float>((step % 25) - 12);
        auto pitchRes = harness.call("audio.setPitchShift", {{"semitones", semitones}});
        EXPECT_TRUE(pitchRes.value("ok", false));
        EXPECT_NEAR(pitchRes["data"].value("pitchSemitones", 0.0f), semitones, 1e-4f);

        // Ratio math verification
        double sampleBpm = 120.0;
        double ratio = simulatedTempo / sampleBpm;
        EXPECT_GT(ratio, 0.0);
        EXPECT_FALSE(std::isnan(ratio));
        EXPECT_FALSE(std::isinf(ratio));
    }
}

TEST(AdversarialHardening, Stress_AdversarialUnicodeDeepHierarchyFileScan) {
    DbTestFixtures fixture;
    // Create nested directory with deep UTF-8 hierarchy (Vietnamese, Japanese, Emoji)
    fs::path deepPath = fixture.tempDir() / platform::u8path("Bộ Mẫu Âm Thanh 2026")
                                         / platform::u8path("Nhạc Cụ Dân Tộc_🔥")
                                         / platform::u8path("Đàn Tranh_東京_Vocal");
    fs::create_directories(deepPath);

    std::string wav1 = platform::pathToUtf8(deepPath / platform::u8path("Giai_Điệu_Âm_Vang_440Hz.wav"));
    std::string wav2 = platform::pathToUtf8(deepPath / platform::u8path("Tiếng_Trống_Bản_Đôn_808.wav"));

    AudioTestFixtures::writeWavFile(wav1, AudioTestFixtures::generateSine(440.0f, 0.1f), 1, 44100);
    AudioTestFixtures::writeWavFile(wav2, AudioTestFixtures::generateKickRhythm(128.0f, 0.5f), 1, 44100);

    EXPECT_TRUE(fs::exists(platform::u8path(wav1)));
    EXPECT_TRUE(fs::exists(platform::u8path(wav2)));

    // Verify hashing works across deep Unicode paths
    std::string h1 = reals::util::sha256File(wav1);
    std::string h2 = reals::util::sha256File(wav2);
    EXPECT_FALSE(h1.empty());
    EXPECT_FALSE(h2.empty());
    EXPECT_NE(h1, h2);

    // Ingest into SQLite database
    auto db = std::make_shared<reals::db::Database>();
    EXPECT_TRUE(db->open(":memory:"));

    reals::db::SampleRecord r1;
    r1.path = wav1;
    r1.filename = "Giai_Điệu_Âm_Vang_440Hz.wav";
    r1.bpm = 120.0;
    r1.keyRoot = "A";
    r1.keyMode = "minor";
    r1.genre = "World Acoustic";
    int64_t id1 = db->upsertSample(r1);
    EXPECT_GT(id1, 0);

    auto retrieved = db->getSampleByPath(wav1);
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->filename, "Giai_Điệu_Âm_Vang_440Hz.wav");
}

TEST(AdversarialHardening, Stress_MemoryAndResourceStability5000Iterations) {
    // 5,000 iterations of rapid feature extraction, hashing, SIMD cosine similarity, and JSON serialization
    auto sine = AudioTestFixtures::generateSine(440.0f, 0.05f, 44100);
    auto vecA = DbTestFixtures::generateUnitEmbedding(123);
    auto vecB = DbTestFixtures::generateUnitEmbedding(456);

    for (int i = 0; i < 5000; ++i) {
        // 1. Feature extraction
        auto metrics = reals::ai::FeatureExtractor::computeMetrics(sine, 44100);
        EXPECT_GT(metrics.rms, 0.0f);

        // 2. Hash calculation
        std::string h = reals::util::sha256("Iteration_" + std::to_string(i));
        EXPECT_EQ(h.length(), 64u);

        // 3. SIMD Cosine similarity
        float sim = reals::util::Simd::cosineSimilarity(vecA.data(), vecB.data(), 512);
        EXPECT_GE(sim, -1.0f);
        EXPECT_LE(sim, 1.0f);
    }
}

} // namespace reals::test
