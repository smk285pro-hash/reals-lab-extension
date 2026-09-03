// ============================================================================
// NativePhaseSnap — phase-snap correctness on the REAPER PlayPreviewEx path
//
// The Bridge has two playback paths: the Engine/SoundTouch fallback (covered by
// TestSuite_PhaseSyncDiagnostics) and the native REAPER preview (PlayPreviewEx).
// Until now MockHostActions did not override playHostPreview, so every Bridge
// test silently exercised only the fallback. This suite drives the native path
// and reproduces four concrete phase-snap defects:
//
//   N1 (bug #2): startPosSeconds is computed against the FULL file duration
//                instead of the nominal bar-quantized loop, so a loop with a
//                reverb tail starts phases off by the tail length.
//   N2 (bug #1): contract guard — Bridge must hand loopBeats + nominalLoopFrames
//                to the host so the preview can wrap on the bar grid.
//   N3 (bug #3): live re-phase on audio.setSyncBpm is gated on eng.isPlaying(),
//                which is always false on the native path (the engine is
//                stopped), so toggling Sync mid-playback never re-seeks.
//   N4 (bug #4): the host audio-block latency (HostTransport.blockLatencySeconds)
//                is ignored, leaving the preview one block behind the playhead.
//
// Tests are written to FAIL on the unfixed code and PASS after the fix.
// ============================================================================
#include <cmath>
#include <string>

#include "../framework/AudioTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/audio/Engine.h>
#include <reals/bridge/Bridge.h>
#include <reals/platform/Path.h>

namespace reals::test {

namespace {

std::string writeNativeLoopWav(const std::string& name, double beats, double bpm, int sampleRate = 44100) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_nativephase");
    platform::ensureDir(tmpDir);
    const std::string path = platform::joinPath(tmpDir, name);
    const double duration = beats * 60.0 / bpm;
    auto pcm = AudioTestFixtures::generateSine(440.0f, static_cast<float>(duration), sampleRate);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(path, pcm, 1, sampleRate));
    return path;
}

} // namespace

