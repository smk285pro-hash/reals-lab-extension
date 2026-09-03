#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "../framework/DbTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"

#include <reals/browser/BrowserModel.h>
#include <reals/db/Database.h>
#include <reals/platform/Path.h>
#include <reals/search/QueryParser.h>
#include <reals/search/SearchEngine.h>

namespace fs = std::filesystem;

namespace reals::test {

class RequirementsR1R2R3Fixture : public reals::test::TestFixture {
public:
    std::string m_testDir;

    void SetUp() override {
        const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        m_testDir = platform::joinPath(platform::tempDir(), "reals_req_r1r2r3_" + std::to_string(nonce));
        std::error_code ec;
        fs::create_directories(platform::u8path(m_testDir), ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(platform::u8path(m_testDir), ec);
    }

    std::string createTestFile(const std::string& relPath, const std::string& content = "RIFF....WAVEfmt ....data....") {
        const std::string fullPath = platform::joinPath(m_testDir, relPath);
        std::error_code ec;
        fs::create_directories(platform::u8path(fullPath).parent_path(), ec);
        std::ofstream ofs(platform::u8path(fullPath), std::ios::binary);
        ofs << content;
        ofs.close();
        return platform::normalizePath(fullPath);
    }
};

// ============================================================================
// Requirement R3: Clean Initial Default Roots (0 Default Roots on Fresh Install)
// ============================================================================

TEST(Requirements_R3, FreshInstall_ZeroDefaultRoots) {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string storeFile = platform::joinPath(platform::tempDir(), "reals_fresh_test_" + std::to_string(nonce) + ".json");
    std::error_code ec;
    fs::remove(platform::u8path(storeFile), ec);

    reals::browser::BrowserModel model(storeFile);
    // On fresh initialization without a store file, roots must be completely empty (0 roots)
    const auto roots = model.roots();
    EXPECT_TRUE(roots.empty());
    EXPECT_EQ(roots.size(), 0u);

    // Verify hardcoded default OS folders are not present
    for (const auto& r : roots) {
        EXPECT_NE(r.name, "Music");
        EXPECT_NE(r.name, "Desktop");
        EXPECT_NE(r.name, "Downloads");
    }
    fs::remove(platform::u8path(storeFile), ec);
}

TEST(Requirements_R3, AddFirstRoot_CleanTransition) {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string storeFile = platform::joinPath(platform::tempDir(), "reals_add_test_" + std::to_string(nonce) + ".json");
    std::error_code ec;
    fs::remove(platform::u8path(storeFile), ec);

    reals::browser::BrowserModel model(storeFile);
    EXPECT_EQ(model.roots().size(), 0u);

    const std::string dummyPath = platform::normalizePath(platform::joinPath(platform::tempDir(), "SampleLibrary_Drums"));
    const bool added = model.addRoot("Drums", dummyPath);
    EXPECT_TRUE(added);

    const auto roots = model.roots();
    EXPECT_EQ(roots.size(), 1u);
    EXPECT_EQ(roots[0].name, "Drums");
    EXPECT_EQ(roots[0].path, dummyPath);

    // Adding duplicate root must return false
    const bool addedAgain = model.addRoot("Drums Duplicate", dummyPath);
    EXPECT_FALSE(addedAgain);
    EXPECT_EQ(model.roots().size(), 1u);
    fs::remove(platform::u8path(storeFile), ec);
}

TEST(Requirements_R3, RemoveLastRoot_RemainsEmpty) {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string storeFile = platform::joinPath(platform::tempDir(), "reals_remove_test_" + std::to_string(nonce) + ".json");
    std::error_code ec;
    fs::remove(platform::u8path(storeFile), ec);

    reals::browser::BrowserModel model(storeFile);
    const std::string pathA = platform::normalizePath(platform::joinPath(platform::tempDir(), "RootA"));
    const std::string pathB = platform::normalizePath(platform::joinPath(platform::tempDir(), "RootB"));

    model.addRoot("A", pathA);
    model.addRoot("B", pathB);
    EXPECT_EQ(model.roots().size(), 2u);

    model.removeRoot(0);
    EXPECT_EQ(model.roots().size(), 1u);
    EXPECT_EQ(model.roots()[0].name, "B");

    model.removeRoot(0);
    EXPECT_EQ(model.roots().size(), 0u);
    EXPECT_TRUE(model.roots().empty());
    fs::remove(platform::u8path(storeFile), ec);
}

TEST(Requirements_R3, StorePersistence_EmptyStoreIntegrity) {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string storeDir = platform::joinPath(platform::tempDir(), "reals_store_test_" + std::to_string(nonce));
    std::error_code ec;
    fs::create_directories(platform::u8path(storeDir), ec);
    const std::string storeFile = platform::joinPath(storeDir, "test_store.json");

    reals::browser::BrowserModel model(storeFile);
    EXPECT_EQ(model.roots().size(), 0u);

    // Save and reload clean empty model
    model.saveStore();
    model.loadStore();
    EXPECT_EQ(model.roots().size(), 0u);

    fs::remove_all(platform::u8path(storeDir), ec);
}

TEST(Requirements_R3, BridgeRPC_RootsCommandOnFreshInstall) {
    BridgeTestHarness harness;

    // Calling fs.roots on clean fresh instance returns 0 roots
    const auto res = harness.call("fs.roots", json::object());
    EXPECT_TRUE(res.value("ok", false));
    const auto data = res.value("data", json::array());
    EXPECT_TRUE(data.is_array());
    EXPECT_EQ(data.size(), 0u);
}

// ============================================================================
// Requirement R1: Global Favorites View (`★`)
// ============================================================================

TEST_F(RequirementsR1R2R3Fixture, GetFavoriteEntries_AggregatesAcrossMultipleFolders) {
    const std::string kickPath = createTestFile("Drums/Kicks/Kick_Punchy_01.wav");
    const std::string snarePath = createTestFile("Drums/Snares/Snare_Clean_02.wav");
    const std::string synthPath = createTestFile("Synths/Leads/Synth_Lead_128bpm_Am.wav");
    const std::string midiPath = createTestFile("MIDI/Melodies/Melody_Chords.mid");

    reals::browser::BrowserModel model(platform::joinPath(m_testDir, "fixture_store.json"));
    model.toggleFavorite(kickPath);
    model.toggleFavorite(synthPath);
    model.toggleFavorite(midiPath);

    EXPECT_TRUE(model.isFavorite(kickPath));
    EXPECT_FALSE(model.isFavorite(snarePath));
    EXPECT_TRUE(model.isFavorite(synthPath));
    EXPECT_TRUE(model.isFavorite(midiPath));

    const auto favEntries = model.getFavoriteEntries();
    EXPECT_EQ(favEntries.size(), 3u);

    bool foundKick = false;
    bool foundSynth = false;
    bool foundMidi = false;
    for (const auto& fe : favEntries) {
        EXPECT_FALSE(fe.isDir);
        EXPECT_FALSE(fe.name.empty());
        EXPECT_FALSE(fe.path.empty());
        if (fe.path == kickPath) {
            foundKick = true;
            EXPECT_EQ(fe.name, "Kick_Punchy_01.wav");
            EXPECT_EQ(fe.ext, "wav");
            EXPECT_TRUE(fe.isAudio);
        } else if (fe.path == synthPath) {
            foundSynth = true;
            EXPECT_EQ(fe.name, "Synth_Lead_128bpm_Am.wav");
            EXPECT_EQ(fe.ext, "wav");
            EXPECT_TRUE(fe.isAudio);
        } else if (fe.path == midiPath) {
            foundMidi = true;
            EXPECT_EQ(fe.name, "Melody_Chords.mid");
            EXPECT_EQ(fe.ext, "mid");
            EXPECT_TRUE(fe.isAudio);
        }
    }
    EXPECT_TRUE(foundKick);
    EXPECT_TRUE(foundSynth);
    EXPECT_TRUE(foundMidi);
}

TEST_F(RequirementsR1R2R3Fixture, GetFavoriteEntries_PrunesNonExistentOrDeletedFiles) {
    const std::string validWav = createTestFile("Audio/sample_valid.wav");
    const std::string nonExistentPath = platform::normalizePath(platform::joinPath(m_testDir, "Audio/deleted_sample.wav"));

    reals::browser::BrowserModel model(platform::joinPath(m_testDir, "fixture_store.json"));
    model.toggleFavorite(validWav);
    model.toggleFavorite(nonExistentPath);

    EXPECT_EQ(model.favorites().size(), 2u);

    // getFavoriteEntries() must only resolve valid existing files
    const auto entries = model.getFavoriteEntries();
    EXPECT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].path, validWav);
    EXPECT_EQ(entries[0].name, "sample_valid.wav");
}

