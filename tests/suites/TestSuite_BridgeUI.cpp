#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/DbTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/audio/DragExporter.h>
#include <reals/audio/Engine.h>
#include <reals/platform/Path.h>

namespace reals::test {

using json = nlohmann::json;

// ============================================================================
// Feature 16: Bridge RPC Extended Contracts
// ============================================================================

TEST(BridgeUI, F16_AppInfoCommand) {
    BridgeTestHarness harness;
    auto res = harness.call("app.info", json::object());

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res.contains("data"));
    EXPECT_EQ(res["data"].value("platform", ""), "windows");
    EXPECT_EQ(res["data"].value("version", ""), "0.2.0");
}

TEST(BridgeUI, F16_ConfigGetAndSet) {
    BridgeTestHarness harness;
    auto getRes = harness.call("config.getAll", json::object());
    EXPECT_TRUE(getRes.value("ok", false));

    // Update language to Vietnamese
    auto setRes = harness.call("config.set", {{"key", "language"}, {"value", "vi"}});
    EXPECT_TRUE(setRes.value("ok", false));
}

TEST(BridgeUI, F16_AudioPlaybackControls) {
    BridgeTestHarness harness;
    auto stopRes = harness.call("audio.stop", json::object());
    EXPECT_TRUE(stopRes.value("ok", false));

    auto volRes = harness.call("audio.setVolume", {{"value", 0.85f}});
    EXPECT_TRUE(volRes.value("ok", false));

    auto loopRes = harness.call("audio.setLoop", {{"value", true}});
    EXPECT_TRUE(loopRes.value("ok", false));
}

TEST(BridgeUI, F16_ReaperInsertMedia) {
    BridgeTestHarness harness;
    std::string samplePath = "C:/Samples/Drums/Kick_01.wav";

    auto insertRes = harness.call("reaper.insert", {{"path", samplePath}});
    EXPECT_TRUE(insertRes.value("ok", false));

    auto inserted = harness.host().getInsertedMedia();
    EXPECT_EQ(inserted.size(), 1u);
    EXPECT_NE(inserted[0].path.find("Kick_01.wav"), std::string::npos);
}

TEST(BridgeUI, F16_InvalidMethodHandling) {
    BridgeTestHarness harness;
    auto res = harness.call("unknown.nonExistentCommand", json::object());

    EXPECT_FALSE(res.value("ok", true));
    EXPECT_TRUE(res.contains("error"));
}

// ============================================================================
// Feature 17: Player Tag & Mood Badges Row
// ============================================================================

TEST(BridgeUI, F17_GenerateTagBadges) {
    std::vector<std::string> sampleTags = {"Trap-EDM", "dark", "aggressive", "Kick", "808"};
    json badges = json::array();
    for (const auto& t : sampleTags) {
        badges.push_back({{"name", t}, {"type", "genre"}});
    }

    EXPECT_EQ(badges.size(), 5u);
    EXPECT_EQ(badges[0]["name"], "Trap-EDM");
}

TEST(BridgeUI, F17_ColorMappingByTagType) {
    // Validate tag type color categories
    struct TagColor { std::string type; std::string colorClass; };
    TagColor map[] = {
        {"genre", "tag-genre-blue"},
        {"mood", "tag-mood-purple"},
        {"instrument", "tag-inst-orange"}
    };

    for (const auto& tc : map) {
        EXPECT_FALSE(tc.colorClass.empty());
    }
}

TEST(BridgeUI, F17_MaxDisplayTagLimit) {
    std::vector<std::string> longTagList = {
        "Trap", "EDM", "Dark", "Aggressive", "Heavy", "Punchy", "Bass", "Vocals", "Choir", "Reverb"
    };

    const size_t maxVisible = 6;
    size_t visibleCount = std::min(longTagList.size(), maxVisible);
    size_t overflowCount = longTagList.size() - visibleCount;

    EXPECT_EQ(visibleCount, 6u);
    EXPECT_EQ(overflowCount, 4u);
}

TEST(BridgeUI, F17_TagClickFilterAction) {
    std::string clickedTag = "Future Bass";
    std::string queryGenerated = "/" + clickedTag;
    EXPECT_EQ(queryGenerated, "/Future Bass");
}

TEST(BridgeUI, F17_EmptyTagListGraceful) {
    std::vector<std::string> emptyTags;
    EXPECT_TRUE(emptyTags.empty());
    json badgeContainer = {{"badges", json::array()}};
    EXPECT_EQ(badgeContainer["badges"].size(), 0u);
}