// ---------------------------------------------------------------------------
// N1 (bug #2). 4-bar loop @ 120 BPM = 16 beats = 8.0 s nominal, but the file
//      carries a 0.8 s reverb tail (8.8 s total). DAW playhead at beat 8 →
//      startFraction = 0.5. The preview must seek to 0.5 × 8.0 s = 4.0 s (the
//      nominal loop), NOT 0.5 × 8.8 s = 4.4 s. The 0.4 s error is ~2 beats of
//      phase drift on every start.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N1_TailedLoop_SeeksAgainstNominalLoopNotFullFile) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_4bars_tail_120bpm.wav", 17.6, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0); // playing, beat 8 of 16

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 16.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5, 0.02);

    // The native preview must have been launched (mock returns true).
    const auto calls = harness.host().previewCalls();
    EXPECT_TRUE(!calls.empty());
    // Nominal loop = 16 beats × 60 / 120 BPM = 8.0 s → seek to 4.0 s.
    EXPECT_NEAR(calls.back().startPosSeconds, 4.0, 0.05);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N2 (bug #1). Contract guard: Bridge must pass the bar-quantized loopBeats and
//      a non-zero nominalLoopFrames to playHostPreview so the REAPER side can
//      wrap the loop on the bar grid instead of at the full file length.
//      (The plugin-side consumption is verified manually in REAPER; this test
//      locks the Bridge→host contract that makes it possible.)
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N2_TailedLoop_PassesLoopBeatsAndNominalFramesToHost) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_contract_4bars_tail.wav", 17.6, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    EXPECT_TRUE(res.value("ok", false));
    const auto calls = harness.host().previewCalls();
    EXPECT_TRUE(!calls.empty());
    const auto& rec = calls.back();
    EXPECT_TRUE(rec.loop);
    EXPECT_NEAR(rec.loopBeats, 16.0, 0.1);
    // 16 beats × 60 / 120 BPM × 44100 Hz = 352800 frames.
    EXPECT_EQ(rec.nominalLoopFrames, 352800ull);
    EXPECT_NEAR(rec.sampleBpm, 120.0, 0.1);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N3 (bug #3). Live re-phase: start a native preview with Sync OFF, then enable
//      Sync while the DAW transports. The preview must be re-seeked to the
//      current bar phase via setHostPreviewPositionFraction. The old gate
//      (eng.isPlaying()) is always false on the native path, so nothing fired.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N3_LiveRephase_OnNativePreview_SeeksToBarPhase) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_live_rephase_16beats.wav", 16.0, 120.0);

    // Launch native preview with sync OFF.
    auto playRes = harness.call("audio.play", {
        {"path", path}, {"syncBpm", false}, {"loop", true}});
    EXPECT_TRUE(playRes.value("ok", false));
    EXPECT_TRUE(harness.host().previewPlaying());

    // DAW transports to beat 4 of the 16-beat loop → expected phase 0.25.
    harness.host().setHostTransport(1, 2.0, 4.0, 120.0);

    // Enable sync live.
    auto syncRes = harness.call("audio.setSyncBpm", {
        {"enabled", true}, {"bpm", 120.0f}, {"sampleBpm", 120.0f}, {"path", path}});
    EXPECT_TRUE(syncRes.value("ok", false));

    const auto fracs = harness.host().previewFractionCalls();
    EXPECT_TRUE(!fracs.empty());
    EXPECT_NEAR(fracs.back(), 0.25, 0.02);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N4 (bug #4). Host audio-block latency compensation: the transport snapshot is
//      one audio block stale by the time the preview actually sounds. With a
//      20 ms block at 120 BPM (= 0.04 beats), the snap must advance the phase
//      so the preview lands on the playhead, not behind it.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N4_BlockLatency_AdvancesPhaseSnap) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_block_latency_16beats.wav", 16.0, 120.0);

    reals::bridge::HostTransport t;
    t.playState = 1;
    t.playPosition = 2.0;
    t.fullBeats = 4.0;
    t.bpm = 120.0;
    t.beatsPerMeasure = 4;
    t.denom = 4;
    t.blockLatencySeconds = 0.02; // 20 ms host block
    harness.host().setHostTransport(t);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    EXPECT_TRUE(res.value("ok", false));
    const auto calls = harness.host().previewCalls();
    EXPECT_TRUE(!calls.empty());

    // beatInLoop = 4.0 + (0.02 s × 120 / 60) = 4.04 → fraction 4.04/16 = 0.2525
    // nominal loop = 8.0 s → startPosSec = 0.2525 × 8.0 = 2.02 s.
    EXPECT_NEAR(calls.back().startPosSeconds, 2.02, 0.01);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N5. UI state contract: while the native preview owns playback the core Engine
//      is stopped, so a shell polling Engine::isPlaying() alone would never
//      push audio.state — the UI playhead would freeze at 0 while audio plays.
//      Bridge::isAudioActive() must be true, and audioStateJson must report a
//      non-zero duration (mirrored preview state, not the empty engine track).
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N5_NativePreview_IsAudioActiveAndStateDuration) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_state_contract_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});
    EXPECT_TRUE(res.value("ok", false));

    EXPECT_TRUE(harness.bridge().isAudioActive());
    EXPECT_FALSE(reals::audio::Engine::instance().isPlaying()); // engine idle on native path

    const auto state = json::parse(harness.audioStateJson());
    EXPECT_TRUE(state["data"].value("playing", false));
    EXPECT_NEAR(state["data"].value("duration", 0.0), 8.0, 0.1); // 16 beats @ 120 BPM
    EXPECT_NEAR(state["data"].value("position", -1.0), 0.5, 0.02); // snapped to beat 8/16
    EXPECT_NEAR(state["data"].value("timeRatio", 0.0), 1.0, 0.001);

    harness.call("audio.stop", json::object());
    EXPECT_FALSE(harness.bridge().isAudioActive());
}

// ---------------------------------------------------------------------------
// N6. Real-time Phase-Snap on DAW Seek (Playing):
//      While preview is actively playing in phase-snap mode, moving the DAW
//      play cursor (e.g. jumping from beat 8 to beat 12 in a 16-beat loop)
//      must immediately seek the preview audio to beat 12 (fraction 0.75)
//      and update the audio.state playhead.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N6_TransportSeek_WhilePlaying_RephasesPreviewAudioAndState) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_seek_playing_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0); // playing, beat 8 of 16

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});
    EXPECT_TRUE(res.value("ok", false));

    // First call initializes transport tracking
    EXPECT_FALSE(harness.bridge().updatePhaseSnapFromHostTransport());

    // User clicks Bar 4 (Beat 12) in REAPER timeline: transport jumps from beat 8 to 12
    harness.host().setHostTransport(1, 6.0, 12.0, 120.0);

    // Phase snap must detect the discontinuity and re-align
    EXPECT_TRUE(harness.bridge().updatePhaseSnapFromHostTransport());

    const auto fracs = harness.host().previewFractionCalls();
    EXPECT_TRUE(!fracs.empty());
    // Beat 12 in 16-beat loop = fraction 0.75
    EXPECT_NEAR(fracs.back(), 0.75, 0.02);

    const auto posCalls = harness.host().previewPositionCalls();
    EXPECT_TRUE(!posCalls.empty());
    // 0.75 * 8.0s nominal loop = 6.0s
    EXPECT_NEAR(posCalls.back(), 6.0, 0.1);

    const auto state = json::parse(harness.audioStateJson());
    EXPECT_TRUE(state["data"].value("playing", false));
    EXPECT_NEAR(state["data"].value("position", 0.0), 0.75, 0.02);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N7. Real-time Phase-Snap on DAW Loop Wrap:
