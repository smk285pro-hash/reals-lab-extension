// ============================================================================
// PhaseSync Diagnostics — tái hiện lỗi "preview lệch so với bar của DAW"
//
// Bộ test này mô phỏng các tình huống THỰC TẾ mà các suite cũ không cover:
//   D1. Sample loop dài 12 bar (blues), 6 bar, 3 bar — bị Bar Quantizer ép
//       về power-of-2 bars (16/8/4) → startFraction lệch tới nhiều bar.
//   D2. Transport DAW được chụp quá sớm (trước khi engine decode file xong)
//       → preview luôn trễ so với playhead một khoảng bằng thời gian decode.
//   D3. Chế độ DSP (time-stretch) có pipeline latency (SoundTouch initial
//       latency) không được bù → onset audible trễ thêm ~20-30ms.
//
// Các test được viết để FAIL trên code lỗi và PASS sau khi fix.
// ============================================================================
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/audio/Engine.h>
#include <reals/bridge/Bridge.h>
#include <reals/platform/Path.h>

namespace reals::test {

namespace {

std::string writeLoopWav(const std::string& name, double beats, double bpm, int sampleRate = 44100) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_phasediag");
    platform::ensureDir(tmpDir);
    const std::string path = platform::joinPath(tmpDir, name);
    const double duration = beats * 60.0 / bpm;
    auto pcm = AudioTestFixtures::generateSine(440.0f, static_cast<float>(duration), sampleRate);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(path, pcm, 1, sampleRate));
    return path;
}

// Host phát hiện Engine đã decode xong file đích chưa. Nếu transport được
// query TRƯỚC khi decode (bug cũ) → trả về vị trí cũ m_beatsBefore; nếu query
// SAU khi decode (đúng — mọi I/O đã xong, sắp phát âm thanh) → m_beatsAfter.
class DecodeAwareHost : public reals::test::MockHostActions {
public:
    DecodeAwareHost(const std::string& targetPath, double beatsBefore, double beatsAfter)
        : MockHostActions(120.0),
          m_targetPath(targetPath),
          m_beatsBefore(beatsBefore),
          m_beatsAfter(beatsAfter) {}

    [[nodiscard]] reals::bridge::HostTransport hostTransport() const override {
        reals::bridge::HostTransport t;
        const bool decoded = reals::audio::Engine::instance().currentTrack().path == m_targetPath;
        t.playState = 1;
        t.bpm = 120.0;
        t.beatsPerMeasure = 4;
        t.denom = 4;
        t.fullBeats = decoded ? m_beatsAfter : m_beatsBefore;
        t.playPosition = t.fullBeats * 0.5;
        return t;
    }

private:
    std::string m_targetPath;
    double m_beatsBefore;
    double m_beatsAfter;
};

} // namespace

// ---------------------------------------------------------------------------
// D1a. 12-bar blues loop: chu kỳ thật = 48 beats. Quantizer cũ (tolerance 35%)
//      ép về 16 bars (64 beats) → preview lệch hàng bar.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D1a_TwelveBarBluesLoop_NotForcedToPowerOfTwo) {
    BridgeTestHarness harness(100.0);
    const std::string path = writeLoopWav("blues_12bars_100bpm.wav", 48.0, 100.0);
    // DAW ở giữa đoạn 12 bar: beat 24 / 48 = 0.5
    harness.host().setHostTransport(1, 14.4, 24.0, 100.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 100.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    // loopBeats PHẢI là 48 (12 bars × 4) — không được ép thành 64 (16 bars)
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 48.0, 0.1);
    // startFraction PHẢI ≈ 24/48 = 0.5 (code cũ cho 24/64 = 0.375 → lệch 1.5 bar!)
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5, 0.02);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D1b. 6-bar loop: thật = 24 beats; cũ ép về 8 bars (32 beats).
//      DAW ở beat 20 → đúng là 20/24 = 0.833; cũ cho 20/32 = 0.625
//      → preview bắt đầu lệch 1.25 bar.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D1b_SixBarLoop_NotForcedToEightBars) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_6bars_120bpm.wav", 24.0, 120.0);
    harness.host().setHostTransport(1, 10.0, 20.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 24.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 20.0 / 24.0, 0.02);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D1c. 3-bar loop: thật = 12 beats; cũ ép về 4 bars (16 beats).
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D1c_ThreeBarLoop_NotForcedToFourBars) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_3bars_120bpm.wav", 12.0, 120.0);
    harness.host().setHostTransport(1, 5.0, 10.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 12.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 10.0 / 12.0, 0.02);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D1d. Loop 2.5 bar (10 beats) — không gần bar chuẩn nào → phải fallback về
//      lưới integer-beat (10 beats) để transient vẫn khớp beat của DAW.
//      Code cũ ép về 2 bars (8 beats).
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D1d_NonIntegerBars_FallbackToIntegerBeatGrid) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_2p5bars_120bpm.wav", 10.0, 120.0);
    harness.host().setHostTransport(1, 2.0, 4.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 10.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.4, 0.02);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D1e. Regression guard: loop chuẩn power-of-2 (kèm encoder padding) vẫn snap