TEST_F(RequirementsR1R2R3Fixture, GetFavoriteEntries_PreservesFileMetadataAndSorting) {
    const std::string pathA = createTestFile("FolderA/Alpha_Sample.wav", std::string(1024, 'A'));
    const std::string pathB = createTestFile("FolderB/Beta_Sample.flac", std::string(4096, 'B'));
    const std::string pathC = createTestFile("FolderC/Charlie_Sample.mp3", std::string(2048, 'C'));

    reals::browser::BrowserModel model(platform::joinPath(m_testDir, "fixture_store.json"));
    model.toggleFavorite(pathC);
    model.toggleFavorite(pathA);
    model.toggleFavorite(pathB);

    // Test sort by Name
    model.setSort(reals::browser::BrowserModel::Sort::Name);
    auto entriesByName = model.getFavoriteEntries();
    EXPECT_EQ(entriesByName.size(), 3u);
    EXPECT_EQ(entriesByName[0].name, "Alpha_Sample.wav");
    EXPECT_EQ(entriesByName[1].name, "Beta_Sample.flac");
    EXPECT_EQ(entriesByName[2].name, "Charlie_Sample.mp3");

    // Test sort by Size
    model.setSort(reals::browser::BrowserModel::Sort::Size);
    auto entriesBySize = model.getFavoriteEntries();
    EXPECT_EQ(entriesBySize.size(), 3u);
    EXPECT_EQ(entriesBySize[0].name, "Beta_Sample.flac"); // 4096 bytes
    EXPECT_EQ(entriesBySize[1].name, "Charlie_Sample.mp3"); // 2048 bytes
    EXPECT_EQ(entriesBySize[2].name, "Alpha_Sample.wav"); // 1024 bytes
}