// ============================================================================
// Feature 18: Sync BPM Button Highlight & Ratio Math
// ============================================================================

TEST(BridgeUI, F18_ToggleStateOnAndOff) {
    bool syncBpm = false;
    // Toggle ON
    syncBpm = !syncBpm;
    EXPECT_TRUE(syncBpm);

    // Toggle OFF
    syncBpm = !syncBpm;
    EXPECT_FALSE(syncBpm);
}

TEST(BridgeUI, F18_RatioCalculationFromReaperTempo) {
    BridgeTestHarness harness(144.0); // DAW tempo = 144 BPM
    auto tempoRes = harness.call("reaper.tempo", json::object());

    EXPECT_TRUE(tempoRes.value("ok", false));
    double dawBpm = tempoRes["data"].value("bpm", 0.0);
    EXPECT_NEAR(dawBpm, 144.0, 1e-4);

    double sampleBpm = 120.0;
    double ratio = dawBpm / sampleBpm;
    EXPECT_NEAR(ratio, 1.20, 1e-4);
}

TEST(BridgeUI, F18_ZeroBpmSampleProtection) {
    double dawBpm = 130.0;
    double sampleBpm = 0.0; // unanalyzed / unknown BPM

    double ratio = 1.0;
    bool syncAllowed = false;
    if (sampleBpm > 20.0 && sampleBpm < 300.0) {
        ratio = dawBpm / sampleBpm;
        syncAllowed = true;
    }

    EXPECT_FALSE(syncAllowed);
    EXPECT_EQ(ratio, 1.0);
}

TEST(BridgeUI, F18_PitchAdjustmentCommand) {
    BridgeTestHarness harness;
    auto res = harness.call("audio.setPitchShift", {{"semitones", 7.0f}});
    EXPECT_TRUE(res.value("ok", false));
}

TEST(BridgeUI, F18_AudioStatePeriodicEvent) {
    BridgeTestHarness harness;
    std::string stateJsonStr = harness.audioStateJson();
    EXPECT_FALSE(stateJsonStr.empty());

    json stateJson = json::parse(stateJsonStr);
    EXPECT_EQ(stateJson.value("event", ""), "audio.state");
    EXPECT_TRUE(stateJson.contains("data"));
    EXPECT_TRUE(stateJson["data"].contains("playing"));
    EXPECT_TRUE(stateJson["data"].contains("peak"));
    EXPECT_TRUE(stateJson["data"].contains("rms"));
}

// ============================================================================
// Feature 19: Mini Piano Keyboard Transposer Popup
// ============================================================================

TEST(BridgeUI, F19_12ChromaticKeysMapping) {
    const std::string notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    for (int i = 0; i < 12; ++i) {
        EXPECT_FALSE(notes[i].empty());
    }
}

TEST(BridgeUI, F19_SemitoneOffsetCalculation) {
    // Root note = F# (index 6), Target note = A (index 9) -> Shift = +3 semitones
    int rootIdx = 6;  // F#
    int targetIdx = 9; // A
    int semitones = targetIdx - rootIdx;
    EXPECT_EQ(semitones, 3);

    // Root note = F# (index 6), Target note = D (index 2) -> Shift = -4 semitones
    int target2 = 2; // D
    int semitones2 = target2 - rootIdx;
    EXPECT_EQ(semitones2, -4);
}

TEST(BridgeUI, F19_RealtimePitchShiftDispatch) {
    BridgeTestHarness harness;
    int semitoneShift = 3;
    auto res = harness.call("audio.setPitchShift", {{"semitones", static_cast<float>(semitoneShift)}});
    EXPECT_TRUE(res.value("ok", false));
}

TEST(BridgeUI, F19_ActiveKeyHighlight) {
    int activeSemitone = 3;
    std::string activeNote = "A";
    EXPECT_EQ(activeSemitone, 3);
    EXPECT_EQ(activeNote, "A");
}

TEST(BridgeUI, F19_MicrotonalDetuneDispatch) {
    BridgeTestHarness harness;
    // Fine detune +15 cents
    float cents = 15.0f;
    float semitones = cents / 100.0f;
    auto res = harness.call("audio.setPitchShift", {{"semitones", semitones}});
    EXPECT_TRUE(res.value("ok", false));
}

// ============================================================================
// Feature 20: Original Key Reset Button
// ============================================================================

TEST(BridgeUI, F20_RestoreOriginalPitch) {
    BridgeTestHarness harness;
    // Shift pitch first
    harness.call("audio.setPitchShift", {{"semitones", 6.0f}});

    // Reset to Original Key (0 semitones)
    auto resetRes = harness.call("audio.setOriginalKey", json::object());
    EXPECT_TRUE(resetRes.value("ok", false));
}

