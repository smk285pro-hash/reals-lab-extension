#include "reals/ai/TempoDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace reals::ai {

namespace {

constexpr std::string_view kTag = "TempoDetector";
constexpr std::string_view kAlgorithmicMethod = "multiband_comb_resonator_2026";

// ---------------------------------------------------------------------------
// Analysis parameters (named instead of magic numbers for easier tuning)
// ---------------------------------------------------------------------------
constexpr int kTargetSampleRate = 44100;      // Algorithmic pipeline rate
constexpr int kHopLength = 256;               // STFT hop -> ~172.27 frames/s
constexpr int kFftSize = 1024;
constexpr size_t kMaxRelevantBin = 256;       // Musically relevant 0 - 11 kHz
constexpr float kLogCompression = 100.0f;     // log1p(k * |X|); assumes normalized PCM

// CNN (TempoCNN) input
constexpr int kCnnSampleRate = 11025;
constexpr float kCnnMinBpm = 30.0f;           // Lowest output bin
constexpr float kCnnBinSpanBpm = 256.0f;      // Bins span [30, 286] BPM
constexpr float kCnnConfidenceGate = 0.4f;    // Below this -> algorithmic result

// Routing / rejection
constexpr float kMinDurationSec = 0.35f;      // Shorter -> unpitched one-shot
constexpr float kMaxAnalysisSeconds = 120.0f; // Center-crop bound for long inputs

// Onset picking thresholds (on the normalized onset envelope)
constexpr float kCountOnsetThreshold = 0.15f;
constexpr float kBeatOnsetThreshold = 0.20f;

// Unpitched percussion one-shot rejection
constexpr float kOneShotMaxDurationSec = 1.0f;
constexpr float kOneShotHeadSec = 0.25f;
constexpr float kOneShotHeadEnergyFrac = 0.92f;
constexpr float kOneShotTailSec = 0.10f;
constexpr float kOneShotTailLoudRatio = 0.25f;

// Algorithmic resonator bank
constexpr float kAlgorithmMinBpm = 48.0f;
constexpr float kAlgorithmMaxBpm = 225.0f;
constexpr float kPriorCenterBpm = 120.0f;
constexpr float kPriorStdOctaves = 0.85f;
constexpr float kPriorFloor = 0.75f;          // score *= floor + strength * prior
constexpr float kPriorStrength = 0.25f;

// Octave / meter disambiguation heuristics
constexpr float kSlowTimeMinBpm = 135.0f;
constexpr float kSlowTimeMaxBpm = 145.0f;
constexpr float kFastTimeMinBpm = 190.0f;
constexpr float kFastTimeMaxBpm = 205.0f;
constexpr float kTripletMinBpm = 100.0f;
constexpr float kTripletMaxBpm = 110.0f;
constexpr float kOnsetDensityGate = 3.5f;     // onsets per second
constexpr float kDoubleLagAcfRatio = 0.70f;
constexpr float kTripletAcfRatio = 0.85f;

// Musical octave bounds
constexpr float kMinMusicalBpm = 45.0f;
constexpr float kMaxMusicalBpm = 230.0f;
constexpr float kDefaultBpm = 120.0f;

// Disambiguate tempo octaves preferring the musical range (45 - 230 BPM).
float disambiguateBpm(float bpm) {
    if (bpm <= 0.0f || !std::isfinite(bpm)) return kDefaultBpm;
    while (bpm < kMinMusicalBpm) bpm *= 2.0f;
    while (bpm > kMaxMusicalBpm) bpm /= 2.0f;
    return bpm;
}

// 2D positive log-flux onset envelope shared by both detection paths.
struct OnsetEnvelope {
    std::vector<std::vector<float>> diff;  // [T-1][K-1] log spectral flux
    std::vector<float> onset1D;            // [T-1] normalized onset strength
    size_t numDiffFrames = 0;
    size_t numDiffBins = 0;
    bool valid = false;
};

// Precomputed analysis state shared by the CNN and algorithmic paths so that
// resampling, STFT and onset extraction each happen exactly once per detect().
struct TempoContext {
    std::vector<float> audio;  // mono @ kTargetSampleRate, center-cropped
    float durationSec = 0.0f;
    size_t onsetCount = 0;
    OnsetEnvelope env;
    bool valid = false;
};

TempoResult emptyAlgorithmicResult() {
    TempoResult res;
    res.bpm = 0.0f;
    res.confidence = 0.0f;
    res.method = kAlgorithmicMethod;
    return res;
}

OnsetEnvelope computeOnsetEnvelope(const std::vector<float>& audio) {
    OnsetEnvelope env;

    auto stft = FeatureExtractor::computeStftMagnitude(audio, kFftSize, kHopLength);
    if (stft.size() < 10) return env;

    const size_t numFrames = stft.size();
    const size_t kMaxBin = std::min<size_t>(kMaxRelevantBin, stft[0].size());

    env.numDiffFrames = numFrames - 1;
    env.numDiffBins = (kMaxBin > 0) ? (kMaxBin - 1) : 0;
    if (env.numDiffFrames == 0 || env.numDiffBins == 0) return env;

    env.diff.assign(env.numDiffFrames, std::vector<float>(env.numDiffBins, 0.0f));
    env.onset1D.assign(env.numDiffFrames, 0.0f);

    float max1D = 0.0f;
    for (size_t t = 1; t < numFrames; ++t) {
        float frameSum = 0.0f;
        for (size_t k = 1; k < kMaxBin; ++k) {
            const float logCurr = std::log1p(kLogCompression * stft[t][k]);
            const float logPrev = std::log1p(kLogCompression * stft[t - 1][k]);
            const float d = logCurr - logPrev;
            if (d > 0.0f) {
                env.diff[t - 1][k - 1] = d;
                frameSum += d;
            }
        }
        env.onset1D[t - 1] = frameSum;
        max1D = std::max(max1D, frameSum);
    }

    if (max1D > 1e-6f) {
        for (float& v : env.onset1D) v /= max1D;
    }

    env.valid = true;
    return env;
}

size_t countOnsets(const OnsetEnvelope& env) {
    size_t onsetCount = 0;
    const auto& o = env.onset1D;
    for (size_t t = 1; t + 1 < env.numDiffFrames; ++t) {
        if (o[t] > kCountOnsetThreshold && o[t] >= o[t - 1] && o[t] >= o[t + 1]) {
            ++onsetCount;
        }
    }
    return onsetCount;
}

std::vector<float> extractBeatOnsets(const OnsetEnvelope& env) {
    std::vector<float> onsets;
    const float hopSeconds = static_cast<float>(kHopLength) / static_cast<float>(kTargetSampleRate);
    const auto& o = env.onset1D;
    for (size_t t = 1; t + 1 < env.numDiffFrames; ++t) {
        if (o[t] > kBeatOnsetThreshold && o[t] >= o[t - 1] && o[t] >= o[t + 1]) {
            // onset1D[t] is the flux between STFT frames t and t + 1, so the
            // physical attack sits at frame t + 1 (fixes the one-hop offset).
            const float timeSec = static_cast<float>(t + 1) * hopSeconds;
            onsets.push_back(std::round(timeSec * 1000.0f) / 1000.0f);
        }
    }
    return onsets;
}

// Unpitched percussion one-shot detector: discards kicks, snares, claps, hats
// and short impacts from false tempo assignment.
bool isLikelyOneShot(const std::vector<float>& audio, int sampleRate, size_t numOnsets, float durationSec) {
    if (durationSec < kMinDurationSec) return true;

    // Periodic material with several attacks is a loop/groove, never a single hit.
    if (numOnsets >= 3) return false;

    if (durationSec < kOneShotMaxDurationSec) {
        if (numOnsets <= 1) return true;

        // Exactly two onsets in < 1 s is ambiguous: a one-shot with a secondary
        // transient, or a very short loop cut while still sounding. A cut loop
        // is loud at its end; a one-shot has decayed towards silence.
        const size_t tailFrames = std::min<size_t>(
            audio.size(),
            static_cast<size_t>(kOneShotTailSec * static_cast<float>(sampleRate)));
        if (tailFrames > 0 && !audio.empty()) {
            float tailMeanSq = 0.0f;
            for (size_t i = audio.size() - tailFrames; i < audio.size(); ++i) {
                tailMeanSq += audio[i] * audio[i];
            }
            tailMeanSq /= static_cast<float>(tailFrames);

            float overallMeanSq = 0.0f;
            for (const float s : audio) {
                overallMeanSq += s * s;
            }
            overallMeanSq /= static_cast<float>(audio.size());

            if (overallMeanSq > 1e-9f && (tailMeanSq / overallMeanSq) >= kOneShotTailLoudRatio) {
                return false;  // still sounding at the cut -> likely a short loop
            }
        }
        return true;
    }

    // One-shot if > 92% of the total energy sits in the first 250 ms.
    const size_t headFrames =
        static_cast<size_t>(kOneShotHeadSec * static_cast<float>(sampleRate));
    if (audio.size() > headFrames) {
        float totalEnergy = 0.0f;
        float headEnergy = 0.0f;
        for (size_t i = 0; i < audio.size(); ++i) {
            const float e = audio[i] * audio[i];
            totalEnergy += e;
            if (i < headFrames) headEnergy += e;
        }
        if (totalEnergy > 1e-6f && (headEnergy / totalEnergy) > kOneShotHeadEnergyFrac) {
            return true;
        }
    }
    return false;
}

// Resample + onset envelope + onset statistics, computed once and shared
// between the CNN and algorithmic paths (fixes the double-compute bug).
TempoContext buildContext(const float* pcm, size_t frames, int sampleRate) {
    TempoContext ctx;
    if (!pcm || frames == 0 || sampleRate <= 0) return ctx;

    // Center-crop overly long inputs: tempo is assumed stationary, and this
    // bounds the O(T * L * K) autocorrelation cost and the memory footprint.
    const float srcDurationSec = static_cast<float>(frames) / static_cast<float>(sampleRate);
    const float* src = pcm;
    size_t srcFrames = frames;
    if (srcDurationSec > kMaxAnalysisSeconds) {
        const size_t windowFrames = static_cast<size_t>(
            kMaxAnalysisSeconds * static_cast<float>(sampleRate));
        src += (frames - windowFrames) / 2;
        srcFrames = windowFrames;
    }

    ctx.audio = FeatureExtractor::resampleMono(src, srcFrames, 1, sampleRate, kTargetSampleRate);
    ctx.durationSec = static_cast<float>(ctx.audio.size()) / static_cast<float>(kTargetSampleRate);
    if (ctx.durationSec < kMinDurationSec) return ctx;

    ctx.env = computeOnsetEnvelope(ctx.audio);
    if (!ctx.env.valid) return ctx;

    ctx.onsetCount = countOnsets(ctx.env);
    ctx.valid = true;
    return ctx;
}

// CNN (TempoCNN) inference on a prepared context. Returns nullopt on any
// failure; the caller decides how to fall back. Note: this function never
// runs the algorithmic pipeline itself.
std::optional<TempoResult> runCnnInference(const TempoContext& ctx) {
    if (!ModelManager::instance().ensureModelLoaded("tempo_cnn")) {
        return std::nullopt;
    }

    // Step 1: Resample the shared 44.1 kHz buffer down to TempoCNN's rate.
    auto monoAudio = FeatureExtractor::resampleMono(
        ctx.audio.data(), ctx.audio.size(), 1, kTargetSampleRate, kCnnSampleRate);
    if (monoAudio.size() < static_cast<size_t>(kCnnSampleRate)) {
        return std::nullopt;  // TempoCNN needs at least ~1 s of audio
    }

    // Step 2: Log-Mel spectrogram
    SpectrogramConfig cfg;
    cfg.sampleRate = kCnnSampleRate;
    cfg.nFft = 1024;
    cfg.hopLength = 256;
    cfg.nMels = 40;
    cfg.fMin = 20.0f;
    cfg.fMax = 5000.0f;

    auto logMel = FeatureExtractor::computeLogMel(monoAudio, cfg);
    if (logMel.empty()) return std::nullopt;

    // Flatten input tensor: [1, 1, time_frames, 40]
    std::vector<float> inputData;
    inputData.reserve(logMel.size() * 40);
    for (const auto& frame : logMel) {
        inputData.insert(inputData.end(), frame.begin(), frame.end());
    }

    const int64_t logMelFrames = static_cast<int64_t>(logMel.size());
    std::vector<float> outputProbs;
    const std::vector<int64_t> shape = {1, 1, logMelFrames, 40};
    // NOTE: verify against the exported ONNX that (a) the layout is [N, C, T, M]
    // and (b) the graph ends with softmax so outputProbs are probabilities.
    if (!OnnxEngine::instance().runSingle("tempo_cnn", inputData, shape, outputProbs) ||
        outputProbs.empty()) {
        return std::nullopt;
    }

    // Peak probability bin, evaluated at the bin CENTER (bins span 30 - 286 BPM).
    const auto maxIt = std::max_element(outputProbs.begin(), outputProbs.end());
    const size_t peakIdx = static_cast<size_t>(std::distance(outputProbs.begin(), maxIt));
    const float binWidthBpm = kCnnBinSpanBpm / static_cast<float>(outputProbs.size());
    const float rawBpm = kCnnMinBpm + (static_cast<float>(peakIdx) + 0.5f) * binWidthBpm;
    const float confidence = std::clamp(*maxIt, 0.0f, 1.0f);

    TempoResult res;
    res.bpm = std::round(disambiguateBpm(rawBpm) * 10.0f) / 10.0f;
    res.confidence = confidence;
    res.beatOnsets = extractBeatOnsets(ctx.env);  // cheap: no ACF involved
    res.method = "tempo_cnn";

    LOG_DEBUG(kTag, "TempoCNN detected BPM successfully");
    return res;
}

// Comb-resonator tempo estimate over the prepared context (the onset envelope
// and onset count are already inside ctx and are NOT recomputed here).
TempoResult runAlgorithmic(const TempoContext& ctx) {
    TempoResult res;
    res.method = kAlgorithmicMethod;

    const OnsetEnvelope& env = ctx.env;
    const size_t numDiffFrames = env.numDiffFrames;
    const size_t numDiffBins = env.numDiffBins;

    const float onsetDensity =
        static_cast<float>(ctx.onsetCount) / std::max(0.1f, ctx.durationSec);

    // 1. 2D Spectral Autocorrelation over BPM range 48 - 225
    const float frameRate =
        static_cast<float>(kTargetSampleRate) / static_cast<float>(kHopLength);  // ~172.27 Hz
    const int minLag =
        std::max(1, static_cast<int>(std::round(frameRate * 60.0f / kAlgorithmMaxBpm)));
    const int maxLag = std::min(static_cast<int>(numDiffFrames) - 1,
                                static_cast<int>(std::round(frameRate * 60.0f / kAlgorithmMinBpm)));
    if (maxLag <= minLag) {
        res.bpm = kDefaultBpm;
        res.confidence = 0.2f;
        return res;
    }

    const int fullMaxLag = std::min(static_cast<int>(numDiffFrames) - 1, 2 * maxLag + 10);
    std::vector<float> acf(static_cast<size_t>(fullMaxLag) + 1, 0.0f);
    for (int lag = 1; lag <= fullMaxLag; ++lag) {
        float sum = 0.0f;
        for (size_t t = 0; t + lag < numDiffFrames; ++t) {
            const auto& rowA = env.diff[t];
            const auto& rowB = env.diff[t + lag];
            for (size_t k = 0; k < numDiffBins; ++k) {
                sum += rowA[k] * rowB[k];
            }
        }
        // Biased sample autocorrelation estimator (scaled by signal length for level invariance)
        acf[lag] = sum / static_cast<float>(numDiffFrames);
    }

    // 2. Comb filter resonator bank scoring
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

        // Smooth log-normal prior centered at 120 BPM (stddev ~ 0.85 octaves)
        const float candBpm = (frameRate * 60.0f) / static_cast<float>(lag);
        const float octaveDiff = std::log2(candBpm / kPriorCenterBpm);
        const float prior = std::exp(-0.5f * std::pow(octaveDiff / kPriorStdOctaves, 2.0f));
        score *= (kPriorFloor + kPriorStrength * prior);

        if (score > bestScore) {
            bestScore = score;
            bestLag = lag;
        }
    }

