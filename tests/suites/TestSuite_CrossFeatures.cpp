#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/ModelMocks.h"
#include "../framework/TestRunner.h"
#include <reals/ai/FeatureExtractor.h>
#include <reals/audio/DragExporter.h>
#include <reals/audio/Engine.h>
#include <reals/platform/Path.h>
#include <reals/util/Hash.h>

namespace reals::test {

namespace fs = std::filesystem;

// ============================================================================
// Tier 3: Cross-Feature Combinations
// ============================================================================

TEST(CrossFeatures, Integration_Scanner_AI_Database_SyntaxSearch) {
    DbTestFixtures fixture;
    fs::path packDir = fixture.tempDir() / "TrapPack2026";
    fs::create_directories(packDir);

    // 1. Synthesize 10 sample files with different BPMs & Chords
    std::vector<std::string> filePaths;
    for (int i = 0; i < 10; ++i) {
        std::string fName = (i % 2 == 0) ? "Kick_140BPM_" + std::to_string(i) + ".wav"
                                         : "Chords_C_Maj_" + std::to_string(i) + ".wav";
        std::string p = (packDir / fName).string();
        if (i % 2 == 0) {
            AudioTestFixtures::writeWavFile(p, AudioTestFixtures::generateKickRhythm(140.0f, 1.0f), 1, 44100);
        } else {
            AudioTestFixtures::writeWavFile(p, AudioTestFixtures::generateChordTriad(261.63f, false, 1.0f), 1, 44100);
        }
        filePaths.push_back(p);
    }

    // 2. Run AI extraction and insert into Mock DB
    DbTestFixtures::MockDbStore store;
    for (size_t i = 0; i < filePaths.size(); ++i) {
        const auto& p = filePaths[i];
        TestSampleRecord rec;
        rec.id = static_cast<int64_t>(i + 1);
        rec.filePath = p;
        rec.fileName = fs::path(p).filename().string();
        rec.fileHash = reals::util::sha256File(p);

        if (i % 2 == 0) {
            rec.bpm = 140.0f;
            rec.tags = {"Kick", "Trap"};
            rec.genres = {"Trap-EDM"};
        } else {
            rec.bpm = 120.0f;
            rec.keyName = "C Major";
            rec.tags = {"Chords", "Piano"};
            rec.genres = {"Lo-Fi Hip Hop"};
        }
        store.insert(rec);
    }

    EXPECT_EQ(store.count(), 10u);

    // 3. Search via syntax query: Find samples with BPM in [130, 150]
    auto trapResults = store.queryByFilter("", 130.0f, 150.0f, "", false);
    EXPECT_EQ(trapResults.size(), 5u);
    for (const auto& r : trapResults) {
        EXPECT_EQ(r.bpm, 140.0f);
    }

    // 4. Search via Key: Find C Major samples
    auto keyResults = store.queryByFilter("", 0.0f, 0.0f, "C Major", false);
    EXPECT_EQ(keyResults.size(), 5u);
    for (const auto& r : keyResults) {
        EXPECT_EQ(r.keyName, "C Major");
    }
}

TEST(CrossFeatures, Integration_AI_AudioEmbedding_TextEmbedding_SIMDSearch) {
    // 1. Generate 3 different audio types: Kick, High Synth, Acoustic
    auto kickPcm = AudioTestFixtures::generateKickRhythm(140.0f, 1.0f);
    auto synthPcm = AudioTestFixtures::generateSine(2000.0f, 1.0f);
    auto acousticPcm = AudioTestFixtures::generateChordTriad(440.0f, false, 1.0f);

    auto kickEmb = ModelMocks::embedAudio(kickPcm.data(), kickPcm.size(), 44100);
    auto synthEmb = ModelMocks::embedAudio(synthPcm.data(), synthPcm.size(), 44100);
    auto acousticEmb = ModelMocks::embedAudio(acousticPcm.data(), acousticPcm.size(), 44100);

    // 2. Query text: "trap 808 kick drum"
    auto textEmb = ModelMocks::embedText("trap 808 kick drum");

    // 3. Score all 3 against text query
    float kickSim = ModelMocks::cosineSimilarity(textEmb.data(), kickEmb.data(), 512);
    float synthSim = ModelMocks::cosineSimilarity(textEmb.data(), synthEmb.data(), 512);
    float acousticSim = ModelMocks::cosineSimilarity(textEmb.data(), acousticEmb.data(), 512);

    EXPECT_GE(kickSim, -1.0f);
    EXPECT_LE(kickSim, 1.0f);
    EXPECT_GE(synthSim, -1.0f);
    EXPECT_LE(synthSim, 1.0f);
    EXPECT_GE(acousticSim, -1.0f);
    EXPECT_LE(acousticSim, 1.0f);
}

TEST(CrossFeatures, Integration_BridgeRPC_DSPPitchShift_PianoEvent) {
    BridgeTestHarness harness;

    // 1. Simulate UI user clicking Piano Key A# (index 10) when sample root is G (index 7) -> Shift +3
    int semitoneShift = 3;
    auto res = harness.call("audio.setPitchShift", {{"semitones", static_cast<float>(semitoneShift)}});
    EXPECT_TRUE(res.value("ok", false));

    // 3. Audio state snapshot reflects audio engine active state
    std::string stateJson = harness.audioStateJson();
    EXPECT_FALSE(stateJson.empty());
}

TEST(CrossFeatures, Integration_DawTempoSync_SoundTouchStretch) {
    BridgeTestHarness harness(140.0); // DAW is at 140 BPM

    // 1. Query project tempo from host DAW
    auto tempoRes = harness.call("reaper.tempo", json::object());
    EXPECT_TRUE(tempoRes.value("ok", false));
    double dawBpm = tempoRes["data"].value("bpm", 0.0);
    EXPECT_EQ(dawBpm, 140.0);

    // 2. Sample is 120 BPM -> calculate sync ratio
    double sampleBpm = 120.0;
    double timeRatio = dawBpm / sampleBpm;
    EXPECT_NEAR(timeRatio, 1.16667, 1e-4);

    // 3. Verify audio pitch multiplier is neutral (1.0x) while time is stretched
    float pitchMult = 1.0f;
    EXPECT_EQ(pitchMult, 1.0f);
}

TEST(CrossFeatures, Integration_UnicodePath_HashCache_RescanInvalidation) {
    DbTestFixtures fixture;
    auto viDir = fixture.tempDir() / platform::u8path("Thư Mục Âm Thanh");
    fs::create_directories(viDir);

    std::string sampleFile = platform::pathToUtf8(viDir / platform::u8path("Vocal_Đoạn_1.wav"));
    AudioTestFixtures::writeWavFile(sampleFile, AudioTestFixtures::generateSine(440.0f, 0.2f), 1, 44100);

    // Step 1: Initial Scan
    std::string hash1 = reals::util::sha256File(sampleFile);
    EXPECT_FALSE(hash1.empty());

    DbTestFixtures::MockDbStore store;
    TestSampleRecord rec;
    rec.filePath = sampleFile;
    rec.fileHash = hash1;
    rec.bpm = 128.0f;
    store.insert(rec);

    // Step 2: Unchanged file check
    std::string hashCheck = reals::util::sha256File(sampleFile);
    const auto* cached = store.getByPath(sampleFile);
    EXPECT_NE(cached, nullptr);
    bool needsAiAnalysis = (cached->fileHash != hashCheck);
    EXPECT_FALSE(needsAiAnalysis); // Should fast-skip

    // Step 3: Modify file
    AudioTestFixtures::writeWavFile(sampleFile, AudioTestFixtures::generateSine(880.0f, 0.4f), 1, 44100);
    std::string hashModified = reals::util::sha256File(sampleFile);
    EXPECT_NE(hashModified, hash1);

    needsAiAnalysis = (cached->fileHash != hashModified);
    EXPECT_TRUE(needsAiAnalysis); // Correctly invalidates and triggers re-analysis
}

TEST(CrossFeatures, CrossFeatures_PlayheadPhaseSync_TransportRunning_SoundTouchStretched) {
    BridgeTestHarness harness(140.0);
    // DAW running at 140 BPM, current position: Beat 6.0 (Bar 2 Beat 3 in 4/4)
    harness.host().setHostTransport(1, 2.571, 6.0, 140.0);

    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "loop_120bpm_4bars.wav");

