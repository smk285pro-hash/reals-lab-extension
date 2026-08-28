#include "framework/AudioTestFixtures.h"
#include "framework/TestRunner.h"

#include "reals/db/Database.h"
#include "reals/db/SampleRecord.h"
#include "reals/platform/Path.h"
#include "reals/scanner/BackgroundScanner.h"
#include "reals/util/Hash.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace reals::db;
using namespace reals::scanner;
using namespace reals::util;
using namespace reals::platform;
using namespace reals::test;

// =============================================================================
// Suite 1: Hash Utilities (xxHash64 & SHA-256)
// =============================================================================

TEST(HashSuite, Sha256StandardVectors) {
    // Empty string
    EXPECT_EQ(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // "abc"
    EXPECT_EQ(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Standard phrase
    EXPECT_EQ(sha256("The quick brown fox jumps over the lazy dog"),
              "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

TEST(HashSuite, XxHash64DeterminismAndStreaming) {
    std::string data = "RealsLab_Audio_Engine_Sample_Checksum_Data_Verification_String_123456789";
    uint64_t direct = Hash::xx64(data);
    EXPECT_NE(direct, 0ULL);

    // Streaming verification
    XxHash64 streamHasher;
    streamHasher.update(data.substr(0, 10));
    streamHasher.update(data.substr(10, 20));
    streamHasher.update(data.substr(30));
    EXPECT_EQ(streamHasher.digest(), direct);

    // Different seed produces different hash
    uint64_t seeded = Hash::xx64(data, 12345);
    EXPECT_NE(direct, seeded);
}

TEST(HashSuite, FileHashingAndUnchangedCheck) {
    const auto tempDir = fs::temp_directory_path() / "reals_hash_test";
    fs::create_directories(tempDir);
    const std::string filePath = (tempDir / "test_audio.wav").string();

    // Write file
    {
        std::ofstream out(reals::platform::u8path(filePath), std::ios::binary);
        out << "RIFF....WAVEfmt ....data....SampleAudioData123";
    }

    const std::string hash1 = Hash::fileXx64Hex(filePath);
    EXPECT_FALSE(hash1.empty());

    const std::string sha1 = Hash::fileSha256Hex(filePath);
    EXPECT_FALSE(sha1.empty());

    std::error_code ec;
    const uint64_t size = fs::file_size(reals::platform::u8path(filePath), ec);
    const auto ftime = fs::last_write_time(reals::platform::u8path(filePath), ec);
    const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    const int64_t mtime = std::chrono::duration_cast<std::chrono::seconds>(sysTime.time_since_epoch()).count();

    // Fast check should confirm unchanged
    EXPECT_TRUE(Hash::isFileUnchanged(filePath, size, mtime, hash1));

    // Wrong size should fail
    EXPECT_FALSE(Hash::isFileUnchanged(filePath, size + 10, mtime, hash1));

    fs::remove_all(tempDir, ec);
}

// =============================================================================
// Suite 2: SQLite Database (`reals::db::Database`)
// =============================================================================

TEST(DatabaseSuite, OpenMemoryAndFile) {
    Database db;
    EXPECT_FALSE(db.isOpen());

    EXPECT_TRUE(db.open(":memory:"));
    EXPECT_TRUE(db.isOpen());
    EXPECT_EQ(db.getSampleCount(), 0u);
    EXPECT_EQ(db.getAnalyzedCount(), 0u);
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST(DatabaseSuite, SampleCrudOperations) {
    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    SampleRecord rec;
    rec.path = "C:/Samples/Drums/Kick_01.wav";
    rec.filename = "Kick_01.wav";
    rec.filesize = 44100 * 2 * 2;
    rec.modifiedTime = 1700000000;
    rec.hash = "abcdef1234567890";
    rec.durationSec = 1.0;
    rec.sampleRate = 44100;
    rec.channels = 2;
    rec.bitDepth = 16;
    rec.bpm = 128.0;
    rec.keyRoot = "C";
    rec.keyMode = "minor";
    rec.camelot = "5A";
    rec.genre = "Tech House";
    rec.mood = "punchy";

    int64_t id = db.upsertSample(rec);
    EXPECT_GT(id, 0);
    EXPECT_EQ(db.getSampleCount(), 1u);

    // Get by ID
    auto fetched = db.getSampleById(id);
    EXPECT_TRUE(fetched.has_value());
    if (fetched.has_value()) {
        EXPECT_EQ(fetched->filename, "Kick_01.wav");
        EXPECT_EQ(fetched->bpm, 128.0);
        EXPECT_EQ(fetched->genre, "Tech House");
        EXPECT_EQ(fetched->hash, "abcdef1234567890");
    }

    // Get by path
    auto fetchedByPath = db.getSampleByPath(rec.path);
    EXPECT_TRUE(fetchedByPath.has_value());
    if (fetchedByPath.has_value()) {
        EXPECT_EQ(fetchedByPath->id, id);
    }

    // Get by hash
    auto fetchedByHash = db.getSampleByHash(rec.hash);
    EXPECT_TRUE(fetchedByHash.has_value());
    if (fetchedByHash.has_value()) {
        EXPECT_EQ(fetchedByHash->id, id);
    }

    // Upsert update
    rec.bpm = 130.0;
    rec.genre = "Deep House";
    int64_t updatedId = db.upsertSample(rec);
    EXPECT_EQ(updatedId, id);
    EXPECT_EQ(db.getSampleCount(), 1u);

    auto refetched = db.getSampleById(id);
    EXPECT_TRUE(refetched.has_value());
    if (refetched.has_value()) {
        EXPECT_EQ(refetched->bpm, 130.0);
        EXPECT_EQ(refetched->genre, "Deep House");
    }

    // Delete
    EXPECT_TRUE(db.deleteSample(id));
    EXPECT_EQ(db.getSampleCount(), 0u);
    EXPECT_FALSE(db.getSampleById(id).has_value());
}

TEST(DatabaseSuite, AnalysisWithEmbeddingBlob) {
    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    SampleRecord sample;
    sample.path = "C:/Samples/Lead/Synth_01.wav";
    sample.filename = "Synth_01.wav";
    sample.filesize = 10000;
    sample.modifiedTime = 1700000000;
    sample.hash = "deadbeef12345678";
    int64_t sampleId = db.upsertSample(sample);
    EXPECT_GT(sampleId, 0);

    AnalysisRecord analysis;
    analysis.sampleId = sampleId;
    analysis.tempoConfidence = 0.95;
    analysis.keyConfidence = 0.88;
    analysis.genreTags = {"Future Bass", "Trap-EDM", "Melodic Dubstep"};
    analysis.moodTags = {"happy", "energetic", "bright"};

    // 512-dim float embedding vector
    std::vector<float> embedding(512);
    for (size_t i = 0; i < 512; ++i) {
        embedding[i] = static_cast<float>(i) * 0.001953125f;
    }
    analysis.embedding = embedding;

    EXPECT_TRUE(db.updateAnalysis(sampleId, analysis));
    EXPECT_EQ(db.getAnalyzedCount(), 1u);

    // Fetch analysis and verify bit-exact embedding
    auto fetched = db.getAnalysis(sampleId);
    EXPECT_TRUE(fetched.has_value());
    if (fetched.has_value()) {
        EXPECT_NEAR(fetched->tempoConfidence, 0.95, 0.0001);
        EXPECT_NEAR(fetched->keyConfidence, 0.88, 0.0001);
        EXPECT_EQ(fetched->genreTags.size(), 3u);
        EXPECT_EQ(fetched->genreTags[0], "Future Bass");
        EXPECT_EQ(fetched->moodTags.size(), 3u);
        EXPECT_EQ(fetched->moodTags[0], "happy");
        EXPECT_EQ(fetched->embedding.size(), 512u);

        for (size_t i = 0; i < 512; ++i) {
            EXPECT_EQ(fetched->embedding[i], embedding[i]);
        }
    }

    // Verify getAllEmbeddings
    auto allEmbeddings = db.getAllEmbeddings();
    EXPECT_EQ(allEmbeddings.size(), 1u);
    if (!allEmbeddings.empty()) {
        EXPECT_EQ(allEmbeddings[0].first, sampleId);
        EXPECT_EQ(allEmbeddings[0].second.size(), 512u);
    }
}

TEST(DatabaseSuite, UserTagsOperations) {
    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    SampleRecord sample;
    sample.path = "C:/Samples/Guitar/Acoustic_01.wav";
    sample.filename = "Acoustic_01.wav";
    sample.hash = "1122334455667788";
    int64_t sampleId = db.upsertSample(sample);

    EXPECT_TRUE(db.addUserTag(sampleId, "Favorites"));
    EXPECT_TRUE(db.addUserTag(sampleId, "Warm"));
    EXPECT_TRUE(db.addUserTag(sampleId, "Lo-Fi"));

    // Duplicate tag should be ignored cleanly
    EXPECT_TRUE(db.addUserTag(sampleId, "Favorites"));

    auto tags = db.getUserTags(sampleId);
    EXPECT_EQ(tags.size(), 3u);

    EXPECT_TRUE(db.removeUserTag(sampleId, "Warm"));
    tags = db.getUserTags(sampleId);
    EXPECT_EQ(tags.size(), 2u);
}

TEST(DatabaseSuite, QueryFilters) {
    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    for (int i = 1; i <= 20; ++i) {
        SampleRecord rec;
        rec.path = "C:/Samples/Pack/Sample_" + std::to_string(i) + ".wav";
        rec.filename = "Sample_" + std::to_string(i) + ".wav";
        rec.filesize = 1000 * i;
        rec.modifiedTime = 1700000000 + i;
        rec.hash = "hash_" + std::to_string(i);
        rec.bpm = 100.0 + i * 2.0; // 102..140
        rec.keyRoot = (i % 2 == 0) ? "C" : "F#";
        rec.keyMode = (i % 2 == 0) ? "major" : "minor";
        rec.genre = (i <= 10) ? "Hip Hop" : "EDM";
        rec.mood = (i % 3 == 0) ? "dark" : "happy";
        int64_t id = db.upsertSample(rec);

        if (i % 5 == 0) {
            db.addUserTag(id, "Starred");
        }
    }

    EXPECT_EQ(db.getSampleCount(), 20u);

    // 1. Filter by text
    QueryFilter filterText;
    filterText.text = "Sample_1";
    auto textResults = db.querySamples(filterText);
    // Matches Sample_1, Sample_10..Sample_19 (total 11)
    EXPECT_EQ(textResults.size(), 11u);

    // 2. Filter by BPM range [120, 130]
    QueryFilter filterBpm;
    filterBpm.minBpm = 120.0;
    filterBpm.maxBpm = 130.0;
    auto bpmResults = db.querySamples(filterBpm);
    for (const auto& r : bpmResults) {
        EXPECT_GE(r.bpm, 120.0);
        EXPECT_LE(r.bpm, 130.0);
    }

    // 3. Filter by genre
    QueryFilter filterGenre;
    filterGenre.genre = "EDM";
    auto genreResults = db.querySamples(filterGenre);
    EXPECT_EQ(genreResults.size(), 10u);

    // 4. Filter by user tag
    QueryFilter filterTag;
    filterTag.userTag = "Starred";
    auto tagResults = db.querySamples(filterTag);
    EXPECT_EQ(tagResults.size(), 4u); // 5, 10, 15, 20
}

TEST(DatabaseSuite, TransactionsCommitAndRollback) {
    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    SampleRecord rec1;
    rec1.path = "C:/Samples/1.wav";
    rec1.filename = "1.wav";
    rec1.hash = "h1";

    // Successful transaction
    {
        auto tx = db.makeTransaction();
        db.upsertSample(rec1);
        EXPECT_TRUE(tx.commit());
    }
    EXPECT_EQ(db.getSampleCount(), 1u);

    // Rolled back transaction
    SampleRecord rec2;
    rec2.path = "C:/Samples/2.wav";
    rec2.filename = "2.wav";
    rec2.hash = "h2";
    {
        auto tx = db.makeTransaction();
        db.upsertSample(rec2);
        // Exiting scope without commit triggers rollback
    }
    EXPECT_EQ(db.getSampleCount(), 1u);
}

// =============================================================================
// Suite 3: Background Scanner Pool (`reals::scanner::BackgroundScanner`)
// =============================================================================

TEST(ScannerSuite, SupportedAudioExtensions) {
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("kick.wav"));
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("snare.WAV"));
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("synth.flac"));
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("loop.mp3"));
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("ambient.aiff"));
    EXPECT_TRUE(BackgroundScanner::isSupportedAudioExtension("midi.mid"));

    EXPECT_FALSE(BackgroundScanner::isSupportedAudioExtension("document.txt"));
    EXPECT_FALSE(BackgroundScanner::isSupportedAudioExtension("image.png"));
    EXPECT_FALSE(BackgroundScanner::isSupportedAudioExtension("video.mp4"));
    EXPECT_FALSE(BackgroundScanner::isSupportedAudioExtension("no_extension"));
}

