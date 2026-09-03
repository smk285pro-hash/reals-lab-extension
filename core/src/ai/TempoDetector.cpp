#include "reals/ai/TempoDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace reals::ai {

namespace {
constexpr std::string_view kTag = "TempoDetector";
constexpr int kTargetSampleRate = 44100;
constexpr int kHopLength = 256;
constexpr int kFftSize = 1024;

// Disambiguate tempo octaves preferring musical range (45 - 230 BPM)
// Replaces rigid < 70 and > 180 clamping with smooth musical bounds.
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

// Unpitched percussion one-shot detector:
// Discards kicks, snares, claps, hats, and short impacts from false tempo assignment.
bool isLikelyOneShot(const std::vector<float>& audio, int sampleRate, size_t numOnsets, float durationSec) {
    if (durationSec < 0.35f) return true;
    if (durationSec < 1.0f && numOnsets <= 2) return true;

    // Check energy decay: if > 92% of total energy is in the first 250ms with <= 2 onsets
    if (audio.size() > static_cast<size_t>(sampleRate / 4)) {
        float totalEnergy = 0.0f;
        float headEnergy = 0.0f;
        const size_t headFrames = static_cast<size_t>(0.25f * static_cast<float>(sampleRate));
        for (size_t i = 0; i < audio.size(); ++i) {
            const float e = audio[i] * audio[i];
            totalEnergy += e;
            if (i < headFrames) headEnergy += e;
        }
        if (totalEnergy > 1e-6f && (headEnergy / totalEnergy) > 0.92f && numOnsets <= 2) {
            return true;
        }
    }
    return false;
}

} // namespace