    // 3. Adaptive octave disambiguation
    float candBpm = (frameRate * 60.0f) / static_cast<float>(bestLag);
    const int doubleLag = static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f));

    // Density-based octave correction for slow vs double time
    if (candBpm >= kSlowTimeMinBpm && candBpm <= kSlowTimeMaxBpm && doubleLag <= maxLag) {
        if (onsetDensity < kOnsetDensityGate && acf[doubleLag] > kDoubleLagAcfRatio * acf[bestLag]) {
            bestLag = doubleLag;
        }
    } else if (candBpm >= kFastTimeMinBpm && candBpm <= kFastTimeMaxBpm && doubleLag <= maxLag) {
        if (acf[doubleLag] > kDoubleLagAcfRatio * acf[bestLag]) {
            bestLag = doubleLag;
        }
    }

    // Triplet-feel subharmonic check (~106.6 BPM -> 160 BPM)
    candBpm = (frameRate * 60.0f) / static_cast<float>(bestLag);
    if (candBpm >= kTripletMinBpm && candBpm <= kTripletMaxBpm) {
        const int tripletLag =
            static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f / 3.0f));
        if (tripletLag >= minLag && acf[tripletLag] > kTripletAcfRatio * acf[bestLag]) {
            bestLag = tripletLag;
        }
    } else if (candBpm >= 64.0f && candBpm <= 68.0f) {
        const int tripletLag =
            static_cast<int>(std::round(static_cast<float>(bestLag) * 2.0f / 3.0f));
        if (tripletLag >= minLag && acf[tripletLag] >= 0.95f * acf[bestLag]) {
            bestLag = tripletLag;
        }
    }

    // 4. Parabolic interpolation with the correct vertex formula
    float exactLag = static_cast<float>(bestLag);
    if (bestLag > minLag && bestLag < maxLag) {
        const float alpha = acf[bestLag - 1];
        const float beta = acf[bestLag];
        const float gamma = acf[bestLag + 1];
        const float denom = alpha - 2.0f * beta + gamma;  // negative at a peak
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

    // 5. Confidence metric based on peak prominence
    float meanAcf = 0.0f;
    int count = 0;
    for (int l = minLag; l <= maxLag; ++l) {
        meanAcf += acf[l];
        ++count;
    }
    meanAcf = (count > 0) ? (meanAcf / static_cast<float>(count)) : 1.0f;
    res.confidence = std::clamp((bestScore / (meanAcf + 1e-6f) - 1.0f) / 3.0f, 0.2f, 0.99f);

    // 6. Beat onset timestamps from the shared envelope
    res.beatOnsets = extractBeatOnsets(env);

    return res;
}

