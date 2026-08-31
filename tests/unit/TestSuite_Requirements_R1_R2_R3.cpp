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
    reals::browser::BrowserModel model;
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
}

TEST(Requirements_R3, AddFirstRoot_CleanTransition) {
    reals::browser::BrowserModel model;
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
}

TEST(Requirements_R3, RemoveLastRoot_RemainsEmpty) {
    reals::browser::BrowserModel model;
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
}

TEST(Requirements_R3, StorePersistence_EmptyStoreIntegrity) {
    const auto nonce = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::string storeDir = platform::joinPath(platform::tempDir(), "reals_store_test_" + std::to_string(nonce));
    std::error_code ec;
    fs::create_directories(platform::u8path(storeDir), ec);

    reals::browser::BrowserModel model;
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

    reals::browser::BrowserModel model;
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

    reals::browser::BrowserModel model;
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

    reals::browser::BrowserModel model;
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

} // namespace reals::test