//      When REAPER's transport loops back (e.g. from beat 15.9 to beat 0.0),
//      the preview must detect the negative beat delta and re-phase back to 0.0.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N7_TransportLoopWrap_RephasesPreview) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_loop_wrap_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(1, 7.9, 15.8, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});
    EXPECT_TRUE(res.value("ok", false));

    EXPECT_FALSE(harness.bridge().updatePhaseSnapFromHostTransport());

    // REAPER loops back to beat 0.0
    harness.host().setHostTransport(1, 0.0, 0.0, 120.0);

    EXPECT_TRUE(harness.bridge().updatePhaseSnapFromHostTransport());

    const auto fracs = harness.host().previewFractionCalls();
    EXPECT_TRUE(!fracs.empty());
    EXPECT_NEAR(fracs.back(), 0.0, 0.02);

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N8. Real-time Phase-Snap when Stopped:
//      When preview is idle/stopped with sync active, moving the edit cursor in
//      REAPER (e.g. to beat 4 of a 16-beat loop) must update the playhead
//      fraction to 0.25 in audio.state so the UI waveform playhead points to it.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N8_TransportSeek_WhileStopped_UpdatesPhaseFraction) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_stopped_seek_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(0, 0.0, 0.0, 120.0); // stopped at beat 0

    // Play and stop to register sample metrics in m_impl
    harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});
    harness.call("audio.stop", json::object());

    EXPECT_FALSE(harness.bridge().isAudioActive());

    // Initial check
    harness.bridge().updatePhaseSnapFromHostTransport();

    // User moves edit cursor in REAPER to beat 4.0 (2.0s)
    harness.host().setHostTransport(0, 2.0, 4.0, 120.0);

    EXPECT_TRUE(harness.bridge().updatePhaseSnapFromHostTransport());

    // Audio state when idle must report the snapped position
    const auto state = json::parse(harness.audioStateJson());
    EXPECT_FALSE(state["data"].value("playing", true));
    EXPECT_NEAR(state["data"].value("position", 0.0), 0.25, 0.02); // 4 / 16 = 0.25
    EXPECT_NEAR(state["data"].value("duration", 0.0), 8.0, 0.1);
}

// ---------------------------------------------------------------------------
// N9. Continuous Playback Stability:
//      During normal continuous playback without timeline seek or loop wrap,
//      updatePhaseSnapFromHostTransport must return false to avoid disturbing
//      the running audio engine.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N9_ContinuousPlayback_DoesNotTriggerSpuriousRephase) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_continuous_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0);

    harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    harness.bridge().updatePhaseSnapFromHostTransport();

    // Advance transport by ~30ms (0.06 beats at 120 BPM)
    harness.host().setHostTransport(1, 4.03, 8.06, 120.0);

    EXPECT_FALSE(harness.bridge().updatePhaseSnapFromHostTransport());

    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// N10. Spacebar ReaperPlayToggle:
//      When Spacebar triggers reaper.playToggle, it MUST stop any running
//      preview (never start preview) and toggle host DAW transport.
// ---------------------------------------------------------------------------
TEST(NativePhaseSnap, N10_Spacebar_ReaperPlayToggle_StopsPreviewAndTogglesDAW) {
    BridgeTestHarness harness(120.0);
    harness.host().setNativePreviewEnabled(true);
    const std::string path = writeNativeLoopWav("native_space_toggle_16beats.wav", 16.0, 120.0);
    harness.host().setHostTransport(0, 0.0, 0.0, 120.0);

    // 1. Start preview
    harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});
    EXPECT_TRUE(harness.host().isHostPreviewPlaying());

    // 2. Trigger Spacebar / reaper.playToggle
    harness.host().setHostPlayToggled(false);
    const auto res = harness.call("reaper.playToggle", json::object());
    EXPECT_TRUE(res.value("ok", false));

    // Preview MUST be stopped
    EXPECT_FALSE(harness.host().isHostPreviewPlaying());
    // Host transport MUST have been toggled
    EXPECT_TRUE(harness.host().hostPlayToggled());
}

} // namespace reals::test
