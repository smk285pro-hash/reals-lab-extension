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

    reals::bridge::Bridge bridge(hostPtr);
    bridge.init();

    const std::string req = R"({"id":1,"cmd":"audio.play","args":{"path":")" + path +
                            R"(","syncBpm":true,"sampleBpm":120.0,"loop":false}})";
    auto res = json::parse(bridge.handle(req));

    EXPECT_TRUE(res.value("ok", false));
    EXPECT_TRUE(res["data"].value("phaseSynced", false));
    // Vị trí áp dụng phải tương ứng DAW beat 13 (sau decode), không phải 5.
    EXPECT_NEAR(res["data"].value("startFraction", 0.0), 13.0 / 32.0, 0.02);
    bridge.handle(R"({"id":2,"cmd":"audio.stop","args":{}})");
}

// ---------------------------------------------------------------------------
// D3. Chế độ time-stretch (projectBpm ≠ sampleBpm): startFraction phải được
//     TiẾN lên để bù pipeline latency (SoundTouch initial latency ~24ms).
//     Code cũ không bù → mọi onset nghe trễ ~24ms so với grid của DAW.
// ---------------------------------------------------------------------------
TEST(PhaseSyncDiagnostics, D3_DspMode_StartFractionAdvancedByPipelineLatency) {
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
    // Phase thô = 4.0 beats. Sau fix, vị trí feed phải > 4.0 (tiến lên bù
    // latency ≈ 24ms × 160/60 ≈ 0.064 beats + device buffer). Code cũ = 4.0.
    EXPECT_GT(appliedBeats, 4.02);          // phải có bù latency đáng kể
    EXPECT_LE(appliedBeats, 4.02 + 0.25);   // nhưng hợp lý (< ~94ms tổng)
    harness.call("audio.stop", json::object());
}

} // namespace reals::test