//      đúng — 7.99 bar @ 120 BPM → 32 beats.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D1e_PowerOfTwoWithPadding_StillSnaps) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_7p99bars_120bpm.wav", 31.96, 120.0);
    harness.host().setHostTransport(1, 8.0, 16.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 32.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5, 0.02);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D2. Transport phải được chụp lại SAU khi engine decode xong file (ngay
//     trước khi âm thanh bắt đầu) — không phải trước lúc decode. Host dưới
//     đây trả về 2 vị trí khác nhau tuỳ thời điểm query: vị trí đúng (13.0)
//     chỉ có sẵn sau khi decode. Code cũ chụp trước → luôn dùng vị trí cũ
//     (5.0) → preview lệch đúng bằng khoảng DAW chạy trong lúc decode.
//     Loop 8 bar = 32 beats: cũ ra 5/32 = 0.15625, đúng là 13/32 = 0.40625.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D2_TransportSnapshotTakenAfterDecode_PreviewNotStale) {
    const std::string path = writeLoopWav("loop_stale_transport.wav", 32.0, 120.0);
    auto host = std::make_unique<DecodeAwareHost>(path, 5.0, 13.0);
    DecodeAwareHost* hostPtr = host.get();

    const std::string tempStore = reals::platform::joinPath(reals::platform::tempDir(), "reals_d2_store.json");
    std::error_code ec;
    std::filesystem::remove(reals::platform::u8path(tempStore), ec);

    reals::bridge::Bridge bridge(hostPtr, tempStore);
    bridge.init();

    json req;
    req["id"] = 1;
    req["cmd"] = "audio.play";
    req["args"] = {{"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0}, {"loop", false}};
    auto res = json::parse(bridge.handle(req.dump()));

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    // Vị trí áp dụng phải tương ứng DAW beat 13 (sau decode), không phải 5.
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 13.0 / 32.0, 0.02);
    bridge.handle(R"({"id":2,"cmd":"audio.stop","args":{}})");
    std::filesystem::remove(reals::platform::u8path(tempStore), ec);
}

// ---------------------------------------------------------------------------
// D3. Chế độ time-stretch (projectBpm ≠ sampleBpm): startFraction phải khớp
//     chính xác với phách thực tế của DAW (4.0 / 16.0 = 0.25).
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D3_DspMode_StartFractionExactBeatAlignment) {
    BridgeTestHarness harness(160.0); // project 160 BPM
    const std::string path = writeLoopWav("loop_120bpm_4bars_st.wav", 16.0, 120.0);
    // Sample 120 BPM → ratio = 160/120 = 1.333 (DSP time-stretch active)
    // DAW đang ở beat 4.0 của chu kỳ 16 beat.
    harness.host().setHostTransport(1, 1.5, 4.0, 160.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    const double appliedBeats = res["data"].value("startFraction", 0.0) * 16.0;
    // Khớp chính xác beat 4.0 của vòng lặp 16 beats (0.25 fraction)
    EXPECT_NEAR(appliedBeats, 4.0, 0.01);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D4. Loop có reverb tail (4 bar = 16 beats = 8.0s @ 120 BPM, nhưng file dài 8.8s
//     do có 0.8s tail) -> Phải snap đúng 16 beats và loopBoundaryFrames = 352800.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D4_LoopWithReverbTail_SnapsToExactBarAndNominalLoopFrames) {
    BridgeTestHarness harness(120.0);
    // 4 bars @ 120 BPM = 16 beats = 8.0s. Thêm 0.8s reverb tail = 8.8s (17.6 beats raw)
    const std::string path = writeLoopWav("loop_4bars_with_tail_120bpm.wav", 17.6, 120.0);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0); // DAW ở beat 8 (Bar 3 Beat 1)

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    EXPECT_NEAR(res["data"].value("loopBeats", 0.0), 16.0, 0.1);
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5, 0.02);

    // Engine loopBoundaryFrames must equal 16 beats * 60 / 120 * 44100 = 352800 frames
    EXPECT_EQ(reals::audio::Engine::instance().loopBoundaryFrames(), 352800ull);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D5. Live re-phase on audio.setSyncBpm: Bật Sync khi sample đang phát
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D5_LiveRephaseOnSyncBpmChange) {
    BridgeTestHarness harness(140.0);
    const std::string path = writeLoopWav("loop_live_sync_120bpm.wav", 16.0, 120.0);
    harness.host().setHostTransport(1, 2.571, 6.0, 140.0);

    // Play without sync first
    auto playRes = harness.call("audio.play", {
        {"path", path}, {"syncBpm", false}, {"loop", true}});
    EXPECT_TRUE(playRes.value("ok", false));

    // Live toggle sync on
    auto syncRes = harness.call("audio.setSyncBpm", {
        {"enabled", true}, {"bpm", 140.0f}, {"sampleBpm", 120.0f}, {"path", path}});
    EXPECT_TRUE(syncRes.value("ok", false));

    EXPECT_NEAR(reals::audio::Engine::instance().getTimeRatio(), 140.0f / 120.0f, 0.01f);
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D6. Multi-Rate Alignment: 44.1kHz audio on 48kHz host device.
//     Target sample rate must be 48000, totalFrames and loopBoundaryFrames
//     must reflect 48000Hz (16 beats @ 120 BPM = 8.0s -> 384000 frames).
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D6_MultiRate_44kAudio_On_48kHost_FrameMetricsAndLoopAligned) {
    reals::audio::Engine::instance().setTargetSampleRate(48000);
    EXPECT_EQ(reals::audio::Engine::instance().targetSampleRate(), 48000);

    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_44k_on_48k_120bpm.wav", 16.0, 120.0, 44100);
    harness.host().setHostTransport(1, 4.0, 8.0, 120.0); // DAW at beat 8 (0.5 fraction)

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", true}, {"sampleBpm", 120.0f}, {"loop", true}});

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 0.5, 0.02);

    const auto& trk = reals::audio::Engine::instance().currentTrack();
    EXPECT_EQ(trk.sampleRate, 48000);
    EXPECT_NEAR(trk.totalFrames, 384000.0, 100.0);
    EXPECT_EQ(reals::audio::Engine::instance().loopBoundaryFrames(), 384000ull);
    EXPECT_NEAR(reals::audio::Engine::instance().positionFraction(), 0.5, 0.05);

    harness.call("audio.stop", json::object());
    reals::audio::Engine::instance().setTargetSampleRate(0);
}