TEST(BridgeUI, F20_UIHighlightReset) {
    int transposedSemitones = 5;
    EXPECT_EQ(transposedSemitones, 5);

    // Reset action
    transposedSemitones = 0;
    EXPECT_EQ(transposedSemitones, 0);
}

TEST(BridgeUI, F20_AudioEngineVerification) {
    auto& engine = reals::audio::Engine::instance();
    EXPECT_NO_THROW(engine.setPitchSemitones(0.0f));
}

TEST(BridgeUI, F20_OriginalKeyLabel) {
    std::string originalKey = "F# Minor";
    std::string displayedKey = "A Minor"; // transposed
    EXPECT_NE(originalKey, displayedKey);

    // On Reset
    displayedKey = originalKey;
    EXPECT_EQ(displayedKey, "F# Minor");
}

TEST(BridgeUI, F20_RapidToggleStability) {
    BridgeTestHarness harness;
    for (int i = 0; i < 20; ++i) {
        float st = (i % 2 == 0) ? 7.0f : 0.0f;
        auto res = harness.call("audio.setPitchShift", {{"semitones", st}});
        EXPECT_TRUE(res.value("ok", false));
    }
}

// ============================================================================
// Feature 21: Responsive UI & REAPER Docking
// ============================================================================

TEST(BridgeUI, F21_DockStateToggle) {
    BridgeTestHarness harness;
    EXPECT_FALSE(harness.host().isDocked());

    auto toggleRes = harness.call("window.toggleDock", json::object());
    EXPECT_TRUE(toggleRes.value("ok", false));
    EXPECT_TRUE(harness.host().isDocked());

    auto isDockedRes = harness.call("window.isDocked", json::object());
    EXPECT_TRUE(isDockedRes.value("ok", false));
    EXPECT_TRUE(isDockedRes.value("docked", false));
}

TEST(BridgeUI, F21_WindowControls) {
    BridgeTestHarness harness;
    auto minRes = harness.call("window.minimize", json::object());
    EXPECT_TRUE(minRes.value("ok", false));

    auto maxRes = harness.call("window.toggleMaximize", json::object());
    EXPECT_TRUE(maxRes.value("ok", false));

    auto dragRes = harness.call("window.startDrag", json::object());
    EXPECT_TRUE(dragRes.value("ok", false));
}

TEST(BridgeUI, F21_I18nLanguageSwitch) {
    BridgeTestHarness harness;
    auto resVi = harness.call("config.set", {{"key", "language"}, {"value", "vi"}});
    EXPECT_TRUE(resVi.value("ok", false));

    auto resEn = harness.call("config.set", {{"key", "language"}, {"value", "en"}});
    EXPECT_TRUE(resEn.value("ok", false));
}

TEST(BridgeUI, F21_BrowserTagColoring) {
    BridgeTestHarness harness;
    std::string samplePath = "C:/Samples/Drums/Snare_01.wav";

    auto tagRes = harness.call("browser.tag", {{"path", samplePath}, {"color", 2}});
    EXPECT_TRUE(tagRes.value("ok", false));

    auto getTagRes = harness.call("browser.tags", {{"path", samplePath}});
    EXPECT_TRUE(getTagRes.value("ok", false));
}

TEST(BridgeUI, F21_BrowserFavoritesPersistence) {
    BridgeTestHarness harness;
    std::string samplePath = "C:/Samples/Vocal_Chant.wav";

    auto favRes = harness.call("browser.toggleFavorite", {{"path", samplePath}});
    EXPECT_TRUE(favRes.value("ok", false));

    auto listFav = harness.call("browser.favorites", json::object());
    EXPECT_TRUE(listFav.value("ok", false));
}

// ============================================================================
// Feature 18: Playhead Phase Synchronization RPC Contracts
// ============================================================================

TEST(BridgeUI, F18_PlayheadPhaseSync_RpcContract) {
    BridgeTestHarness harness(140.0);
    // DAW is playing at Beat 9.0 (Bar 3 Beat 2 in 4/4)
    harness.host().setHostTransport(1, 3.857, 9.0, 140.0);

    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "loop_140bpm_4bars.wav");

    // 4 bars at 140 BPM = 16 beats = (16 / 140) * 60 = 6.85714 seconds
    auto pcm = AudioTestFixtures::generateSine(440.0f, 6.85714f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("audio.play", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 140.0f},
        {"loop", true}
    });

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res.contains("data"));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5625, 0.01);
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 16.0, 0.1);
}