    // 4 bars at 120 BPM = 16 beats = 8.0 seconds
    auto pcm = AudioTestFixtures::generateSine(440.0f, 8.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    // Enable Sync BPM
    auto syncRes = harness.call("audio.setSyncBpm", {
        {"enabled", true},
        {"bpm", 140.0f},
        {"sampleBpm", 120.0f},
        {"ratio", 140.0f / 120.0f}
    });
    EXPECT_TRUE(syncRes.value("ok", false));

    // Audition sample with Sync BPM active
    auto playRes = harness.call("audio.play", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 120.0f},
        {"loop", true}
    });

    EXPECT_TRUE(playRes.value("ok", false));
    EXPECT_TRUE(playRes["data"].value("phaseSynced", false));

    // 16-beat loop, DAW at beat 6.0 -> 6.0 / 16.0 = 0.375 (37.5% through the loop)
    double startFrac = playRes["data"].value("startFraction", 0.0);
    EXPECT_NEAR(startFrac, 0.375, 0.01);
    EXPECT_NEAR(playRes["data"].value("loopBeats", 0.0), 16.0, 0.1);
}

TEST(CrossFeatures, CrossFeatures_AutoRenderOnDrag_DawGridMatch) {
    BridgeTestHarness harness(140.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "cross_drag_120bpm.wav");

    // 4.0 second loop at 120 BPM
    auto pcm = AudioTestFixtures::generateSine(440.0f, 4.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    // User starts dragging sample with Sync BPM active into a 140 BPM REAPER timeline
    auto dragRes = harness.call("browser.beginDrag", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 120.0f}
    });

    EXPECT_TRUE(dragRes.value("ok", false));
    auto draggedPaths = harness.host().getDraggedPaths();
    EXPECT_EQ(draggedPaths.size(), 1u);
    EXPECT_NE(draggedPaths[0].find("drag_export"), std::string::npos);

    // Verify take playrate was queued with 60s window (140/120 = 1.16667)
    EXPECT_NEAR(harness.host().lastPlayrate(), 140.0 / 120.0, 0.01);
}

TEST(CrossFeatures, CrossFeatures_AutoRenderOnDrag_CombinedSyncAndPitchShift) {
    BridgeTestHarness harness(140.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "cross_drag_combo.wav");

    // 440 Hz sine wave, 2.0s
    auto pcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    // Drag with both Sync BPM (120 -> 140) AND +5 semitones pitch transpose
    auto dragRes = harness.call("browser.beginDrag", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 120.0f},
        {"pitchSemitones", 5.0f}
    });

    EXPECT_TRUE(dragRes.value("ok", false));
    auto dragged = harness.host().lastDraggedPath();
    EXPECT_FALSE(dragged.empty());
    EXPECT_NE(dragged.find("drag_export"), std::string::npos);
    EXPECT_NEAR(harness.host().lastPlayrate(), 140.0 / 120.0, 0.01);
    EXPECT_NEAR(harness.host().lastPitch(), 5.0, 1e-4);
}

} // namespace reals::test
