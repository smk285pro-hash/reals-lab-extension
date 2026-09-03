#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace reals::test {

// ============================================================================
// 1. Digital Biquad IIR Filter (Butterworth Lowpass / Highpass for 20-150Hz)
// ============================================================================
class BiquadFilter {
public:
    enum class Type { LowPass, HighPass, BandPass };

    void setup(Type type, float sampleRate, float cutoffHz, float Q = 0.7071f) {
        if (sampleRate <= 0.0f || cutoffHz <= 0.0f) return;
        const float w0 = 2.0f * 3.14159265358979323846f * (cutoffHz / sampleRate);
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);
        const float alpha = sinw0 / (2.0f * Q);

        float a0 = 1.0f;
        if (type == Type::LowPass) {
            m_b0 = (1.0f - cosw0) / 2.0f;
            m_b1 = 1.0f - cosw0;
            m_b2 = (1.0f - cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            m_a1 = -2.0f * cosw0;
            m_a2 = 1.0f - alpha;
        } else if (type == Type::HighPass) {
            m_b0 = (1.0f + cosw0) / 2.0f;
            m_b1 = -(1.0f + cosw0);
            m_b2 = (1.0f + cosw0) / 2.0f;
            a0 = 1.0f + alpha;
            m_a1 = -2.0f * cosw0;
            m_a2 = 1.0f - alpha;
        } else { // BandPass
            m_b0 = alpha;
            m_b1 = 0.0f;
            m_b2 = -alpha;
            a0 = 1.0f + alpha;
            m_a1 = -2.0f * cosw0;
            m_a2 = 1.0f - alpha;
        }
        m_b0 /= a0; m_b1 /= a0; m_b2 /= a0;
        m_a1 /= a0; m_a2 /= a0;
        reset();
    }

    void reset() {
        m_x1 = m_x2 = m_y1 = m_y2 = 0.0f;
    }

    float process(float in) {
        float out = m_b0 * in + m_b1 * m_x1 + m_b2 * m_x2 - m_a1 * m_y1 - m_a2 * m_y2;
        m_x2 = m_x1; m_x1 = in;
        m_y2 = m_y1; m_y1 = out;
        return out;
    }

    std::vector<float> processBuffer(const std::vector<float>& input) {
        reset();
        std::vector<float> out(input.size(), 0.0f);
        for (size_t i = 0; i < input.size(); ++i) {
            out[i] = process(input[i]);
        }
        return out;
    }

private:
    float m_b0 = 1.0f, m_b1 = 0.0f, m_b2 = 0.0f;
    float m_a1 = 0.0f, m_a2 = 0.0f;
    float m_x1 = 0.0f, m_x2 = 0.0f, m_y1 = 0.0f, m_y2 = 0.0f;
};

// ============================================================================
// 2. Programmatic 1st & 2nd Derivative Pop / Click / Step Discontinuity Metrics
// ============================================================================
struct DiscontinuityReport {
    size_t totalFrames = 0;
    size_t stepDiscontinuities = 0;
    size_t popGlitches = 0;
    float maxFirstDerivative = 0.0f;
    float maxSecondDerivative = 0.0f;
    float onsetJump = 0.0f;
    std::vector<size_t> glitchFrameIndices;
};

inline DiscontinuityReport analyzeBufferDiscontinuities(
    const float* pcmInterleaved, size_t frames, int channels,
    float maxStepThreshold = 0.40f,
    float maxGlitchThreshold = 0.25f) {

    DiscontinuityReport rep;
    rep.totalFrames = frames;
    if (!pcmInterleaved || frames < 4 || channels <= 0) return rep;

    for (int c = 0; c < channels; ++c) {
        // 1. Check Onset Discontinuity at frame 0
        float onset = std::abs(pcmInterleaved[c]);
        rep.onsetJump = std::max(rep.onsetJump, onset);

        // 2. Check continuous derivatives
        for (size_t i = 1; i < frames; ++i) {
            float cur = pcmInterleaved[i * static_cast<size_t>(channels) + static_cast<size_t>(c)];
            float prev = pcmInterleaved[(i - 1) * static_cast<size_t>(channels) + static_cast<size_t>(c)];
            float d1 = std::abs(cur - prev);
            rep.maxFirstDerivative = std::max(rep.maxFirstDerivative, d1);

            if (d1 > maxStepThreshold) {
                rep.stepDiscontinuities++;
            }

            if (i >= 2) {
                float prev2 = pcmInterleaved[(i - 2) * static_cast<size_t>(channels) + static_cast<size_t>(c)];
                float d2 = std::abs(cur - 2.0f * prev + prev2);
                rep.maxSecondDerivative = std::max(rep.maxSecondDerivative, d2);
            }

            if (i + 1 < frames) {
                float next = pcmInterleaved[(i + 1) * static_cast<size_t>(channels) + static_cast<size_t>(c)];
                // Isolated single-sample pop glitch metric:
                float glitch = std::abs(cur - 0.5f * (prev + next));
                if (glitch > maxGlitchThreshold) {
                    rep.popGlitches++;
                    rep.glitchFrameIndices.push_back(i);
                }
            }
        }
    }
    return rep;
}