// Full algorithmic pipeline over a prebuilt context, preserving the original
// detectAlgorithmic() semantics (too-short / one-shot rejection included).
TempoResult detectAlgorithmicCore(const TempoContext& ctx) {
    if (!ctx.valid) return emptyAlgorithmicResult();
    if (isLikelyOneShot(ctx.audio, kTargetSampleRate, ctx.onsetCount, ctx.durationSec)) {
        return emptyAlgorithmicResult();
    }
    return runAlgorithmic(ctx);
}

}  // namespace

TempoResult TempoDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return TempoResult{0.0f, 0.0f, {}, "invalid_input"};
    }

    // Build the shared analysis state once; every stage below reuses it.
    TempoContext ctx = buildContext(pcm, frames, sampleRate);
    if (!ctx.valid) {
        // Too short (< 0.35 s) or spectrally degenerate -> unpitched one-shot.
        return emptyAlgorithmicResult();
    }

    // Reject unpitched percussion one-shots BEFORE spending CNN/ACF budget.
    if (isLikelyOneShot(ctx.audio, kTargetSampleRate, ctx.onsetCount, ctx.durationSec)) {
        return emptyAlgorithmicResult();
    }

    // CNN attempt: each expensive stage now runs at most once per detect().
    if (ModelManager::instance().isModelAvailable("tempo_cnn")) {
        if (auto cnn = runCnnInference(ctx)) {
            if (cnn->confidence > kCnnConfidenceGate) {
                return *cnn;
            }
        }
    }

    return runAlgorithmic(ctx);
}

TempoResult TempoDetector::detectCnn(const float* pcm, size_t frames, int sampleRate) {
    TempoContext ctx = buildContext(pcm, frames, sampleRate);
    if (!ctx.valid) return detectAlgorithmicCore(ctx);

    if (auto cnn = runCnnInference(ctx)) {
        return *cnn;
    }
    return detectAlgorithmicCore(ctx);
}

TempoResult TempoDetector::detectAlgorithmic(const float* pcm, size_t frames, int sampleRate) {
    return detectAlgorithmicCore(buildContext(pcm, frames, sampleRate));
}

}  // namespace reals::ai
