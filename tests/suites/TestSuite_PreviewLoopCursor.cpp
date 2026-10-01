#include <cmath>

#include "../framework/TestRunner.h"
#include <reals/audio/PreviewLoopCursor.h>

namespace reals::test {

TEST(PreviewLoopCursor, RawReadNeverCrossesNominalBoundary) {
    constexpr double kRawBoundary = 15.0; // 32 beats at 128 BPM
    constexpr double kRate = 48000.0;
    constexpr int kChunk = 2048;

    // The source is 16.875 s long, but its musical loop ends at 15 s.  A
    // read-ahead chunk must be split at 15 s; otherwise the 1.875 s tail leaks
    // into the next cycle and shifts the audible downbeat.
    double cursor = 14.98;
    const int first = audio::PreviewLoopCursor::FramesUntilBoundary(
        cursor, kRawBoundary, kRate, kChunk);
    EXPECT_EQ(first, 960); // 20 ms exactly
    cursor = audio::PreviewLoopCursor::AdvanceWrapped(cursor, first, kRate, kRawBoundary);
    EXPECT_NEAR(cursor, 0.0, 1.0e-9);

    const int second = audio::PreviewLoopCursor::FramesUntilBoundary(
        cursor, kRawBoundary, kRate, kChunk);
    EXPECT_EQ(second, kChunk);
    cursor = audio::PreviewLoopCursor::AdvanceWrapped(cursor, second, kRate, kRawBoundary);
    EXPECT_NEAR(cursor, static_cast<double>(kChunk) / kRate, 1.0e-9);
}

TEST(PreviewLoopCursor, OutputRequestStopsAtBoundaryInsteadOfOverreading) {
    constexpr double kBoundary = 16.0;
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 128;

    const double beforeEnd = (kBoundary * kRate - kBlock) / kRate;
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  beforeEnd, kBoundary, kRate, kBlock), kBlock);

    // This is the observed REAPER callback after the old implementation had
    // already advanced 245 ms past the nominal loop.  It must produce no
    // frames; the host can then wrap and request the next cycle at zero.
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  16.245333, kBoundary, kRate, kBlock), 0);

    const double wrapped = audio::PreviewLoopCursor::WrapPosition(16.245333, kBoundary);
    EXPECT_NEAR(wrapped, 0.245333, 1.0e-6);
}

TEST(PreviewLoopCursor, RawAndOutputMusicalBoundariesAreIndependentOfSampleRate) {
    constexpr double kBeats = 32.0;
    const double rawSeconds = audio::PreviewLoopCursor::RawLoopSeconds(kBeats, 128.0);
    const double outputSeconds = audio::PreviewLoopCursor::OutputLoopSeconds(kBeats, 120.0);
    EXPECT_NEAR(rawSeconds, 15.0, 1.0e-12);
    EXPECT_NEAR(outputSeconds, 16.0, 1.0e-12);

    // A 44.1 kHz source rendered at 48 kHz still has the same time boundary;
    // only the frame count changes.
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  14.0, rawSeconds, 44100.0, 100000), 44100);
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  14.0, rawSeconds, 48000.0, 100000), 48000);
}

TEST(PreviewLoopCursor, ModuloWrapHandlesDelayedAndNegativePositions) {
    EXPECT_NEAR(audio::PreviewLoopCursor::WrapPosition(32.5, 16.0), 0.5, 1.0e-12);
    EXPECT_NEAR(audio::PreviewLoopCursor::WrapPosition(-0.5, 16.0), 15.5, 1.0e-12);
    EXPECT_NEAR(audio::PreviewLoopCursor::WrapPosition(16.0, 16.0), 0.0, 1.0e-12);
    EXPECT_EQ(audio::PreviewLoopCursor::FramesUntilBoundary(
                  0.0, 0.0, 48000.0, 128), 128);
}

} // namespace reals::test
