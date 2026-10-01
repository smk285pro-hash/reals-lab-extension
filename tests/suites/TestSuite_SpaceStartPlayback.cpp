#include <cmath>
#include <string>

#include "../framework/AudioTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/bridge/Bridge.h>
#include <reals/platform/Path.h>

namespace reals::test {
namespace {

// Model the native host's finite-source lifetime. The ordinary mock never
// reaches EOF, so it cannot expose a seek to the last block before DAW play.
class FinitePreviewHost : public MockHostActions {
public:
    explicit FinitePreviewHost(double outputDuration) : m_duration(outputDuration) {
        setNativePreviewEnabled(true);
        setPreviewSimulatedDuration(outputDuration);
    }

    void startTransport() override {
        MockHostActions::startTransport();
        auto transport = hostTransport();
        transport.playState = 1;
        setHostTransport(transport);
    }

    void startTransport(double startPositionSeconds) override {
        MockHostActions::startTransport(startPositionSeconds);
        auto transport = hostTransport();
        transport.playState = 1;
        transport.playPosition = startPositionSeconds;
        transport.fullBeats = startPositionSeconds * transport.bpm / 60.0;
        setHostTransport(transport);
    }

    void Advance(double seconds) {
        if (isHostPreviewPlaying()) {
            double position = previewPositionSeconds() + seconds;
            if (lastPreviewCall().loop) {
                position = std::fmod(position, m_duration);
                setHostPreviewPosition(position);
            } else if (position >= m_duration) {
                stopHostPreview();
            } else {
                setHostPreviewPosition(position);
            }
        }
        auto transport = hostTransport();
        if (transport.isPlaying()) {
            transport.playPosition += seconds;
            transport.fullBeats += seconds * transport.bpm / 60.0;
            setHostTransport(transport);
        }
    }

private:
    double m_duration;
};

void CheckSpaceStart(const std::string& name, double sampleBpm, bool loop, double startBeat,
                     double blockSeconds, double tailSeconds = 0.0) {
    const std::string dir = platform::joinPath(platform::tempDir(), "RealsLab", "tests_space_start");
    platform::ensureDir(dir);
    const std::string path = platform::joinPath(dir, name + ".wav");
    const double rawDuration = 16.0 * 60.0 / sampleBpm + tailSeconds;
    const auto pcm = AudioTestFixtures::generateSine(440.0f, static_cast<float>(rawDuration), 48000);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(path, pcm, 1, 48000));
    const double outputDuration = loop ? 8.0 : rawDuration / (120.0 / sampleBpm);
    FinitePreviewHost host(outputDuration);
    bridge::Bridge bridge(&host, platform::joinPath(dir, name + ".json"));
    bridge.init();
    const auto call = [&bridge](const std::string& cmd, const json& args) {
        return json::parse(bridge.handle(json{{"cmd", cmd}, {"args", args}}.dump()));
    };

    // Repeat the user's preview -> Space -> stop cycle with the same host.
    for (int cycle = 0; cycle < 2; ++cycle) {
        bridge::HostTransport transport;
        transport.bpm = 120.0;
        transport.fullBeats = startBeat;
        transport.playPosition = startBeat * 0.5;
        transport.blockLatencySeconds = blockSeconds;
        host.setHostTransport(transport);
        EXPECT_TRUE(call("audio.play", {
            {"path", path}, {"syncBpm", true}, {"sampleBpm", sampleBpm}, {"loop", loop}}).value("ok", false));
        host.Advance(0.75);
        const double previewBeforeSpace = host.previewPositionSeconds();
        const auto seeksBeforeSpace = host.previewPositionCalls().size();
        EXPECT_TRUE(call("reaper.dawPlay", json::object()).value("ok", false));

        // Space aligns the DAW to the preview's live phase. It must not rewind
        // or reset the already-audible native preview, even at project beat 0.
        EXPECT_NEAR(host.previewPositionAtTransportStart(), previewBeforeSpace, 0.0001);
        EXPECT_EQ(host.previewPositionCalls().size(), seeksBeforeSpace);

        // The first transport snapshot must not apply a second correction.
        const auto seeksBeforeTimer = host.previewPositionCalls().size();
        bridge.updatePhaseSnapFromHostTransport();
        EXPECT_EQ(host.previewPositionCalls().size(), seeksBeforeTimer);

        // Enough frames to reproduce the real EOF, not just a mocked seek call.
        host.Advance(0.25);
        EXPECT_TRUE(host.hostTransport().isPlaying());
        EXPECT_TRUE(host.isHostPreviewPlaying());
        EXPECT_EQ(host.lastPreviewCall().loop, loop);
        // EOF is still terminal when looping is disabled; no forced loop workaround.
        host.Advance(outputDuration);
        EXPECT_EQ(host.isHostPreviewPlaying(), loop);
        call("reaper.dawStop", json::object());
        EXPECT_FALSE(host.isHostPreviewPlaying());
    }
}

} // namespace

TEST(SpaceStartPlayback, NonLoopingStartSurvivesHardwareBlock) {
    CheckSpaceStart("space_finite_120bpm", 120.0, false, 0.0, 128.0 / 48000.0);
}

TEST(SpaceStartPlayback, NonLoopingLaterBarSurvivesHardwareBlock) {
    CheckSpaceStart("space_later_bar_120bpm", 120.0, false, 128.0, 128.0 / 48000.0);
}

TEST(SpaceStartPlayback, LoopingStartUsesDownbeatWithNonzeroLatency) {
    CheckSpaceStart("space_loop_120bpm", 120.0, true, 128.0, 0.02);
}

TEST(SpaceStartPlayback, StretchedNonLoopingStartSurvivesHardwareBlock) {
    CheckSpaceStart("space_stretch_90bpm", 90.0, false, 128.0, 128.0 / 48000.0);
}

TEST(SpaceStartPlayback, TailedNonLoopingMidPhraseKeepsNominalPhase) {
    CheckSpaceStart("space_tail_90bpm", 90.0, false, 136.0, 0.02, 1.0);
}

} // namespace reals::test
