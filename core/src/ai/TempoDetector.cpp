#include "reals/ai/TempoDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace reals::ai {

namespace {
constexpr std::string_view kTag = "TempoDetector";
constexpr int kTargetSampleRate = 44100;
constexpr int kHopLength = 256;
constexpr int kFftSize = 1024;
constexpr float kMaxAnalysisSec = 60.0f;

// Disambiguate tempo octaves preferring standard musical production range (45 - 230 BPM)
float disambiguateBpm(float bpm) {
    if (bpm <= 0.0f || !std::isfinite(bpm)) return 120.0f;
    while (bpm < 45.0f) {
        bpm *= 2.0f;
    }
    while (bpm > 230.0f) {
        bpm /= 2.0f;
    }
    return bpm;
}

// Early unpitched percussion one-shot detector:
// Discards kicks, snares, claps, hats, and short impacts from receiving false BPM ratings.
bool isQuickOneShot(const std::vector<float>& audio, int sampleRate, float durationSec) {
    if (durationSec < 0.35f) return true;

    // Check energy decay: if > 92% of total energy is in the first 250ms
    if (durationSec < 1.2f && audio.size() > static_cast<size_t>(sampleRate / 4)) {
        float totalEnergy = 0.0f;
        float headEnergy = 0.0f;
        const size_t headFrames = static_cast<size_t>(0.25f * static_cast<float>(sampleRate));
        for (size_t i = 0; i < audio.size(); ++i) {
            const float e = audio[i] * audio[i];
            totalEnergy += e;
            if (i < headFrames) headEnergy += e;
        }
        if (totalEnergy > 1e-6f && (headEnergy / totalEnergy) > 0.92f) {
            return true;
        }
    }
    return false;
}

// Lightweight beat onset extractor (O(T), without expensive O(T*L*K) autocorrelation)
std::vector<float> extractBeatOnsetsFromAudio(const std::vector<float>& audio, int sampleRate) {
    auto stft = FeatureExtractor::computeStftMagnitude(audio, kFftSize, kHopLength);
    if (stft.size() < 2) return {};

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size();
    const size_t kMaxBin = std::min<size_t>(256, numBins);

    std::vector<float> onset1D(numFrames - 1, 0.0f);
    float max1D = 0.0f;

    std::vector<float> prevLog(kMaxBin, 0.0f);
    for (size_t k = 1; k < kMaxBin; ++k) {
        prevLog[k] = std::log1p(100.0f * stft[0][k]);
    }

    for (size_t t = 1; t < numFrames; ++t) {
        float frameSum = 0.0f;
        for (size_t k = 1; k < kMaxBin; ++k) {
            const float currLog = std::log1p(100.0f * stft[t][k]);
            const float d = currLog - prevLog[k];
            if (d > 0.0f) frameSum += d;
            prevLog[k] = currLog;
        }
        onset1D[t - 1] = frameSum;
        max1D = std::max(max1D, frameSum);
    }

    if (max1D > 1e-6f) {
        for (float& v : onset1D) v /= max1D;
    }

    std::vector<float> onsets;
    const float hopSeconds = static_cast<float>(kHopLength) / static_cast<float>(sampleRate);
    constexpr float kThreshold = 0.20f;
    for (size_t t = 1; t + 1 < onset1D.size(); ++t) {
        // Strict one-sided peak picking (prevents plateau double-counting)
        if (onset1D[t] > kThreshold && onset1D[t] > onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]) {
            // Fix off-by-one: flux between t and t+1 corresponds to transient at frame t+1
            const float timeSec = static_cast<float>(t + 1) * hopSeconds;
            onsets.push_back(std::round(timeSec * 1000.0f) / 1000.0f);
        }
    }
    return onsets;
}

} // namespace

TempoResult TempoDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return TempoResult{0.0f, 0.0f, {}, "invalid_input"};
    }

    // 1. Resample to standard 44.1kHz mono once for the entire pipeline
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetSampleRate);
    const float durationSec = static_cast<float>(audio.size()) / static_cast<float>(kTargetSampleRate);

    // 2. Early One-Shot Gate: Check before CNN and before Algorithmic
    if (isQuickOneShot(audio, kTargetSampleRate, durationSec)) {
        return TempoResult{0.0f, 0.0f, {}, "oneshot_rejected"};
    }

    // 3. CNN Path: Only run if model available, without duplicate algorithmic computation
    if (ModelManager::instance().isModelAvailable("tempo_cnn")) {
        auto result = detectCnn(pcm, frames, sampleRate);
        if (result.confidence > 0.4f && result.bpm > 0.0f) {
            return result;
        }
    }

    // 4. Algorithmic Fallback Path (Guaranteed to execute exactly once)
    return detectAlgorithmic(pcm, frames, sampleRate);
}