// ============================================================================
// 3. Sub-Bass (20-150Hz) & Kick Punch Energy Preservation Measurement
// ============================================================================
struct KickPunchMetric {
    size_t kickCount = 0;
    size_t droppedKicks = 0;
    float avgPunchRatio = 0.0f;
    float minPunchRatio = 1.0f;
    float avgSubBassEnergyRatio = 0.0f;
    float minSubBassEnergyRatio = 1.0f;
};

inline KickPunchMetric measureKickAndSubBassPreservation(
    const std::vector<float>& originalAudio,
    const std::vector<float>& stretchedAudio,
    float timeRatio,
    int sampleRate) {

    KickPunchMetric metric;
    if (originalAudio.empty() || stretchedAudio.empty() || sampleRate <= 0 || timeRatio <= 0.0f) return metric;

    // 1. Isolate 20Hz - 150Hz Sub-Bass Band
    BiquadFilter lpFilter;
    lpFilter.setup(BiquadFilter::Type::LowPass, static_cast<float>(sampleRate), 150.0f);
    BiquadFilter hpFilter;
    hpFilter.setup(BiquadFilter::Type::HighPass, static_cast<float>(sampleRate), 20.0f);

    auto subOrig = hpFilter.processBuffer(lpFilter.processBuffer(originalAudio));
    auto subStretched = hpFilter.processBuffer(lpFilter.processBuffer(stretchedAudio));

    // 2. Detect Transient Kick Onsets in Original Audio
    std::vector<size_t> kickFrames;
    const size_t minDistance = static_cast<size_t>(sampleRate * 60 / 180); // Max 180 BPM
    float maxSubPeak = 0.0f;
    for (float v : subOrig) {
        if (std::abs(v) > maxSubPeak) maxSubPeak = std::abs(v);
    }
    const float kickThreshold = std::max(0.15f, maxSubPeak * 0.35f);

    for (size_t i = 100; i + 1000 < subOrig.size(); ++i) {
        if (std::abs(subOrig[i]) >= kickThreshold) {
            bool isMax = true;
            for (size_t k = (i > 150 ? i - 150 : 0); k < std::min(i + 150, subOrig.size()); ++k) {
                if (std::abs(subOrig[k]) > std::abs(subOrig[i])) { isMax = false; break; }
            }
            if (isMax) {
                if (kickFrames.empty() || (i - kickFrames.back() > minDistance)) {
                    kickFrames.push_back(i);
                }
            }
        }
    }

    if (kickFrames.empty()) return metric;
    metric.kickCount = kickFrames.size();

    const size_t burstWindow = static_cast<size_t>(0.080f * static_cast<float>(sampleRate)); // 80ms kick body
    float totalPunchRatio = 0.0f;
    float totalEnergyRatio = 0.0f;

    for (size_t kFrame : kickFrames) {
        // Calculate original 80ms burst energy & peak punch
        float origPeak = 0.0f;
        float origEnergy = 0.0f;
        const size_t oEnd = std::min(kFrame + burstWindow, subOrig.size());
        for (size_t i = kFrame; i < oEnd; ++i) {
            float v = std::abs(subOrig[i]);
            if (v > origPeak) origPeak = v;
            origEnergy += v * v;
        }

        // Expected position under time stretch
        const size_t expectedFrame = static_cast<size_t>(static_cast<float>(kFrame) / timeRatio);
        if (expectedFrame >= subStretched.size()) break;

        // Search +/- 30ms around expected frame
        const size_t sWindow = static_cast<size_t>(0.030f * static_cast<float>(sampleRate));
        const size_t sStart = expectedFrame > sWindow ? expectedFrame - sWindow : 0;
        const size_t sEnd = std::min(expectedFrame + sWindow + burstWindow, subStretched.size());

        float stretchPeak = 0.0f;
        size_t bestPeakIdx = expectedFrame;
        for (size_t j = sStart; j < sEnd; ++j) {
            float v = std::abs(subStretched[j]);
            if (v > stretchPeak) {
                stretchPeak = v;
                bestPeakIdx = j;
            }
        }

        // Measure stretched 80ms burst energy
        float stretchEnergy = 0.0f;
        const size_t burstEnd = std::min(bestPeakIdx + static_cast<size_t>(static_cast<float>(burstWindow) / timeRatio), subStretched.size());
        for (size_t j = bestPeakIdx; j < burstEnd; ++j) {
            float v = std::abs(subStretched[j]);
            stretchEnergy += v * v;
        }

        const float punchRatio = (origPeak > 1e-4f) ? (stretchPeak / origPeak) : 1.0f;
        const float normOrigEnergy = (origEnergy / timeRatio) + 1e-6f;
        const float energyRatio = stretchEnergy / normOrigEnergy;

        totalPunchRatio += punchRatio;
        totalEnergyRatio += energyRatio;
        metric.minPunchRatio = std::min(metric.minPunchRatio, punchRatio);
        metric.minSubBassEnergyRatio = std::min(metric.minSubBassEnergyRatio, energyRatio);

        // Dropped kick: punch ratio < 0.35 (-9dB) or significant collapse from loud original
        if (punchRatio < 0.35f || (origPeak >= 0.30f && stretchPeak < 0.15f)) {
            metric.droppedKicks++;
        }
    }

    metric.avgPunchRatio = totalPunchRatio / static_cast<float>(kickFrames.size());
    metric.avgSubBassEnergyRatio = totalEnergyRatio / static_cast<float>(kickFrames.size());
    return metric;
}

} // namespace reals::test
