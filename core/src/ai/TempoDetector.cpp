#include "reals/ai/TempoDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace reals::ai {

namespace {
constexpr std::string_view kTag = "TempoDetector";
constexpr int kTargetSampleRate = 44100;
constexpr int kHopLength = 512;
constexpr int kFftSize = 1024;

// Disambiguate tempo octaves preferring dance/standard music range (75 - 165 BPM)
float disambiguateBpm(float bpm) {
    // Non-finite inputs would make `while (bpm > 180) bpm /= 2` loop forever
    // (+inf / 2 == +inf) — guard before touching the loops (CRIT-04).
    if (bpm <= 0.0f || !std::isfinite(bpm)) return 120.0f;
    while (bpm < 70.0f) {
        bpm *= 2.0f;
    }
    while (bpm > 180.0f) {
        bpm /= 2.0f;
    }
    return bpm;
}

} // namespace

TempoResult TempoDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return TempoResult{120.0f, 0.0f, {}, "invalid_input"};
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
    return res;
}

TempoResult TempoDetector::detectAlgorithmic(const float* pcm, size_t frames, int sampleRate) {
    TempoResult res;
    res.method = "rhythm_extractor_2013";

    if (!pcm || frames == 0 || sampleRate <= 0) {
        res.bpm = 120.0f;
        res.confidence = 0.0f;
        return res;
    }

    // 1. Resample to standard 44.1kHz mono
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetSampleRate);
    if (audio.size() < static_cast<size_t>(kTargetSampleRate / 2)) {
        // Audio too short (< 0.5s)
        res.bpm = 120.0f;
        res.confidence = 0.1f;
        return res;
    }

    // 2. Compute Onset Novelty Curve (Spectral Flux)
    auto onset = FeatureExtractor::computeOnsetEnvelope(audio, kTargetSampleRate, kFftSize, kHopLength);
    if (onset.size() < 10) {
        res.bpm = 120.0f;
        res.confidence = 0.1f;
        return res;
    }

    // 3. Autocorrelation over BPM range 40..240
    // Lag in frames = (60 * kTargetSampleRate) / (bpm * kHopLength)
    const float frameRate = static_cast<float>(kTargetSampleRate) / static_cast<float>(kHopLength); // ~86.13 Hz
    const int minLag = std::max(1, static_cast<int>(frameRate * 60.0f / 240.0f));                   // ~21 frames
    const int maxLag = std::min(static_cast<int>(onset.size()) - 1, static_cast<int>(frameRate * 60.0f / 40.0f)); // ~129 frames

    if (maxLag <= minLag) {
        res.bpm = 120.0f;
        res.confidence = 0.2f;
        return res;
    }

    std::vector<float> acf(maxLag + 1, 0.0f);
    for (int lag = minLag; lag <= maxLag; ++lag) {
        float sum = 0.0f;
        for (size_t i = 0; i + lag < onset.size(); ++i) {
            sum += onset[i] * onset[i + lag];
        }
        acf[lag] = sum;
    }

    // 4. Comb filter resonance scoring with balanced harmonics & gentle 120 BPM prior
    // CRIT-TEMPO-OCTAVE: In earlier versions, short lags (120-240 BPM) received up to +75% harmonic boost
    // while long lags (40-70 BPM) received +0%, causing systematic 2x octave doubling (e.g. 70 BPM -> 139.5 BPM).
    // Normalizing by `weightSum` and including sub-harmonics (lag / 2) with a smooth 120-BPM prior resolves octave errors.
    float bestScore = -1.0f;
    int bestLag = minLag;

    for (int lag = minLag; lag <= maxLag; ++lag) {
        float score = acf[lag];
        float weightSum = 1.0f;
        if (lag * 2 <= maxLag) { score += 0.5f * acf[lag * 2]; weightSum += 0.5f; }
        if (lag * 3 <= maxLag) { score += 0.25f * acf[lag * 3]; weightSum += 0.25f; }
        if (lag / 2 >= minLag) { score += 0.5f * acf[lag / 2]; weightSum += 0.5f; }

        score /= weightSum;

        // Gentle Log-Normal prior centered around 120 BPM (stddev ~ 0.75 octaves)
        const float candBpm = (frameRate * 60.0f) / static_cast<float>(lag);
        const float octaveDiff = std::log2(candBpm / 120.0f);
        const float prior = std::exp(-0.5f * std::pow(octaveDiff / 0.75f, 2.0f));
        score *= (0.65f + 0.35f * prior);

        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }

    // 5. Parabolic interpolation for sub-frame precision
    float exactLag = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        const float alpha = acf[bestLag - 1];
        const float beta = acf[bestLag];
        const float gamma = acf[bestLag + 1];
        const float denom = 2.0f * (2.0f * beta - alpha - gamma);
        if (std::abs(denom) > 1e-6f) {
            exactLag += (alpha - gamma) / denom;
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

    // 6. Beat onset timestamps extraction via peak picking
    const float hopSeconds = static_cast<float>(kHopLength) / static_cast<float>(kTargetSampleRate);
    float threshold = 0.25f;
    for (size_t t = 1; t + 1 < onset.size(); ++t) {
        if (onset[t] > threshold && onset[t] >= onset[t - 1] && onset[t] >= onset[t + 1]) {
            const float timeSec = static_cast<float>(t) * hopSeconds;
            res.beatOnsets.push_back(std::round(timeSec * 1000.0f) / 1000.0f);
        }
    }

    return res;
}

} // namespace reals::ai