TempoResult TempoDetector::detectCnn(const float* pcm, size_t frames, int sampleRate) {
    if (!ModelManager::instance().ensureModelLoaded("tempo_cnn")) {
        return detectAlgorithmic(pcm, frames, sampleRate);
    }

    // Step 1: Resample to 11025 Hz for TempoCNN input
    constexpr int kCnnSampleRate = 11025;
    auto monoAudio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kCnnSampleRate);
    if (monoAudio.size() < 11025) {
        return detectAlgorithmic(pcm, frames, sampleRate);
    }

    // Step 2: Compute Log-Mel Spectrogram
    SpectrogramConfig cfg;
    cfg.sampleRate = kCnnSampleRate;
    cfg.nFft = 1024;
    cfg.hopLength = 256;
    cfg.nMels = 40;
    cfg.fMin = 20.0f;
    cfg.fMax = 5000.0f;

    auto logMel = FeatureExtractor::computeLogMel(monoAudio, cfg);
    if (logMel.empty()) {
        return detectAlgorithmic(pcm, frames, sampleRate);
    }

    // Flatten input tensor: [1, 1, time_frames, 40]
    std::vector<float> inputData;
    inputData.reserve(logMel.size() * 40);
    for (const auto& frame : logMel) {
        inputData.insert(inputData.end(), frame.begin(), frame.end());
    }

    std::vector<float> outputProbs;
    const std::vector<int64_t> shape = {1, 1, static_cast<int64_t>(logMel.size()), 40};
    if (!OnnxEngine::instance().runSingle("tempo_cnn", inputData, shape, outputProbs) || outputProbs.empty()) {
        return detectAlgorithmic(pcm, frames, sampleRate);
    }

    // Find peak probability bin (bins span 30 to 286 BPM)
    // Centers of bins: 30.0f + (peakIdx + 0.5f) * binWidth (eliminates 0.5 BPM edge bias)
    auto maxIt = std::max_element(outputProbs.begin(), outputProbs.end());
    const size_t peakIdx = std::distance(outputProbs.begin(), maxIt);
    const float binWidth = 256.0f / static_cast<float>(outputProbs.size());
    const float rawBpm = 30.0f + (static_cast<float>(peakIdx) + 0.5f) * binWidth;
    const float confidence = std::clamp(*maxIt, 0.0f, 1.0f);

    TempoResult res;
    res.bpm = std::round(disambiguateBpm(rawBpm) * 10.0f) / 10.0f;
    res.confidence = confidence;
    // Lightweight onset extraction without re-running O(T*L*K) autocorrelation
    auto audio44k = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetSampleRate);
    res.beatOnsets = extractBeatOnsetsFromAudio(audio44k, kTargetSampleRate);
    res.method = "tempo_cnn";

    LOG_DEBUG(kTag, "TempoCNN detected BPM: " + std::to_string(res.bpm) + " (confidence: " + std::to_string(res.confidence) + ")");
    return res;
}

