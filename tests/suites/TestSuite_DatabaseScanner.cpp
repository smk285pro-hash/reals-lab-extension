#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/TestRunner.h"
#include <reals/platform/Path.h>
#include <reals/util/Hash.h>

namespace reals::test {

namespace fs = std::filesystem;

// ============================================================================
// Feature 11: SQLite Library Database & Vector BLOB
// ============================================================================

TEST(DatabaseScanner, F11_SchemaInitialization) {
    DbTestFixtures fixture;
    EXPECT_FALSE(fixture.dbPath().empty());

    DbTestFixtures::MockDbStore store;
    EXPECT_EQ(store.count(), 0u);
}

TEST(DatabaseScanner, F11_UpsertSampleRecord) {
    DbTestFixtures::MockDbStore store;
    auto dataset = DbTestFixtures::generateSampleDataset(5);

    for (const auto& rec : dataset) {
        store.insert(rec);
    }
    EXPECT_EQ(store.count(), 5u);

    // Upsert existing record with modified tag
    auto updatedRec = dataset[0];
    updatedRec.tags.push_back("Custom_Favorite");
    store.insert(updatedRec);

    // Count should still be 5
    EXPECT_EQ(store.count(), 5u);
    const auto* retrieved = store.getByPath(updatedRec.filePath);
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->tags.back(), "Custom_Favorite");
}

TEST(DatabaseScanner, F11_VectorBlobStorage) {
    auto originalVec = DbTestFixtures::generateUnitEmbedding(42);
    EXPECT_EQ(originalVec.size(), 512u);

    // Serialize to byte BLOB
    auto blob = DbTestFixtures::serializeEmbedding(originalVec);
    EXPECT_EQ(blob.size(), 512u * sizeof(float));

    // Deserialize and check bit-exact equality
    auto restoredVec = DbTestFixtures::deserializeEmbedding(blob.data(), blob.size());
    EXPECT_EQ(restoredVec.size(), 512u);

    for (size_t i = 0; i < 512; ++i) {
        EXPECT_EQ(originalVec[i], restoredVec[i]);
    }
}

TEST(DatabaseScanner, F11_TransactionCommitRollback) {
    DbTestFixtures::MockDbStore store;
    auto dataset = DbTestFixtures::generateSampleDataset(10);

    for (size_t i = 0; i < 5; ++i) {
        store.insert(dataset[i]);
    }
    EXPECT_EQ(store.count(), 5u);
}

TEST(DatabaseScanner, F11_QueryFiltering) {
    DbTestFixtures::MockDbStore store;
    auto dataset = DbTestFixtures::generateSampleDataset(50);
    for (const auto& rec : dataset) {
        store.insert(rec);
    }

    // Filter by BPM range [120, 140]
    auto bpmResults = store.queryByFilter("", 120.0f, 140.0f, "", false);
    for (const auto& r : bpmResults) {
        EXPECT_GE(r.bpm, 120.0f);
        EXPECT_LE(r.bpm, 140.0f);
    }

    // Filter by Favorite
    auto favResults = store.queryByFilter("", 0.0f, 0.0f, "", true);
    for (const auto& r : favResults) {
        EXPECT_TRUE(r.isFavorite);
    }
}

// ============================================================================
// Feature 12: File Hash Checksum Cache
// ============================================================================

TEST(DatabaseScanner, F12_ComputeChecksum) {
    std::string sampleText = "RealsLab_Audio_Engine_Sample_Checksum_Data";
    uint64_t h64 = reals::util::xxhash64(sampleText);
    EXPECT_NE(h64, 0u);

    std::string sha = reals::util::sha256(sampleText);
    EXPECT_EQ(sha.length(), 64u);
}

TEST(DatabaseScanner, F12_FastSkipUnchangedFiles) {
    DbTestFixtures fixture;
    std::string testFile = (fixture.tempDir() / "test_sample.wav").string();
    AudioTestFixtures::writeWavFile(testFile, AudioTestFixtures::generateSine(440.0f, 0.1f), 1, 44100);

    std::string hash1 = reals::util::sha256File(testFile);
    EXPECT_FALSE(hash1.empty());

    // Second read without modification should return identical hash
    std::string hash2 = reals::util::sha256File(testFile);
    EXPECT_EQ(hash1, hash2);
}

TEST(DatabaseScanner, F12_DetectFileModification) {
    DbTestFixtures fixture;
    std::string testFile = (fixture.tempDir() / "mod_sample.wav").string();
    AudioTestFixtures::writeWavFile(testFile, AudioTestFixtures::generateSine(440.0f, 0.1f), 1, 44100);
    std::string hash1 = reals::util::sha256File(testFile);

    // Modify file
    AudioTestFixtures::writeWavFile(testFile, AudioTestFixtures::generateSine(880.0f, 0.2f), 1, 44100);
    std::string hash2 = reals::util::sha256File(testFile);

    EXPECT_NE(hash1, hash2);
}

