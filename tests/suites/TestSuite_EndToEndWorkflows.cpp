#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
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
#include <reals/platform/Path.h>
#include <reals/util/Hash.h>

namespace reals::test {

namespace fs = std::filesystem;

// ============================================================================
// Tier 4: Real-World Workload Scenarios
// ============================================================================

TEST(EndToEndWorkflows, Workflow_Scenario1_ProducerSamplePackIngestion) {
    DbTestFixtures fixture;
    fs::path packRoot = fixture.tempDir() / "Producer_MegaPack";
    fs::create_directories(packRoot / "Kicks");
    fs::create_directories(packRoot / "Snares");
    fs::create_directories(packRoot / "Vocals");

    // 1. Synthesize 15 realistic audio samples in subfolders
    for (int i = 1; i <= 5; ++i) {
        std::string kPath = (packRoot / "Kicks" / ("Kick_" + std::to_string(i) + ".wav")).string();
        AudioTestFixtures::writeWavFile(kPath, AudioTestFixtures::generateKickRhythm(140.0f, 0.5f), 1, 44100);

        std::string sPath = (packRoot / "Snares" / ("Snare_" + std::to_string(i) + ".wav")).string();
        AudioTestFixtures::writeWavFile(sPath, AudioTestFixtures::generateSine(250.0f, 0.3f), 1, 44100);

        std::string vPath = (packRoot / "Vocals" / ("Vocal_" + std::to_string(i) + ".wav")).string();
        AudioTestFixtures::writeWavFile(vPath, AudioTestFixtures::generateChordTriad(220.0f, true, 1.0f), 1, 44100);
    }

    // 2. Scan and ingest files into DB
    DbTestFixtures::MockDbStore store;
    int64_t idCounter = 1;
    for (const auto& entry : fs::recursive_directory_iterator(packRoot)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            TestSampleRecord rec;
            rec.id = idCounter++;
            rec.filePath = entry.path().string();
            rec.fileName = entry.path().filename().string();
            rec.fileHash = reals::util::sha256File(rec.filePath);

            if (rec.filePath.find("Kick") != std::string::npos) {
                rec.bpm = 140.0f;
                rec.tags = {"Kick", "Trap"};
                rec.genres = {"Trap-EDM"};
            } else if (rec.filePath.find("Vocal") != std::string::npos) {
                rec.bpm = 120.0f;
                rec.keyName = "A Minor";
                rec.tags = {"Vocal", "Soul"};
                rec.genres = {"R&B"};
            } else {
                rec.bpm = 140.0f;
                rec.tags = {"Snare", "Punchy"};
                rec.genres = {"Hip Hop"};
            }
            store.insert(rec);
        }
    }

    EXPECT_EQ(store.count(), 15u);

    // 3. User performs search "/trap /bpm:135-145"
    auto queryResults = store.queryByFilter("Trap", 135.0f, 145.0f, "", false);
    EXPECT_EQ(queryResults.size(), 5u);

    // 4. User previews first result and inserts into DAW
    BridgeTestHarness harness(140.0);
    auto insertRes = harness.call("reaper.insert", {{"path", queryResults[0].filePath}});
    EXPECT_TRUE(insertRes.value("ok", false));

    auto inserted = harness.host().getInsertedMedia();
    EXPECT_EQ(inserted.size(), 1u);
    EXPECT_EQ(inserted[0].path, queryResults[0].filePath);
}

TEST(EndToEndWorkflows, Workflow_Scenario2_LiveRemixRapidAuditionAndKeyTranspose) {
    BridgeTestHarness harness(128.0); // DAW running at 128 BPM

    // 1. Rapidly switch preview tracks (20 iterations)
    for (int i = 0; i < 20; ++i) {
        std::string samplePath = "C:/Samples/Loop_" + std::to_string(i) + ".wav";
        auto playRes = harness.call("audio.play", {{"path", samplePath}, {"loop", true}});
        EXPECT_TRUE(playRes.value("ok", false));
    }

    // 2. User transposes key by +4 semitones via piano keyboard
    int semitoneOffset = 4;
    auto pitchRes = harness.call("audio.setPitchShift", {{"semitones", static_cast<float>(semitoneOffset)}});
    EXPECT_TRUE(pitchRes.value("ok", false));

    // 3. User resets to Original Key
    auto resetRes = harness.call("audio.setOriginalKey", json::object());
    EXPECT_TRUE(resetRes.value("ok", false));

    // 4. Drag & Drop into REAPER
    auto dragRes = harness.call("browser.beginDrag", {{"path", "C:/Samples/Loop_19.wav"}});
    EXPECT_TRUE(dragRes.value("ok", false));
}

TEST(EndToEndWorkflows, Workflow_Scenario3_HeavyIndexingUnderSimultaneousPlayback) {
    const size_t largeCount = 5000;
    auto dataset = DbTestFixtures::generateSampleDataset(largeCount);
    EXPECT_EQ(dataset.size(), largeCount);

    std::atomic<bool> stopFlag{false};
    std::atomic<size_t> playbackFramesProcessed{0};

    // Thread 1: Continuous Audio Playback DSP simulation
    std::thread audioThread([&]() {
        auto sine = AudioTestFixtures::generateSine(440.0f, 0.05f);
        while (!stopFlag.load()) {
            float sum = 0.0f;
            for (float s : sine) {
                sum += s * 0.9f;
            }
            playbackFramesProcessed += sine.size();
            std::this_thread::yield();
        }
    });

    // Thread 2: Heavy indexing of 5000 records
    DbTestFixtures::MockDbStore store;
    for (size_t i = 0; i < largeCount; ++i) {
        store.insert(dataset[i]);
    }

    stopFlag.store(true);
    audioThread.join();

    EXPECT_EQ(store.count(), largeCount);
    EXPECT_GT(playbackFramesProcessed.load(), 0u);
}

TEST(EndToEndWorkflows, Workflow_Scenario4_ErrorRecoveryAndGracefulDegradation) {
    BridgeTestHarness harness;

    // 1. Test missing file path in probe
    auto probeRes = harness.call("audio.probe", {{"path", "C:/NonExistent/ghost.wav"}});
    EXPECT_TRUE(probeRes.value("ok", false));
    EXPECT_EQ(probeRes["data"].value("sampleRate", 0), 0);

    // 2. Test malformed JSON syntax sent to bridge
    std::string malformedJson = "{id: 12, cmd: 'broken'";
    std::string rawResponse = harness.bridge().handle(malformedJson);
    json parsed = json::parse(rawResponse);
    EXPECT_FALSE(parsed.value("ok", true));

    // 3. Test non-existent RPC method
    auto badCmdRes = harness.call("unregistered_cmd", json::object());
    EXPECT_FALSE(badCmdRes.value("ok", true));

    // 4. Verify bridge remains healthy after all error cases
    auto healthRes = harness.call("app.info", json::object());
    EXPECT_TRUE(healthRes.value("ok", false));
    EXPECT_EQ(healthRes["data"].value("platform", ""), "windows");
}

} // namespace reals::test