TempoResult TempoDetector::detectAlgorithmic(const float* pcm, size_t frames, int sampleRate) {
    TempoResult res;
    res.method = "multiband_comb_resonator_2026";

    if (!pcm || frames == 0 || sampleRate <= 0) {
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    // 1. Resample to standard 44.1kHz mono
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetSampleRate);
    const float durationSec = static_cast<float>(audio.size()) / static_cast<float>(kTargetSampleRate);

    if (isQuickOneShot(audio, kTargetSampleRate, durationSec)) {
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    // 2. Peak normalization (ensures invariant logarithmic spectral flux behavior)
    float peak = 0.0f;
    for (float s : audio) {
        peak = std::max(peak, std::abs(s));
    }
    if (peak > 1e-5f) {
        const float invPeak = 1.0f / peak;
        for (float& s : audio) {
            s *= invPeak;
        }
    }

    // 3. Cap analysis window to kMaxAnalysisSec (~60s) taken from the representative middle
    const size_t maxSamples = static_cast<size_t>(kMaxAnalysisSec * static_cast<float>(kTargetSampleRate));
    if (audio.size() > maxSamples) {
        const size_t startIdx = (audio.size() - maxSamples) / 2;
        audio.assign(audio.begin() + startIdx, audio.begin() + startIdx + maxSamples);
    }

    // 4. Compute STFT magnitude
    auto stft = FeatureExtractor::computeStftMagnitude(audio, kFftSize, kHopLength);
    if (stft.size() < 10) {
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size(); // 513 bins (~43.07 Hz/bin)
    const size_t kMaxBin = std::min<size_t>(256, numBins); // Focus on musically relevant 0 - 11 kHz

    // 5. Cache log-spectrogram in a contiguous flat 1D buffer (Single log1p per bin)
    std::vector<float> logSpec(numFrames * kMaxBin, 0.0f);
    for (size_t t = 0; t < numFrames; ++t) {
        const size_t rowOffset = t * kMaxBin;
        const auto& stftFrame = stft[t];
        for (size_t k = 1; k < kMaxBin; ++k) {
            logSpec[rowOffset + k] = std::log1p(100.0f * stftFrame[k]);
        }
    }

    // 6. Compute 2D Spectral Flux Novelty Matrix in a contiguous flat 1D buffer
    const size_t numDiffFrames = numFrames - 1;
    const size_t numDiffBins = kMaxBin - 1;
    std::vector<float> diff(numDiffFrames * numDiffBins, 0.0f);
    std::vector<float> onset1D(numDiffFrames, 0.0f);
    float max1D = 0.0f;

    for (size_t t = 0; t < numDiffFrames; ++t) {
        const size_t currRow = (t + 1) * kMaxBin;
        const size_t prevRow = t * kMaxBin;
        const size_t diffRow = t * numDiffBins;
        float frameSum = 0.0f;

        for (size_t k = 1; k < kMaxBin; ++k) {
            const float d = logSpec[currRow + k] - logSpec[prevRow + k];
            if (d > 0.0f) {
                diff[diffRow + (k - 1)] = d;
                frameSum += d;
            }
        }
        onset1D[t] = frameSum;
        max1D = std::max(max1D, frameSum);
    }

    if (max1D > 1e-6f) {
        const float invMax = 1.0f / max1D;
        for (float& v : onset1D) v *= invMax;
    }

    // 7. Onset Count & Density Metric (Strict one-sided peak picking to prevent plateau inflation)
    size_t onsetCount = 0;
    constexpr float kOnsetThreshold = 0.15f;
    for (size_t t = 1; t + 1 < numDiffFrames; ++t) {
        if (onset1D[t] > kOnsetThreshold && onset1D[t] > onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]) {
            ++onsetCount;
        }
    }

    const float effectiveDurationSec = static_cast<float>(audio.size()) / static_cast<float>(kTargetSampleRate);
    const float onsetDensity = static_cast<float>(onsetCount) / std::max(0.1f, effectiveDurationSec);

    // 8. 2D Spectral Autocorrelation over BPM range 48..225 BPM
    const float frameRate = static_cast<float>(kTargetSampleRate) / static_cast<float>(kHopLength); // ~172.2656 Hz
    const int minLag = std::max(1, static_cast<int>(std::round(frameRate * 60.0f / 225.0f)));      // ~46 frames
    const int maxLag = std::min(static_cast<int>(numDiffFrames) - 1, static_cast<int>(std::round(frameRate * 60.0f / 48.0f))); // ~215 frames

    if (maxLag <= minLag) {
        res.bpm = 120.0f;
        res.confidence = 0.2f;
        return res;
    }

    const int fullMaxLag = std::min(static_cast<int>(numDiffFrames) - 1, 2 * maxLag + 10);
    std::vector<float> acf(fullMaxLag + 1, 0.0f);
    const float invFrames = 1.0f / static_cast<float>(numDiffFrames);

    for (int lag = 1; lag <= fullMaxLag; ++lag) {
        float sum = 0.0f;
        for (size_t t = 0; t + lag < numDiffFrames; ++t) {
            const size_t offsetA = t * numDiffBins;
            const size_t offsetB = (t + lag) * numDiffBins;
            for (size_t k = 0; k < numDiffBins; ++k) {
                sum += diff[offsetA + k] * diff[offsetB + k];
            }
        }
        // Biased sample autocorrelation estimator (scaled by signal length for level invariance)
        acf[lag] = sum * invFrames;
    }

    // 9. Comb filter resonator bank scoring
    float bestScore = -1.0f;
    int bestLag = minLag;
    std::vector<float> combScores(maxLag + 1, 0.0f);

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float score = acf[lag] + 0.5f * (lag * 2 <= fullMaxLag ? acf[lag * 2] : 0.0f);

        // Smooth Log-Normal prior centered at 120 BPM (stddev ~ 0.85 octaves)
        const float candBpm = (frameRate * 60.0f) / static_cast<float>(lag);
        const float octaveDiff = std::log2(candBpm / 120.0f);
        const float prior = std::exp(-0.5f * std::pow(octaveDiff / 0.85f, 2.0f));
        score *= (0.75f + 0.25f * prior);
        combScores[lag] = score;

        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }

    // 10. Non-Cascading Adaptive Octave Disambiguation
    float candBpm = (frameRate * 60.0f) / static_cast<float>(bestLag);
    const int doubleLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f));

    if (candBpm >= 135.0f && candBpm <= 145.0f && doubleLag <= maxLag) {
        if (onsetDensity < 3.5f && acf[doubleLag] > 0.70f * acf[bestLag]) {
            bestLag = doubleLag;
        }
    } else if (candBpm >= 190.0f && candBpm <= 205.0f && doubleLag <= maxLag) {
        if (acf[doubleLag] > 0.70f * acf[bestLag]) {
            bestLag = doubleLag;
        }
    } else if (candBpm >= 100.0f && candBpm <= 110.0f) {
        const int tripletLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f / 3.0f));
        if (tripletLag >= minLag && acf[tripletLag] > 0.85f * acf[bestLag]) {
            bestLag = tripletLag;
        }
    } else if (candBpm >= 64.0f && candBpm <= 68.0f) {
        const int tripletLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f / 3.0f));
        if (tripletLag >= minLag && acf[tripletLag] >= 0.95f * acf[bestLag]) {
            bestLag = tripletLag;
        }
    }

    // 11. Sub-frame Parabolic Peak Interpolation with correct vertex formula
    float exactLag = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        const float alpha = acf[bestLag - 1];
        const float beta = acf[bestLag];
        const float gamma = acf[bestLag + 1];
        const float denom = alpha - 2.0f * beta + gamma; // negative at peak
        if (std::abs(denom) > 1e-6f) {
            const float delta = 0.5f * (alpha - gamma) / denom;
            if (std::abs(delta) < 1.0f) {
                exactLag += delta;
            }
        }
    }

    float calculatedBpm = (frameRate * 60.0f) / exactLag;
    calculatedBpm = disambiguateBpm(calculatedBpm);
    res.bpm = std::round(calculatedBpm * 10.0f) / 10.0f;

    // 12. Confidence metric based on comb score prominence (same metric scale)
    float meanScore = 0.0f;
    int scoreCount = 0;
    for (int l = minLag; l <= maxLag; ++l) {
        meanScore += combScores[l];
        ++scoreCount;
    }
    meanScore = (scoreCount > 0) ? (meanScore / static_cast<float>(scoreCount)) : 1.0f;
    res.confidence = std::clamp((bestScore / (meanScore + 1e-6f) - 1.0f) / 2.5f, 0.2f, 0.99f);

    // 13. Beat onset timestamps extraction via peak picking (with fixed 1-frame offset)
    const float hopSeconds = static_cast<float>(kHopLength) / static_cast<float>(kTargetSampleRate);
    constexpr float kTimestampThreshold = 0.20f;
    for (size_t t = 1; t + 1 < numDiffFrames; ++t) {
        if (onset1D[t] > kTimestampThreshold && onset1D[t] > onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]) {
            // Transient occurs at frame t+1 (between frame t and t+1)
            const float timeSec = static_cast<float>(t + 1) * hopSeconds;
            res.beatOnsets.push_back(std::round(timeSec * 1000.0f) / 1000.0f);
        }
    }

    return res;
}

} // namespace reals::ai