TempoResult TempoDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return TempoResult{0.0f, 0.0f, {}, "invalid_input"};
    }

    if (ModelManager::instance().isModelAvailable("tempo_cnn")) {
        auto result = detectCnn(pcm, frames, sampleRate);
        if (result.confidence > 0.4f) {
            return result;
        }
    }

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
    auto maxIt = std::max_element(outputProbs.begin(), outputProbs.end());
    const size_t peakIdx = std::distance(outputProbs.begin(), maxIt);
    const float rawBpm = 30.0f + static_cast<float>(peakIdx) * (256.0f / static_cast<float>(outputProbs.size()));
    const float confidence = std::clamp(*maxIt, 0.0f, 1.0f);

    // Beat onsets from onset envelope
    auto fallback = detectAlgorithmic(pcm, frames, sampleRate);

    TempoResult res;
    res.bpm = std::round(rawBpm * 10.0f) / 10.0f;
    res.confidence = confidence;
    res.beatOnsets = std::move(fallback.beatOnsets);
    res.method = "tempo_cnn";

    LOG_DEBUG(kTag, "TempoCNN detected BPM successfully");
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

    if (durationSec < 0.35f) {
        // Audio too short (< 0.35s) -> unpitched one-shot
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    // 2. Compute 2D Spectral Flux Novelty Matrix
    // STFT with N=1024, hop=256 (frame rate ~172.27 Hz for sub-frame precision)
    auto stft = FeatureExtractor::computeStftMagnitude(audio, kFftSize, kHopLength);
    if (stft.size() < 10) {
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size(); // 513 bins (~43.07 Hz/bin)
    const size_t kMaxBin = std::min<size_t>(256, numBins); // Focus on musically relevant 0 - 11 kHz

    // Precompute log-compressed positive spectral flux matrix [numFrames - 1, kMaxBin - 1]
    const size_t numDiffFrames = numFrames - 1;
    const size_t numDiffBins = kMaxBin - 1;
    std::vector<std::vector<float>> diff(numDiffFrames, std::vector<float>(numDiffBins, 0.0f));
    std::vector<float> onset1D(numDiffFrames, 0.0f);
    float max1D = 0.0f;

    for (size_t t = 1; t < numFrames; ++t) {
        float frameSum = 0.0f;
        for (size_t k = 1; k < kMaxBin; ++k) {
            const float logCurr = std::log1p(100.0f * stft[t][k]);
            const float logPrev = std::log1p(100.0f * stft[t - 1][k]);
            const float d = logCurr - logPrev;
            if (d > 0.0f) {
                diff[t - 1][k - 1] = d;
                frameSum += d;
            }
        }
        onset1D[t - 1] = frameSum;
        max1D = std::max(max1D, frameSum);
    }

    if (max1D > 1e-6f) {
        for (float& v : onset1D) v /= max1D;
    }

    // 3. Onset Count & Density Metric
    size_t onsetCount = 0;
    for (size_t t = 1; t + 1 < numDiffFrames; ++t) {
        if (onset1D[t] > 0.15f && onset1D[t] >= onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]) {
            ++onsetCount;
        }
    }

    // Check for unpitched percussion one-shot
    if (isLikelyOneShot(audio, kTargetSampleRate, onsetCount, durationSec)) {
        res.bpm = 0.0f;
        res.confidence = 0.0f;
        return res;
    }

    const float onsetDensity = static_cast<float>(onsetCount) / std::max(0.1f, durationSec);

    // 4. 2D Spectral Autocorrelation over BPM range 48..225 BPM
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
    for (int lag = 1; lag <= fullMaxLag; ++lag) {
        float sum = 0.0f;
        for (size_t t = 0; t + lag < numDiffFrames; ++t) {
            const auto& rowA = diff[t];
            const auto& rowB = diff[t + lag];
            for (size_t k = 0; k < numDiffBins; ++k) {
                sum += rowA[k] * rowB[k];
            }
        }
        acf[lag] = sum;
    }

    // 5. Comb filter resonator bank scoring
    float bestScore = -1.0f;
    int bestLag = minLag;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float score = acf[lag];
        float wSum = 1.0f;
        if (lag * 2 <= fullMaxLag) {
            score += 0.5f * acf[lag * 2];
            wSum += 0.5f;
        }
        score /= wSum;

        // Smooth Log-Normal prior centered at 120 BPM (stddev ~ 0.85 octaves)
        const float candBpm = (frameRate * 60.0f) / static_cast<float>(lag);
        const float octaveDiff = std::log2(candBpm / 120.0f);
        const float prior = std::exp(-0.5f * std::pow(octaveDiff / 0.85f, 2.0f));
        score *= (0.75f + 0.25f * prior);

        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }

    // 6. Adaptive Octave Disambiguation
    float candBpm = (frameRate * 60.0f) / static_cast<float>(bestLag);
    const int doubleLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f));

    // Density-based octave correction for slow vs double time
    if (candBpm >= 135.0f && candBpm <= 145.0f && doubleLag <= maxLag) {
        if (onsetDensity < 3.5f && acf[doubleLag] > 0.70f * acf[bestLag]) {
            bestLag = doubleLag;
        }
    } else if (candBpm >= 190.0f && candBpm <= 205.0f && doubleLag <= maxLag) {
        if (acf[doubleLag] > 0.70f * acf[bestLag]) {
            bestLag = doubleLag;
        }
    }

    // Check 160 BPM / 106.6 triplet subharmonic
    candBpm = (frameRate * 60.0f) / static_cast<float>(bestLag);
    if (candBpm >= 100.0f && candBpm <= 110.0f) {
        const int tripletLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f / 3.0f));
        if (tripletLag >= minLag && acf[tripletLag] > 0.85f * acf[bestLag]) {
            bestLag = tripletLag;
        }
    }

    // 7. Parabolic interpolation with correct vertex formula
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

    // Confidence metric based on peak prominence
    float meanAcf = 0.0f;
    int count = 0;
    for (int l = minLag; l <= maxLag; ++l) {
        meanAcf += acf[l];
        ++count;
    }
    meanAcf = (count > 0) ? (meanAcf / static_cast<float>(count)) : 1.0f;
    res.confidence = std::clamp((bestScore / (meanAcf + 1e-6f) - 1.0f) / 3.0f, 0.2f, 0.99f);

    // 8. Beat onset timestamps extraction via peak picking
    const float hopSeconds = static_cast<float>(kHopLength) / static_cast<float>(kTargetSampleRate);
    const float threshold = 0.20f;
    for (size_t t = 1; t + 1 < numDiffFrames; ++t) {
        if (onset1D[t] > threshold && onset1D[t] >= onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]) {
            const float timeSec = static_cast<float>(t) * hopSeconds;
            res.beatOnsets.push_back(std::round(timeSec * 1000.0f) / 1000.0f);
        }
    }

    return res;
}

} // namespace reals::ai
