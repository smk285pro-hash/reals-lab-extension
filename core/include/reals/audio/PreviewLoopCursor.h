#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace reals::audio {

// Small, allocation-free helpers shared by preview readers.  A preview source
// is asked for audio in output-time seconds while its decoder cursor is kept in
// source-time seconds.  Keeping the boundary math here makes it possible for
// both shells to use the same frame contract at a loop seam.
class PreviewLoopCursor final {
public:
    // Return the number of frames that can be read without crossing a loop
    // boundary.  A zero result means the caller should wrap the cursor before
    // trying another read.  Invalid/unbounded inputs leave the request intact.
    [[nodiscard]] static int FramesUntilBoundary(const double positionSeconds,
                                                 const double boundarySeconds,
                                                 const double sampleRate,
                                                 const int requestedFrames) noexcept {
        if (requestedFrames <= 0) return 0;
        if (!(boundarySeconds > 0.0) || !(sampleRate > 0.0) ||
            !std::isfinite(positionSeconds) || !std::isfinite(boundarySeconds) ||
            !std::isfinite(sampleRate)) {
            return requestedFrames;
        }

        const double remaining = (boundarySeconds - std::max(0.0, positionSeconds)) * sampleRate;
        if (!(remaining > 0.0) || !std::isfinite(remaining)) return 0;

        // The epsilon absorbs the last-bit error from seconds↔frames
        // conversion (for example 15.9973333333 s at 48 kHz should be 128
        // frames, not 127).  We still floor, so a read can never cross the
        // boundary.
        constexpr double kFrameEpsilon = 1.0e-6;
        const auto available = static_cast<int64_t>(std::floor(remaining + kFrameEpsilon));
        if (available <= 0) return 0;
        return static_cast<int>(std::min<int64_t>(available, requestedFrames));
    }

    // Wrap a positive/negative time position into [0, boundary).  This is
    // intentionally modulo based rather than a single subtraction because a
    // host can deliver a delayed callback several loop lengths after a seek.
    [[nodiscard]] static double WrapPosition(const double positionSeconds,
                                              const double boundarySeconds) noexcept {
        if (!(boundarySeconds > 0.0) || !std::isfinite(boundarySeconds) ||
            !std::isfinite(positionSeconds)) {
            return 0.0;
        }
        double wrapped = std::fmod(positionSeconds, boundarySeconds);
        if (wrapped < 0.0) wrapped += boundarySeconds;
        // Avoid returning the mathematical boundary after seconds↔frames
        // round-off.  A frame-grid position can land a few ulps below the
        // boundary (for example 3.9999999999999996 s for a 4 s loop), which
        // would otherwise be treated as a real position and make the next
        // callback observe a one-frame drift.  The tolerance is many orders
        // smaller than one audio frame at supported rates.
        constexpr double kBoundaryEpsilonSeconds = 1.0e-9;
        if (wrapped >= boundarySeconds ||
            boundarySeconds - wrapped <= kBoundaryEpsilonSeconds) {
            wrapped = 0.0;
        }
        return wrapped;
    }

    [[nodiscard]] static double AdvanceWrapped(const double positionSeconds,
                                                const int frames,
                                                const double sampleRate,
                                                const double boundarySeconds) noexcept {
        if (frames <= 0 || !(sampleRate > 0.0) || !(boundarySeconds > 0.0) ||
            !std::isfinite(sampleRate)) {
            return WrapPosition(positionSeconds, boundarySeconds);
        }
        return WrapPosition(positionSeconds + static_cast<double>(frames) / sampleRate,
                            boundarySeconds);
    }

    [[nodiscard]] static double RawLoopSeconds(const double loopBeats,
                                                const double sampleBpm) noexcept {
        if (!(loopBeats > 0.0) || !(sampleBpm > 0.0) ||
            !std::isfinite(loopBeats) || !std::isfinite(sampleBpm)) {
            return 0.0;
        }
        return loopBeats * 60.0 / sampleBpm;
    }

    [[nodiscard]] static double OutputLoopSeconds(const double loopBeats,
                                                   const double projectBpm) noexcept {
        return RawLoopSeconds(loopBeats, projectBpm);
    }
};

} // namespace reals::audio