TEST(ScannerSuite, RecursiveScanAndCaching) {
    const auto tempDir = fs::temp_directory_path() / "reals_scanner_test";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir / "Drums" / "Kicks");
    fs::create_directories(tempDir / "Synths");
    fs::create_directories(tempDir / ".ignored_folder");

    // Write dummy audio files
    const auto writeDummyFile = [](const fs::path& p, const std::string& content) {
        std::ofstream out(reals::platform::u8path(p.string()), std::ios::binary);
        out << content;
    };

    writeDummyFile(tempDir / "Drums" / "Kicks" / "Kick_01.wav", "DUMMY_KICK_01_DATA_123");
    writeDummyFile(tempDir / "Drums" / "Kicks" / "Kick_02.wav", "DUMMY_KICK_02_DATA_456");
    writeDummyFile(tempDir / "Synths" / "Lead_01.wav", "DUMMY_SYNTH_01_DATA_789");
    writeDummyFile(tempDir / "Synths" / "Readme.txt", "This is not an audio file");
    writeDummyFile(tempDir / ".ignored_folder" / "Hidden.wav", "Hidden audio file");

    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    BackgroundScanner scanner(db);
    ScanOptions opts;
    opts.numThreads = 2;
    opts.extractAudioInfo = false; // dummy files

    bool progressCalled = false;
    scanner.setProgressCallback([&](const ScanProgress&) {
        progressCalled = true;
    });

    // Start initial scan
    EXPECT_TRUE(scanner.startScan({tempDir.string()}, opts));
    scanner.waitForCompletion();

    EXPECT_TRUE(progressCalled);
    auto p1 = scanner.getProgress();
    EXPECT_EQ(p1.totalFiles, 3);
    EXPECT_EQ(p1.addedCount, 3);
    EXPECT_EQ(p1.skippedCount, 0);
    EXPECT_EQ(db.getSampleCount(), 3u);

    // Second scan without modifications -> should fast skip all 3 files
    EXPECT_TRUE(scanner.startScan({tempDir.string()}, opts));
    scanner.waitForCompletion();

    auto p2 = scanner.getProgress();
    EXPECT_EQ(p2.totalFiles, 3);
    EXPECT_EQ(p2.addedCount, 0);
    EXPECT_EQ(p2.skippedCount, 3);
    EXPECT_EQ(db.getSampleCount(), 3u);

    // Modify one file and rescan
    writeDummyFile(tempDir / "Synths" / "Lead_01.wav", "MODIFIED_SYNTH_DATA_NEW");
    // Ensure mtime updates
    fs::last_write_time(reals::platform::u8path((tempDir / "Synths" / "Lead_01.wav").string()),
                        fs::file_time_type::clock::now() + std::chrono::seconds(5), ec);

    EXPECT_TRUE(scanner.startScan({tempDir.string()}, opts));
    scanner.waitForCompletion();

    auto p3 = scanner.getProgress();
    EXPECT_EQ(p3.totalFiles, 3);
    EXPECT_EQ(p3.updatedCount, 1);
    EXPECT_EQ(p3.skippedCount, 2);
    EXPECT_EQ(db.getSampleCount(), 3u);

    fs::remove_all(tempDir, ec);
}