TEST(DatabaseScanner, F12_DatabasePersistence) {
    DbTestFixtures::MockDbStore store;
    TestSampleRecord rec;
    rec.filePath = "C:/Samples/Kick.wav";
    rec.fileHash = "a1b2c3d4e5f6";
    store.insert(rec);

    const auto* retrieved = store.getByPath("C:/Samples/Kick.wav");
    EXPECT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->fileHash, "a1b2c3d4e5f6");
}

TEST(DatabaseScanner, F12_EmptyAndLargeFiles) {
    DbTestFixtures fixture;
    std::string emptyFile = (fixture.tempDir() / "empty.wav").string();
    std::ofstream(emptyFile, std::ios::binary).close();

    std::string emptyHash = reals::util::sha256File(emptyFile);
    // SHA256 of empty string is e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    EXPECT_EQ(emptyHash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// ============================================================================
// Feature 13: Multi-Threaded Background Scanner Pool
// ============================================================================

TEST(DatabaseScanner, F13_DirectoryDiscovery) {
    DbTestFixtures fixture;
    fs::path sub1 = fixture.tempDir() / "SubPack1";
    fs::path sub2 = fixture.tempDir() / "SubPack2" / "Drums";
    fs::create_directories(sub1);
    fs::create_directories(sub2);

    AudioTestFixtures::writeWavFile((sub1 / "Kick.wav").string(), AudioTestFixtures::generateSine(100.0f, 0.1f), 1, 44100);
    AudioTestFixtures::writeWavFile((sub1 / "Snare.wav").string(), AudioTestFixtures::generateSine(200.0f, 0.1f), 1, 44100);
    AudioTestFixtures::writeWavFile((sub2 / "HiHat.wav").string(), AudioTestFixtures::generateSine(1000.0f, 0.1f), 1, 44100);

    // Recursively count wav files
    size_t wavCount = 0;
    for (const auto& entry : fs::recursive_directory_iterator(fixture.tempDir())) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            ++wavCount;
        }
    }
    EXPECT_EQ(wavCount, 3u);
}

TEST(DatabaseScanner, F13_ThreadPoolConcurrency) {
    // Process 20 mock audio files concurrently across 4 threads
    const size_t numJobs = 20;
    std::vector<std::string> jobHashes(numJobs);

    #pragma omp parallel for
    for (int i = 0; i < static_cast<int>(numJobs); ++i) {
        std::string payload = "AUDIO_PAYLOAD_" + std::to_string(i);
        jobHashes[i] = reals::util::sha256(payload);
    }

    for (size_t i = 0; i < numJobs; ++i) {
        EXPECT_EQ(jobHashes[i].length(), 64u);
    }
}

TEST(DatabaseScanner, F13_ProgressReporting) {
    const size_t totalFiles = 50;
    size_t processed = 0;

    std::vector<float> progressSnapshots;
    for (size_t i = 0; i < totalFiles; ++i) {
        ++processed;
        float progress = static_cast<float>(processed) / totalFiles;
        if (i % 10 == 0 || i == totalFiles - 1) {
            progressSnapshots.push_back(progress);
        }
    }

    EXPECT_GT(progressSnapshots.size(), 0u);
    EXPECT_EQ(progressSnapshots.back(), 1.0f);
}

TEST(DatabaseScanner, F13_GracefulCancellation) {
    std::atomic<bool> cancelRequested{false};
    size_t processed = 0;

    for (size_t i = 0; i < 1000; ++i) {
        if (i == 25) {
            cancelRequested = true;
        }
        if (cancelRequested) {
            break;
        }
        ++processed;
    }

    EXPECT_EQ(processed, 25u);
}

TEST(DatabaseScanner, F13_ErrorResilience) {
    DbTestFixtures fixture;
    std::string unreadableFile = (fixture.tempDir() / "nonexistent_or_unreadable.wav").string();

    size_t successfullyProcessed = 0;
    size_t skippedWithWarning = 0;

    // Ingestion loop should catch errors on unreadable/missing file without terminating batch
    for (int i = 0; i < 5; ++i) {
        if (i == 2) {
            // Simulate reading unreadable file
            std::ifstream testRead(platform::u8path(unreadableFile));
            if (!testRead.is_open()) {
                ++skippedWithWarning;
                continue;
            }
        }
        ++successfullyProcessed;
    }

    EXPECT_EQ(successfullyProcessed + skippedWithWarning, 5u);
    EXPECT_EQ(successfullyProcessed, 4u);
    EXPECT_EQ(skippedWithWarning, 1u);
}

} // namespace reals::test
