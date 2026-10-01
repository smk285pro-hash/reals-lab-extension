#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/TestRunner.h"
#include <reals/audio/Engine.h>
#include <reals/platform/Path.h>

namespace reals::test {
namespace {

struct RatePair {
    int source;
    int output;
};

constexpr std::array kRatePairs{
    RatePair{44100, 44100}, RatePair{44100, 48000}, RatePair{48000, 44100},
    RatePair{44100, 96000}, RatePair{96000, 44100}};

class RateConversionSession {
public:
    RateConversionSession()
        : m_engine(audio::Engine::instance()), m_rate(m_engine.targetSampleRate()),
          m_ratio(m_engine.getTimeRatio()), m_pitch(m_engine.getPitchSemitones()),
          m_volume(m_engine.volume()), m_loop(m_engine.loop()), m_ready(m_engine.isReady()) {
        m_engine.stop();
        EXPECT_TRUE(m_engine.init(false));
        m_engine.setTimeRatio(1.0f);
        m_engine.setPitchSemitones(0.0f);
        m_engine.setVolume(1.0f);
    }

    ~RateConversionSession() {
        m_engine.stop();
        m_engine.setTargetSampleRate(m_rate);
        m_engine.setTimeRatio(m_ratio);
        m_engine.setPitchSemitones(m_pitch);
        m_engine.setVolume(m_volume);
        m_engine.setLoop(m_loop);
        if (!m_ready) m_engine.shutdown();
        for (const auto& path : m_files) {
            std::error_code ec;
            std::filesystem::remove(platform::u8path(path), ec);
        }
    }

    std::string Write(const std::vector<float>& pcm, int channels, int sampleRate) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto path = platform::joinPath(platform::tempDir(),
            "reals_rate_conversion_" + std::to_string(stamp) + ".wav");
        m_files.push_back(path);
        EXPECT_TRUE(AudioTestFixtures::writeWavFile(path, pcm, channels, sampleRate, true));
        return path;
    }

private:
    audio::Engine& m_engine;
    int m_rate;
    float m_ratio;
    float m_pitch;
    float m_volume;
    bool m_loop;
    bool m_ready;
    std::vector<std::string> m_files;
};

double MeasureFrequency(const std::vector<float>& samples, int sampleRate) {
    // Interpolate positive zero crossings after the start fade/filter settling.
    const size_t start = static_cast<size_t>(sampleRate / 10);
    double first = 0.0;
    double last = 0.0;
    size_t crossings = 0;
    for (size_t i = std::max(size_t{1}, start); i < samples.size(); ++i) {
        EXPECT_TRUE(std::isfinite(samples[i]));
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f) {
            const double crossing = static_cast<double>(i - 1) -
                samples[i - 1] / static_cast<double>(samples[i] - samples[i - 1]);
            if (crossings == 0) first = crossing;
            last = crossing;
            ++crossings;
        }
    }
    EXPECT_TRUE(crossings > 100);
    return static_cast<double>(crossings - 1) * sampleRate / (last - first);
}

void CheckStereoPitch(int sourceRate, int outputRate) {
    RateConversionSession session;
    auto& engine = audio::Engine::instance();
    engine.setTargetSampleRate(outputRate);
    const auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 1.25f, sourceRate);
    const auto path = session.Write(pcm, 2, sourceRate);
    EXPECT_TRUE(engine.playFile(path));
    const double expectedFrames = std::round(static_cast<double>(pcm.size() / 2) * outputRate / sourceRate);
    EXPECT_EQ(engine.currentTrack().totalFrames, expectedFrames);
    EXPECT_EQ(engine.currentTrack().sampleRate, outputRate);

    std::vector<float> left(static_cast<size_t>(outputRate));
    std::vector<float> right(left.size());
    engine.renderFrames(left.data(), right.data(), left.size());
    EXPECT_NEAR(MeasureFrequency(left, outputRate), 440.0, 0.1);
    EXPECT_NEAR(MeasureFrequency(right, outputRate), 880.0, 0.1);
}

} // namespace

TEST(EngineRateConversion, StereoPitchAndDurationSurviveRateChanges) {
    for (const auto rates : kRatePairs) CheckStereoPitch(rates.source, rates.output);
}

