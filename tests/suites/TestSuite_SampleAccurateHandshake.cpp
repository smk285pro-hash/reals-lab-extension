#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../framework/TestRunner.h"
#include <reals/audio/SampleAccurateHandshake.h>

namespace reals::test {
namespace {

std::vector<double> MakePrbs(int64_t frames) {
    std::vector<double> signal(static_cast<size_t>(frames));
    uint32_t state = 0x5a17c9e3U;
    for (int64_t i = 0; i < frames; ++i) {
        const uint32_t bit = ((state >> 0U) ^ (state >> 1U) ^ (state >> 21U) ^ (state >> 31U)) & 1U;
        state = (state >> 1U) | (bit << 31U);
        signal[static_cast<size_t>(i)] = (state & 1U) != 0U ? 0.75 : -0.75;
    }
    return signal;
}

struct NullMetrics {
    int64_t lag = 0;
    double maxResidual = 0.0;
};

NullMetrics RunNullCase(int blockFrames, int delayedBlocks, int sampleRate,
                        double rate, bool impulse, bool previewBeforeHook) {
    constexpr double kLoopSeconds = 2.0;
    const int64_t loopFrames = static_cast<int64_t>(std::llround(kLoopSeconds * sampleRate));
    const double loopBeats = 4.0 * rate;
    auto source = MakePrbs(loopFrames);
    if (impulse) {
        std::fill(source.begin(), source.end(), 0.0);
        source[0] = 1.0;
        source[static_cast<size_t>(loopFrames / 3)] = -0.5;
    }

    audio::SampleAccurateHandshake handshake;
    handshake.configure(loopFrames, loopBeats);
    for (int i = 0; i < delayedBlocks; ++i) {
        handshake.beginCallback(false, 0.0);
    }
    handshake.arm();

    const int64_t dawStartFrame = (static_cast<int64_t>(delayedBlocks) * blockFrames + blockFrames / 3) % loopFrames;
    const double fullBeats = static_cast<double>(dawStartFrame) * loopBeats / static_cast<double>(loopFrames);
    uint64_t consumedGeneration = 0;
    audio::SampleAccurateCapture capture;
    int64_t hostCaptureFrame = 0;
    bool active = false;

    if (!previewBeforeHook) {
        handshake.beginCallback(true, fullBeats);
        active = handshake.consume(consumedGeneration, capture);
    } else {
        active = handshake.consume(consumedGeneration, capture);
        handshake.beginCallback(true, fullBeats);
        active = handshake.consume(consumedGeneration, capture);
    }
    EXPECT_TRUE(active);
    hostCaptureFrame = 0;

    NullMetrics metrics;
    for (int64_t frame = 0; frame < static_cast<int64_t>(blockFrames) * 4; ++frame) {
        const int64_t dawFrame = audio::WrapFrame(dawStartFrame + frame, loopFrames);
        const int64_t previewFrame = audio::AlignedPreviewFrame(
            frame, hostCaptureFrame, capture.dawPhaseFrame, capture.loopFrames);
        metrics.lag = std::max(metrics.lag, std::llabs(previewFrame - dawFrame));
        const double residual = source[static_cast<size_t>(dawFrame)] -
                                source[static_cast<size_t>(previewFrame)];
        metrics.maxResidual = std::max(metrics.maxResidual, std::abs(residual));
    }
    return metrics;
}

} // namespace

TEST(SampleAccurateHandshake, PRBSAndImpulseNullAcrossBlocksRatesAndDelayedStarts) {
    constexpr std::array<int, 4> kBlocks{64, 128, 256, 512};
    constexpr std::array<int, 3> kRates{44100, 48000, 96000};
    constexpr std::array<double, 4> kPlayRates{0.5, 0.75, 1.0, 1.5};
    int64_t measuredMaxOffset = 0;
    double measuredMaxResidual = 0.0;

    for (const int block : kBlocks) {
        for (const int sampleRate : kRates) {
            for (const double rate : kPlayRates) {
                for (int delay = 0; delay <= 16; ++delay) {
                    for (const bool impulse : {false, true}) {
                        for (const bool previewBeforeHook : {false, true}) {
                            const auto metrics = RunNullCase(block, delay, sampleRate, rate,
                                                             impulse, previewBeforeHook);
                            measuredMaxOffset = std::max(measuredMaxOffset, metrics.lag);
                            measuredMaxResidual = std::max(measuredMaxResidual, metrics.maxResidual);
                        }
                    }
                }
            }
        }
    }

    std::cout << "    measured_max_offset_samples=" << measuredMaxOffset
              << " measured_max_residual=" << measuredMaxResidual << '\n';
    EXPECT_EQ(measuredMaxOffset, 0);
    EXPECT_NEAR(measuredMaxResidual, 0.0, 1.0e-12);
}

TEST(SampleAccurateHandshake, LoopWrapKeepsZeroLagAndZeroResidual) {
    audio::SampleAccurateHandshake handshake;
    constexpr int64_t kLoopFrames = 1024;
    handshake.configure(kLoopFrames, 4.0);
    handshake.arm();
    handshake.beginCallback(true, 3.99609375);

    uint64_t generation = 0;
    audio::SampleAccurateCapture capture;
    EXPECT_TRUE(handshake.consume(generation, capture));
    EXPECT_EQ(capture.dawPhaseFrame, 1023);
    EXPECT_EQ(audio::AlignedPreviewFrame(1, 0, capture.dawPhaseFrame, capture.loopFrames), 0);
}

TEST(SampleAccurateHandshake, GenerationIsConsumedOnceAtCapturedCallbackEpoch) {
    audio::SampleAccurateHandshake handshake;
    handshake.configure(48000, 4.0);
    const uint64_t generation = handshake.arm();
    const uint64_t epoch = handshake.beginCallback(true, 1.0);

    uint64_t consumed = 0;
    audio::SampleAccurateCapture capture;
    EXPECT_TRUE(handshake.consume(consumed, capture));
    EXPECT_EQ(capture.generation, generation);
    EXPECT_EQ(capture.callbackEpoch, epoch);
    EXPECT_TRUE(!handshake.consume(consumed, capture));
}

} // namespace reals::test