TEST_F(RequirementsR1R2R3Fixture, BridgeRPC_GetFavoriteEntriesCommand) {
    const std::string file1 = createTestFile("Samples/fav_kick.wav");
    const std::string file2 = createTestFile("Samples/fav_snare.wav");

    BridgeTestHarness harness;

    // Toggle favorites
    harness.call("browser.toggleFavorite", {{"path", file1}});
    harness.call("browser.toggleFavorite", {{"path", file2}});

    // Call browser.getFavoriteEntries
    const auto res = harness.call("browser.getFavoriteEntries", json::object());
    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res.contains("data"));
    const auto files = res["data"].value("files", json::array());
    EXPECT_EQ(files.size(), 2u);

    bool hasFile1 = false;
    bool hasFile2 = false;
    for (const auto& f : files) {
        if (f.value("path", "") == file1) hasFile1 = true;
        if (f.value("path", "") == file2) hasFile2 = true;
        EXPECT_TRUE(f.value("isAudio", false));
        EXPECT_FALSE(f.value("isDir", true));
    }
    EXPECT_TRUE(hasFile1);
    EXPECT_TRUE(hasFile2);

    // Untag/unfavorite one file and verify live update
    harness.call("browser.toggleFavorite", {{"path", file1}});
    const auto res2 = harness.call("browser.getFavoriteEntries", json::object());
    const auto files2 = res2["data"].value("files", json::array());
    EXPECT_EQ(files2.size(), 1u);
    EXPECT_EQ(files2[0].value("path", ""), file2);
}

// ============================================================================
// Requirement R2: Global Multi-Root Search & Syntax Filters
// ============================================================================