TEST(ScannerSuite, PauseResumeAndCancel) {
    const auto tempDir = fs::temp_directory_path() / "reals_cancel_test";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir);

    for (int i = 0; i < 50; ++i) {
        std::ofstream out(reals::platform::u8path((tempDir / ("Sample_" + std::to_string(i) + ".wav")).string()), std::ios::binary);
        out << "AUDIO_SAMPLE_DATA_" << i;
    }

    Database db;
    EXPECT_TRUE(db.open(":memory:"));

    BackgroundScanner scanner(db);
    ScanOptions opts;
    opts.numThreads = 1;
    opts.extractAudioInfo = false;

    EXPECT_TRUE(scanner.startScan({tempDir.string()}, opts));
    EXPECT_TRUE(scanner.isScanning());

    scanner.pause();
    EXPECT_TRUE(scanner.isPaused());

    scanner.resume();
    EXPECT_FALSE(scanner.isPaused());

    scanner.cancel();
    scanner.waitForCompletion();

    EXPECT_FALSE(scanner.isScanning());
    EXPECT_TRUE(scanner.isCancelled());

    fs::remove_all(tempDir, ec);
}

// =============================================================================
// Main Runner Entrypoint
// =============================================================================

int main(int argc, char** argv) {
    return TestRunner::run(argc, argv);
}
