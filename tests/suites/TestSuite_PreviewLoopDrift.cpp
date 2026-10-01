#include <array>
#include <cmath>
#include <cstdint>

#include "../framework/TestRunner.h"
#include <reals/audio/PreviewLoopCursor.h>

namespace reals::test {
namespace {

struct LoopRun {
    int64_t renderedFrames = 0;
    double cursorSeconds = 0.0;
};

template <size_t N>
LoopRun RenderOneLoop(const double boundarySeconds,
                      const double sampleRate,
                      const std::array<int, N>& callbackFrames,
                      const size_t firstCallback = 0) {
    LoopRun run;
    size_t callback = firstCallback;
    constexpr size_t kGuardCallbacks = 1000000;
    for (size_t guard = 0; guard < kGuardCallbacks; ++guard, ++callback) {
        const int requested = callbackFrames[callback % callbackFrames.size()];
        const int rendered = audio::PreviewLoopCursor::FramesUntilBoundary(
            run.cursorSeconds, boundarySeconds, sampleRate, requested);
        if (rendered == 0) return run;

        EXPECT_TRUE(rendered > 0);
        EXPECT_TRUE(rendered <= requested);
        run.renderedFrames += rendered;
        run.cursorSeconds = audio::PreviewLoopCursor::AdvanceWrapped(
            run.cursorSeconds, rendered, sampleRate, boundarySeconds);
        // An integer-frame loop wraps exactly to zero.  Stop immediately so
        // the next iteration does not start rendering the following cycle.
        if (run.renderedFrames > 0 && run.cursorSeconds == 0.0) return run;
    }
    EXPECT_TRUE(false); // A valid finite boundary must always terminate the cycle.
    return run;
}

} // namespace

TEST(PreviewLoopDrift, IntegerFrameBoundaryDoesNotLoseAFrameAcrossManyWraps) {
    constexpr double kRate = 48000.0;
    constexpr double kBoundary = 4.0;
    constexpr int64_t kLoopFrames = 192000;
    constexpr std::array kCallbackFrames{128, 257, 64, 511, 1024};

    int64_t totalRendered = 0;
    for (size_t cycle = 0; cycle < 512; ++cycle) {
        const LoopRun run = RenderOneLoop(kBoundary, kRate, kCallbackFrames, cycle);
        EXPECT_EQ(run.renderedFrames, kLoopFrames);
        EXPECT_NEAR(run.cursorSeconds, 0.0, 1.0e-12);
        totalRendered += run.renderedFrames;
        EXPECT_EQ(totalRendered, static_cast<int64_t>(cycle + 1) * kLoopFrames);
    }
}

TEST(PreviewLoopDrift, FractionalBoundaryUsesStableNearestFrameEveryCycle) {
    constexpr double kRate = 48000.0;
    // 16 beats at 123 BPM does not land on an integer 48 kHz frame.
    constexpr double kBoundary = 16.0 * 60.0 / 123.0;
    constexpr int64_t kNearestLoopFrames = 374634;
    constexpr std::array kCallbackFrames{127, 128, 257, 1024, 63, 511};

    int64_t totalRendered = 0;
    for (size_t cycle = 0; cycle < 512; ++cycle) {
        const LoopRun run = RenderOneLoop(kBoundary, kRate, kCallbackFrames, cycle);
        EXPECT_EQ(run.renderedFrames, kNearestLoopFrames);
        totalRendered += run.renderedFrames;
        EXPECT_EQ(totalRendered, static_cast<int64_t>(cycle + 1) * kNearestLoopFrames);
    }
}

TEST(PreviewLoopDrift, WrappedHostRequestCannotLeakFramesFromNextCycle) {
    constexpr double kRate = 48000.0;
    constexpr double kBoundary = 4.0;
    constexpr int kBlock = 128;

    // Fresh runtime logs showed the final callback at len - one frame. It must
    // render that frame, while an already wrapped/overrun old callback renders
    // nothing. This prevents one sample of silence or next-cycle audio at seam.
    const double oneFrameBeforeEnd = kBoundary - (1.0 / kRate);
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  oneFrameBeforeEnd, kBoundary, kRate, kBlock), 1);
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  kBoundary, kBoundary, kRate, kBlock), 0);
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  kBoundary + 0.245333, kBoundary, kRate, kBlock), 0);
}

} // namespace reals::test