TEST_F(RequirementsR1R2R3Fixture, GlobalSearch_MultiRootCrawlerFallback) {
    const std::string rootADir = platform::joinPath(m_testDir, "Root_Drums");
    const std::string rootBDir = platform::joinPath(m_testDir, "Root_Synths");
    const std::string rootCDir = platform::joinPath(m_testDir, "Root_Vocals");

    const std::string kickA = createTestFile("Root_Drums/Acoustic/Kick_Punchy.wav");
    const std::string snareA = createTestFile("Root_Drums/Electronic/Snare_808.wav");
    const std::string leadB = createTestFile("Root_Synths/Leads/Synth_Punchy_Lead.wav");
    const std::string vocalC = createTestFile("Root_Vocals/Hooks/Vocal_Punchy_Chant.wav");

    BridgeTestHarness harness;

    // Register 3 roots
    harness.call("fs.addRoot", {{"name", "Drums"}, {"path", rootADir}});
    harness.call("fs.addRoot", {{"name", "Synths"}, {"path", rootBDir}});
    harness.call("fs.addRoot", {{"name", "Vocals"}, {"path", rootCDir}});

    // Global Search for "Punchy" with empty base ""
    const auto searchRes = harness.call("browser.search", {{"base", ""}, {"query", "Punchy"}, {"gen", 1}});
    EXPECT_TRUE(searchRes.value("ok", false));

    // Wait for search result event
    bool receivedResult = false;
    for (int retry = 0; retry < 50; ++retry) {
        const auto rawEvents = harness.drainEvents();
        for (const auto& evStr : rawEvents) {
            try {
                auto ev = json::parse(evStr);
                if (ev.value("event", "") == "browser.searchResult" && ev.contains("data") && ev["data"].contains("results")) {
                    const auto results = ev["data"]["results"];
                    if (results.size() >= 3u) {
                        receivedResult = true;
                        bool foundKick = false;
                        bool foundSynth = false;
                        bool foundVocal = false;
                        for (const auto& r : results) {
                            const std::string p = r.value("path", "");
                            if (p == kickA) foundKick = true;
                            if (p == leadB) foundSynth = true;
                            if (p == vocalC) foundVocal = true;
                        }
                        EXPECT_TRUE(foundKick);
                        EXPECT_TRUE(foundSynth);
                        EXPECT_TRUE(foundVocal);
                        break;
                    }
                }
            } catch (...) {}
        }
        if (receivedResult) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(receivedResult);
}

TEST(Requirements_R2, QueryParser_SyntaxTokensComprehensive) {
    const auto parsed = reals::search::QueryParser::parse("/bpm:120-130 /key:Am /camelot:8A /genre:House /mood:Energetic /fav /trap punchy kick");
    EXPECT_NEAR(parsed.minBpm, 120.0f, 0.01f);
    EXPECT_NEAR(parsed.maxBpm, 130.0f, 0.01f);
    EXPECT_EQ(parsed.keyRoot, "A");
    EXPECT_EQ(parsed.keyMode, "minor");
    EXPECT_EQ(parsed.camelot, "8A");
    EXPECT_EQ(parsed.genre, "House");
    EXPECT_EQ(parsed.mood, "Energetic");
    EXPECT_TRUE(parsed.onlyFavorites);
    EXPECT_EQ(parsed.tags.size(), 1u);
    EXPECT_EQ(parsed.tags[0], "trap");
    EXPECT_EQ(parsed.freeText, "punchy kick");
}

TEST(Requirements_R2, GlobalSearch_EmptyQuerySafety) {
    reals::browser::BrowserModel model;
    const auto res = model.search("C:/NonExistentPath", "", false, 100);
    EXPECT_TRUE(res.empty());
}

// ============================================================================
// Requirement R2.3: SQLite Metadata Hydration in fs.list via getSamplesByPaths()
// ============================================================================

TEST_F(RequirementsR1R2R3Fixture, Database_GetSamplesByPaths_ChunkingAndBatchHydration) {
    const std::string dbPath = platform::joinPath(m_testDir, "test_batch.db");
    reals::db::Database db;
    EXPECT_TRUE(db.open(dbPath));

    // 1. Empty query returns empty map immediately
    auto emptyRes = db.getSamplesByPaths({});
    EXPECT_TRUE(emptyRes.empty());

    // 2. Insert 1000 sample records (spanning across multiple 400-path chunks: 400 + 400 + 200)
    constexpr size_t kSampleCount = 1000;
    std::vector<std::string> allPaths;
    allPaths.reserve(kSampleCount);

    {
        auto tx = db.makeTransaction();
        for (size_t i = 0; i < kSampleCount; ++i) {
            reals::db::SampleRecord rec;
            rec.path = platform::normalizePath(platform::joinPath(m_testDir, "Sample_" + std::to_string(i) + ".wav"));
            rec.filename = "Sample_" + std::to_string(i) + ".wav";
            rec.filesize = 1024 + (i * 10);
            rec.modifiedTime = 1700000000 + static_cast<int64_t>(i);
            rec.hash = "hash_" + std::to_string(i);
            rec.durationSec = 2.0 + (static_cast<double>(i) * 0.01);
            rec.sampleRate = 44100;
            rec.channels = 2;
            rec.bitDepth = 16;
            rec.bpm = 100.0 + (static_cast<double>(i % 60));
            rec.keyRoot = (i % 2 == 0) ? "C#" : "Am";
            rec.keyMode = (i % 2 == 0) ? "major" : "minor";
            rec.camelot = (i % 2 == 0) ? "3B" : "8A";
            rec.genre = "Trap";
            rec.mood = "Dark";

            allPaths.push_back(rec.path);
            EXPECT_GT(db.upsertSample(rec), 0);
        }
        EXPECT_TRUE(tx.commit());
    }

    // 3. Batch query for all 1000 paths
    auto batchRes = db.getSamplesByPaths(allPaths);
    EXPECT_EQ(batchRes.size(), kSampleCount);

    for (size_t i = 0; i < kSampleCount; ++i) {
        const auto& p = allPaths[i];
        auto it = batchRes.find(p);
        ASSERT_TRUE(it != batchRes.end());
        const auto& rec = it->second;
        EXPECT_EQ(rec.path, p);
        EXPECT_NEAR(rec.bpm, 100.0 + static_cast<double>(i % 60), 1e-4);
        EXPECT_EQ(rec.keyRoot, (i % 2 == 0) ? "C#" : "Am");
        EXPECT_EQ(rec.keyMode, (i % 2 == 0) ? "major" : "minor");
        EXPECT_EQ(rec.camelot, (i % 2 == 0) ? "3B" : "8A");
        EXPECT_NEAR(rec.durationSec, 2.0 + (static_cast<double>(i) * 0.01), 1e-4);
    }

    // 4. Mixed query: 500 valid paths + 500 non-existent paths
    std::vector<std::string> mixedPaths;
    mixedPaths.reserve(1000);
    for (size_t i = 0; i < 500; ++i) {
        mixedPaths.push_back(allPaths[i]);
        mixedPaths.push_back("C:/NonExistentFolder/GhostSample_" + std::to_string(i) + ".wav");
    }
    auto mixedRes = db.getSamplesByPaths(mixedPaths);
    EXPECT_EQ(mixedRes.size(), 500u);
    for (size_t i = 0; i < 500; ++i) {
        EXPECT_TRUE(mixedRes.find(allPaths[i]) != mixedRes.end());
    }

    // 5. UTF-8 Vietnamese and special character paths
    const std::string unicodePath1 = platform::normalizePath(platform::joinPath(m_testDir, "ÂmThanh/TiếngTrống_128BPM.wav"));
    const std::string unicodePath2 = platform::normalizePath(platform::joinPath(m_testDir, "Folder with spaces & 'quotes'/Synth_Am.wav"));
    {
        reals::db::SampleRecord r1;
        r1.path = unicodePath1;
        r1.filename = "TiếngTrống_128BPM.wav";
        r1.bpm = 128.0;
        r1.keyRoot = "F#";
        r1.keyMode = "minor";
        r1.camelot = "11A";
        r1.durationSec = 3.5;
        EXPECT_GT(db.upsertSample(r1), 0);

        reals::db::SampleRecord r2;
        r2.path = unicodePath2;
        r2.filename = "Synth_Am.wav";
        r2.bpm = 140.0;
        r2.keyRoot = "A";
        r2.keyMode = "minor";
        r2.camelot = "8A";
        r2.durationSec = 5.0;
        EXPECT_GT(db.upsertSample(r2), 0);
    }

    auto unicodeRes = db.getSamplesByPaths({unicodePath1, unicodePath2});
    EXPECT_EQ(unicodeRes.size(), 2u);
    EXPECT_EQ(unicodeRes[unicodePath1].bpm, 128.0);
    EXPECT_EQ(unicodeRes[unicodePath1].keyRoot, "F#");
    EXPECT_EQ(unicodeRes[unicodePath2].bpm, 140.0);
    EXPECT_EQ(unicodeRes[unicodePath2].keyRoot, "A");
}

TEST_F(RequirementsR1R2R3Fixture, BridgeRPC_FsList_MetadataBatchHydrationVerification) {
    const std::string sample1 = createTestFile("BatchBrowse/Kick_Punchy.wav");
    const std::string sample2 = createTestFile("BatchBrowse/Bass_Reese.wav");
    const std::string sample3 = createTestFile("BatchBrowse/Pad_Atmospheric.flac");
    const std::string textFile = createTestFile("BatchBrowse/readme.txt", "Sample pack info");
    const std::string subDir = platform::joinPath(m_testDir, "BatchBrowse/SubPreset");
    std::error_code ec;
    fs::create_directories(platform::u8path(subDir), ec);

    BridgeTestHarness harness;

    // Seed database with analyzed metadata for sample1 and sample2
    reals::db::Database db;
    EXPECT_TRUE(db.open("")); // Opens library.db shared with bridge
    {
        reals::db::SampleRecord r1;
        r1.path = sample1;
        r1.filename = "Kick_Punchy.wav";
        r1.bpm = 128.0;
        r1.keyRoot = "C";
        r1.keyMode = "major";
        r1.camelot = "8B";
        r1.durationSec = 1.85;
        db.upsertSample(r1);

        reals::db::SampleRecord r2;
        r2.path = sample2;
        r2.filename = "Bass_Reese.wav";
        r2.bpm = 174.0;
        r2.keyRoot = "F";
        r2.keyMode = "minor";
        r2.camelot = "4A";
        r2.durationSec = 4.25;
        db.upsertSample(r2);
    }

    const std::string folderPath = platform::normalizePath(platform::joinPath(m_testDir, "BatchBrowse"));
    const auto res = harness.call("fs.list", {{"path", folderPath}});
    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res.contains("data"));
    const auto files = res["data"];
    EXPECT_TRUE(files.is_array());
    EXPECT_EQ(files.size(), 3u);

    bool verifiedKick = false;
    bool verifiedBass = false;
    bool verifiedPad = false;

    for (const auto& f : files) {
        const std::string p = f.value("path", "");
        if (p == sample1) {
            verifiedKick = true;
            EXPECT_TRUE(f.value("isAudio", false));
            EXPECT_FALSE(f.value("isDir", true));
            EXPECT_NEAR(f.value("bpm", 0.0), 128.0, 1e-3);
            EXPECT_EQ(f.value("key", ""), "C");
            EXPECT_EQ(f.value("camelot", ""), "8B");
            EXPECT_NEAR(f.value("duration", 0.0), 1.85, 1e-3);
        } else if (p == sample2) {
            verifiedBass = true;
            EXPECT_TRUE(f.value("isAudio", false));
            EXPECT_FALSE(f.value("isDir", true));
            EXPECT_NEAR(f.value("bpm", 0.0), 174.0, 1e-3);
            EXPECT_EQ(f.value("key", ""), "Fm"); // minor mode produces "m" suffix
            EXPECT_EQ(f.value("camelot", ""), "4A");
            EXPECT_NEAR(f.value("duration", 0.0), 4.25, 1e-3);
        } else if (p == sample3) {
            verifiedPad = true;
            EXPECT_TRUE(f.value("isAudio", false));
            EXPECT_FALSE(f.value("isDir", true));
            // Unanalyzed sample has default 0 bpm and empty key
            EXPECT_EQ(f.value("bpm", 0.0), 0.0);
            EXPECT_EQ(f.value("key", ""), "");
        }
    }

    EXPECT_TRUE(verifiedKick);
    EXPECT_TRUE(verifiedBass);
    EXPECT_TRUE(verifiedPad);
}

} // namespace reals::test
