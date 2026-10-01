#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace reals::audio {

struct SampleAccurateCapture {
    uint64_t generation = 0;
    uint64_t callbackEpoch = 0;
    int64_t dawPhaseFrame = 0;
    int64_t loopFrames = 0;
};

class SampleAccurateHandshake final {
public:
    void configure(int64_t loopFrames, double loopBeats) noexcept {
        m_loopFrames.store(std::max<int64_t>(0, loopFrames), std::memory_order_relaxed);
        m_loopBeats.store(loopBeats > 0.0 ? loopBeats : 0.0, std::memory_order_relaxed);
    }

    uint64_t arm() noexcept {
        const uint64_t generation = m_nextGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
        m_armedGeneration.store(generation, std::memory_order_release);
        return generation;
    }

    void cancel() noexcept {
        m_armedGeneration.store(0, std::memory_order_release);
    }

    uint64_t beginCallback(bool dawPlaying, double fullBeats) noexcept {
        const uint64_t epoch = m_callbackEpoch.fetch_add(1, std::memory_order_relaxed) + 1;
        if (!dawPlaying) return epoch;

        const uint64_t generation = m_armedGeneration.load(std::memory_order_acquire);
        const int64_t loopFrames = m_loopFrames.load(std::memory_order_relaxed);
        const double loopBeats = m_loopBeats.load(std::memory_order_relaxed);
        if (generation == 0 || generation <= m_capturedGeneration.load(std::memory_order_relaxed) ||
            loopFrames <= 0 || loopBeats <= 0.0) {
            return epoch;
        }

        double beatInLoop = std::fmod(fullBeats, loopBeats);
        if (beatInLoop < 0.0) beatInLoop += loopBeats;
        int64_t phaseFrame = static_cast<int64_t>(std::llround(
            beatInLoop * static_cast<double>(loopFrames) / loopBeats));
        phaseFrame %= loopFrames;
        if (phaseFrame < 0) phaseFrame += loopFrames;

        m_capturedPhaseFrame.store(phaseFrame, std::memory_order_relaxed);
        m_capturedLoopFrames.store(loopFrames, std::memory_order_relaxed);
        m_capturedEpoch.store(epoch, std::memory_order_relaxed);
        m_capturedGeneration.store(generation, std::memory_order_release);
        return epoch;
    }

    bool consume(uint64_t& lastGeneration, SampleAccurateCapture& capture) const noexcept {
        const uint64_t generation = m_capturedGeneration.load(std::memory_order_acquire);
        if (generation == 0 || generation <= lastGeneration) return false;

        capture.dawPhaseFrame = m_capturedPhaseFrame.load(std::memory_order_relaxed);
        capture.loopFrames = m_capturedLoopFrames.load(std::memory_order_relaxed);
        capture.callbackEpoch = m_capturedEpoch.load(std::memory_order_relaxed);
        capture.generation = generation;
        lastGeneration = generation;
        return true;
    }

    [[nodiscard]] uint64_t callbackEpoch() const noexcept {
        return m_callbackEpoch.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint64_t> m_nextGeneration{0};
    std::atomic<uint64_t> m_armedGeneration{0};
    std::atomic<uint64_t> m_capturedGeneration{0};
    std::atomic<uint64_t> m_capturedEpoch{0};
    std::atomic<int64_t> m_capturedPhaseFrame{0};
    std::atomic<int64_t> m_capturedLoopFrames{0};
    std::atomic<int64_t> m_loopFrames{0};
    std::atomic<double> m_loopBeats{0.0};
    std::atomic<uint64_t> m_callbackEpoch{0};
};

inline int64_t WrapFrame(int64_t frame, int64_t loopFrames) noexcept {
    if (loopFrames <= 0) return std::max<int64_t>(0, frame);
    frame %= loopFrames;
    return frame < 0 ? frame + loopFrames : frame;
}

inline int64_t AlignedPreviewFrame(int64_t hostPreviewFrame,
                                   int64_t capturedHostPreviewFrame,
                                   int64_t capturedDawPhaseFrame,
                                   int64_t loopFrames) noexcept {
    return WrapFrame(capturedDawPhaseFrame + (hostPreviewFrame - capturedHostPreviewFrame), loopFrames);
}

} // namespace reals::audio