// ---------------------------------------------------------------------------
// D7. Multi-Rate Alignment: 44.1kHz audio on 96kHz host device.
//     Target sample rate must be 96000, 8 beats @ 120 BPM = 4.0s -> 384000 frames.
//     Audio plays at 1.0x pitch-neutral playback rate when BPM sync is off.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D7_MultiRate_44kAudio_On_96kHost_PitchNeutral) {
    reals::audio::Engine::instance().setTargetSampleRate(96000);
    EXPECT_EQ(reals::audio::Engine::instance().targetSampleRate(), 96000);

    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_44k_on_96k_120bpm.wav", 8.0, 120.0, 44100);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", false}, {"loop", false}});

    EXPECT_TRUE(res.value("ok", false));
    const auto& trk = reals::audio::Engine::instance().currentTrack();
    EXPECT_EQ(trk.sampleRate, 96000);
    EXPECT_NEAR(trk.totalFrames, 384000.0, 100.0);
    EXPECT_NEAR(reals::audio::Engine::instance().getTimeRatio(), 1.0f, 0.001f);
    EXPECT_NEAR(reals::audio::Engine::instance().getPitchSemitones(), 0.0f, 0.001f);

    harness.call("audio.stop", json::object());
    reals::audio::Engine::instance().setTargetSampleRate(0);
}

// ---------------------------------------------------------------------------
// D8. Audio Thread Safety: Zero-allocation and lock-free renderFrames execution
//     for stereo and mono hardware master outputs.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D8_AudioThreadSafety_ZeroAllocAndLockFreeRendering) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_render_safety_120bpm.wav", 16.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", false}, {"loop", true}});
    EXPECT_TRUE(res.value("ok", false));

    std::vector<float> bufL(8192, 0.0f);
    std::vector<float> bufR(8192, 0.0f);

    // Test Stereo render
    reals::audio::Engine::instance().renderFrames(bufL.data(), bufR.data(), 512);
    reals::audio::Engine::instance().renderFrames(bufL.data(), bufR.data(), 1024);
    reals::audio::Engine::instance().renderFrames(bufL.data(), bufR.data(), 4096);
    reals::audio::Engine::instance().renderFrames(bufL.data(), bufR.data(), 8192);

    // Test Mono render (outR == nullptr)
    reals::audio::Engine::instance().renderFrames(bufL.data(), nullptr, 512);
    reals::audio::Engine::instance().renderFrames(bufL.data(), nullptr, 1024);

    // Test Mono render (outL == nullptr)
    reals::audio::Engine::instance().renderFrames(nullptr, bufR.data(), 512);

    EXPECT_TRUE(reals::audio::Engine::instance().isPlaying());
    harness.call("audio.stop", json::object());
}

// ---------------------------------------------------------------------------
// D9. Discontinuity and Seek Bounds: Position fractions and seek clamping.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D9_SeekDiscontinuity_LockFreePlayback) {
    BridgeTestHarness harness(120.0);
    const std::string path = writeLoopWav("loop_seek_bounds_120bpm.wav", 16.0, 120.0);

    auto res = harness.call("audio.play", {
        {"path", path}, {"syncBpm", false}, {"loop", true}});
    EXPECT_TRUE(res.value("ok", false));

    // Seek to 75%
    harness.call("audio.seek", {{"fraction", 0.75}});
    EXPECT_NEAR(reals::audio::Engine::instance().positionFraction(), 0.75, 0.05);

    // Seek to 25%
    harness.call("audio.seek", {{"fraction", 0.25}});
    EXPECT_NEAR(reals::audio::Engine::instance().positionFraction(), 0.25, 0.05);

    harness.call("audio.stop", json::object());
}

} // namespace reals::test