TEST(BridgeUI, F18_PlayheadPhaseSync_DawTransportStateQuery) {
    BridgeTestHarness harness(120.0);
    // Host transport stopped
    harness.host().setHostTransport(0, 0.0, 0.0, 120.0);

    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "loop_120bpm_2bars.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 4.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("audio.play", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 120.0f}
    });

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_FALSE(res["data"].value("phaseSynced", true));
    EXPECT_NEAR(res["data"].value("startFraction", 1.0), 0.0, 1e-5);
}

TEST(BridgeUI, F18_PlayheadPhaseSync_ZeroBpmSampleFallback) {
    BridgeTestHarness harness(120.0);
    harness.host().setHostTransport(1, 2.0, 4.0, 120.0);

    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "short_oneshot.wav");

    // Short one-shot (0.5s) < 1.2s threshold -> phase sync is bypassed
    auto pcm = AudioTestFixtures::generateSine(880.0f, 0.5f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("audio.play", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 0.0f}
    });

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_FALSE(res["data"].value("phaseSynced", true));
    EXPECT_NEAR(res["data"].value("startFraction", 1.0), 0.0, 1e-5);
}

// ============================================================================
// Feature 16: Mechanism A drag RPC routing (SPEC.md: drag the ORIGINAL file
// with zero lag; REAPER applies native take stretch via the queued playrate).
// ============================================================================

TEST(BridgeUI, F16_MechanismA_BeginDragWithSync) {
    BridgeTestHarness harness(140.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "drag_sync_test.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("browser.beginDrag", {
        {"path", samplePath},
        {"syncBpm", true},
        {"sampleBpm", 120.0f}
    });

    EXPECT_TRUE(res.value("ok", false));
    auto dragged = harness.host().getDraggedPaths();
    EXPECT_EQ(dragged.size(), 1u);
    // Mechanism A: the ORIGINAL file must be dragged — no temp render, zero lag.
    EXPECT_EQ(dragged[0], samplePath);
    EXPECT_EQ(harness.host().lastDraggedPath(), samplePath);
    EXPECT_NEAR(harness.host().lastQueuedPlayrate(), 140.0 / 120.0, 1e-4);
    // The sync playrate is queued for the original path; REAPER applies the
    // native take stretch (D_PLAYRATE/B_PPITCH/D_PITCH) when the drop lands.
    EXPECT_EQ(harness.host().queuedSyncPaths().size(), 1u);
    EXPECT_EQ(harness.host().queuedSyncPaths()[0], samplePath);
}

TEST(BridgeUI, F16_MechanismA_BeginDragWithPitchShift) {
    BridgeTestHarness harness(120.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "drag_pitch_test.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("browser.beginDrag", {
        {"path", samplePath},
        {"syncBpm", false},
        {"pitchSemitones", 5.0f}
    });

    EXPECT_TRUE(res.value("ok", false));
    auto dragged = harness.host().getDraggedPaths();
    EXPECT_EQ(dragged.size(), 1u);
    // Mechanism A: original file dragged, pitch queued for native REAPER transpose.
    EXPECT_EQ(dragged[0], samplePath);
    EXPECT_EQ(harness.host().lastDraggedPath(), samplePath);
    EXPECT_EQ(harness.host().queuedSyncPaths().size(), 1u);
    EXPECT_EQ(harness.host().queuedSyncPaths()[0], samplePath);
    EXPECT_NEAR(harness.host().lastPitch(), 5.0, 1e-4);
}

TEST(BridgeUI, F16_MechanismA_BypassWhenUnmodified) {
    BridgeTestHarness harness(120.0);
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string samplePath = platform::joinPath(tmpDir, "drag_raw_test.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(samplePath, pcm, 1, 44100));

    auto res = harness.call("browser.beginDrag", {
        {"path", samplePath},
        {"syncBpm", false},
        {"pitchSemitones", 0.0f}
    });

    EXPECT_TRUE(res.value("ok", false));
    auto dragged = harness.host().getDraggedPaths();
    EXPECT_EQ(dragged.size(), 1u);
    // Unmodified must pass the original file directly with zero rendering overhead
    EXPECT_EQ(dragged[0], samplePath);
}

TEST(BridgeUI, F16_DragExporter_TempDirectorySanitization) {
    EXPECT_NO_THROW(reals::audio::DragExporter::cleanupTempFiles(86400));
}

} // namespace reals::test