TEST(EngineRateConversion, StartPhaseUsesOutputFramesWithNominalLoopAndTail) {
    RateConversionSession session;
    auto& engine = audio::Engine::instance();
    for (const auto rates : kRatePairs) {
        engine.setTargetSampleRate(rates.output);
        const auto pcm = AudioTestFixtures::generateSine(440.0f, 2.25f, rates.source);
        const auto path = session.Write(pcm, 1, rates.source);
        const auto nominalFrames = static_cast<uint64_t>(rates.source) * 2;
        const auto outputLoopFrames = static_cast<uint64_t>(rates.output) * 2;
        EXPECT_TRUE(engine.playFile(path, true, 0.5, nullptr, nominalFrames));
        EXPECT_EQ(engine.loopBoundaryFrames(), outputLoopFrames);
        EXPECT_NEAR(engine.positionFraction() * static_cast<double>(outputLoopFrames),
                    static_cast<double>(rates.output), 1.0);

        // A fresh phase anchor must use the same converted reference length.
        EXPECT_TRUE(engine.playFile(path, true, 0.0, [](double) { return 0.75; }, nominalFrames));
        EXPECT_NEAR(engine.positionFraction() * static_cast<double>(outputLoopFrames),
                    1.5 * rates.output, 1.0);
        engine.stop();
    }
}

TEST(EngineRateConversion, MonoSourceRendersIdenticalStereoChannels) {
    RateConversionSession session;
    auto& engine = audio::Engine::instance();
    engine.setTargetSampleRate(48000);
    const auto path = session.Write(AudioTestFixtures::generateSine(440.0f, 1.25f, 44100), 1, 44100);
    EXPECT_TRUE(engine.playFile(path));
    std::vector<float> left(48000);
    std::vector<float> right(left.size());
    engine.renderFrames(left.data(), right.data(), left.size());
    EXPECT_NEAR(MeasureFrequency(left, 48000), 440.0, 0.1);
    for (size_t i = 0; i < left.size(); ++i) EXPECT_NEAR(left[i], right[i], 0.000001f);
}

TEST(EngineRateConversion, TransientTimingStaysOnGridAcrossThreeLoopWraps) {
    RateConversionSession session;
    auto& engine = audio::Engine::instance();
    for (const auto rates : kRatePairs) {
        engine.setTargetSampleRate(rates.output);
        const auto nominalFrames = static_cast<size_t>(rates.source) * 2;
        std::vector<float> pcm(nominalFrames + static_cast<size_t>(rates.source / 4), 0.0f);
        // A narrow symmetric pulse every half-second gives an unambiguous peak.
        const int pulseRadius = rates.source / 1000;
        for (int beat = 0; beat < 4; ++beat) {
            const auto center = static_cast<size_t>((0.125 + 0.5 * beat) * rates.source);
            for (int offset = -pulseRadius; offset <= pulseRadius; ++offset) {
                const auto index = static_cast<size_t>(static_cast<int64_t>(center) + offset);
                pcm[index] = 0.8f * (1.0f - static_cast<float>(std::abs(offset)) / pulseRadius);
            }
        }
        // A strong tail marker must never leak into the nominal loop.
        pcm[nominalFrames + static_cast<size_t>(rates.source / 8)] = 1.0f;
        const auto path = session.Write(pcm, 1, rates.source);
        EXPECT_TRUE(engine.playFile(path, true, 0.5, nullptr, nominalFrames));
        const size_t renderFrames = static_cast<size_t>(rates.output) * 6;
        std::vector<float> left(renderFrames);
        std::vector<float> right(renderFrames);
        engine.renderFrames(left.data(), right.data(), renderFrames);

        for (int beat = 0; beat < 12; ++beat) {
            const auto expected = static_cast<size_t>((0.125 + 0.5 * beat) * rates.output);
            const auto radius = static_cast<size_t>(rates.output / 100);
            const auto begin = left.begin() + static_cast<std::ptrdiff_t>(expected - radius);
            const auto end = left.begin() + static_cast<std::ptrdiff_t>(expected + radius);
            const auto peak = std::max_element(begin, end);
            EXPECT_TRUE(*peak > 0.5f);
            const auto actual = static_cast<double>(std::distance(left.begin(), peak));
            // Bound interpolation/filter timing below 1 ms, far below the old
            // sample-rate cursor error (40+ ms even on short loops).
            EXPECT_NEAR(actual, static_cast<double>(expected), rates.output * 0.001);
        }
        EXPECT_NEAR(engine.positionFraction(), 0.5, 1.0 / (2.0 * rates.output));
        engine.stop();
    }
}

} // namespace reals::test
