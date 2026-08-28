#include "reals/ai/MoodClassifier.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/ai/TempoDetector.h"

#include <algorithm>
#include <cmath>

namespace reals::ai {

namespace {

const std::vector<std::string> kMoodTags = {
    "action", "adventure", "advertising", "background", "ballad", "calm", "children", "christmas",
    "commercial", "corporate", "dark", "documentary", "drama", "dramatic", "dream", "emotional",
    "energetic", "epic", "fast", "film", "funny", "game", "groovy", "happy", "heavy", "holiday",
    "hopeful", "horror", "inspiring", "love", "meditative", "melancholy", "motivational", "movie",
    "nature", "optimistic", "party", "peaceful", "powerful", "positive", "quiet",
    "relaxing", "romantic", "sad", "sexy", "slow", "soft", "soundscape", "space",
    "sport", "summer", "trailer", "travel", "upbeat", "voice", "warm"
};

} // namespace

const std::vector<std::string>& MoodClassifier::getMoodTags() {
    return kMoodTags;
}

std::vector<MoodResult> MoodClassifier::classify(
    const float* pcm, size_t frames, int sampleRate, float threshold) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return {};
    }

    // 1. Resample to standard 44.1kHz mono
    constexpr int kTargetRate = 44100;
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetRate);
    if (audio.empty()) {
        return {};
    }

    // Check if ONNX model is available
    if (ModelManager::instance().isModelAvailable("mood_jamendo") &&
        ModelManager::instance().ensureModelLoaded("mood_jamendo")) {
        SpectrogramConfig cfg;
        cfg.sampleRate = kTargetRate;
        cfg.nFft = 2048;
        cfg.hopLength = 512;
        cfg.nMels = 96;
        auto logMel = FeatureExtractor::computeLogMel(audio, cfg);

        if (!logMel.empty()) {
            std::vector<float> inputFlat;
            inputFlat.reserve(logMel.size() * 96);
            for (const auto& row : logMel) {
                inputFlat.insert(inputFlat.end(), row.begin(), row.end());
            }

            std::vector<float> outputProbs;
            const std::vector<int64_t> shape = {1, static_cast<int64_t>(logMel.size()), 96};
            if (OnnxEngine::instance().runSingle("mood_jamendo", inputFlat, shape, outputProbs) &&
                outputProbs.size() >= kMoodTags.size()) {
                std::vector<MoodResult> results;
                for (size_t i = 0; i < kMoodTags.size(); ++i) {
                    if (outputProbs[i] >= threshold) {
                        results.push_back({kMoodTags[i], outputProbs[i]});
                    }
                }
                std::sort(results.begin(), results.end(), [](const MoodResult& a, const MoodResult& b) {
                    return a.score > b.score;
                });
                return results;
            }
        }
    }

    // 2. Algorithmic multi-label mood acoustic scoring fallback
    auto metrics = FeatureExtractor::computeMetrics(audio, kTargetRate);
    auto tempoRes = TempoDetector::detectAlgorithmic(pcm, frames, sampleRate);
    auto keyRes = KeyDetector::detect(pcm, frames, sampleRate);

    const float bpm = tempoRes.bpm;
    const bool isMajor = (keyRes.mode == "Major");

    // Valence (-1.0 sad/dark to +1.0 happy/bright)
    float valence = isMajor ? 0.35f : -0.35f;
    valence += (metrics.spectralCentroid > 2500.0f) ? 0.25f : -0.25f;

    // Arousal (0.0 calm/quiet to +1.0 energetic/fast)
    float arousal = std::clamp((bpm - 60.0f) / 120.0f, 0.0f, 1.0f);
    arousal = 0.5f * arousal + 0.5f * std::clamp(metrics.rms * 3.0f, 0.0f, 1.0f);

    std::vector<MoodResult> results;
    results.reserve(kMoodTags.size());

    for (const auto& tag : kMoodTags) {
        float score = 0.10f; // base score

        // High arousal + high valence
        if (tag == "happy" || tag == "positive" || tag == "upbeat" || tag == "party") {
            if (valence > 0.0f) score += 0.40f * valence;
            if (arousal > 0.5f) score += 0.35f * arousal;
        }
        // High arousal + low valence (dark, heavy, aggressive, action, energetic)
        else if (tag == "dark" || tag == "heavy" || tag == "action" || tag == "energetic" || tag == "horror" || tag == "drama") {
            if (valence < 0.0f) score += 0.40f * (-valence);
            if (arousal > 0.4f) score += 0.35f * arousal;
        }
        // Low arousal + low valence (sad, melancholy, emotional, ballad)
        else if (tag == "sad" || tag == "melancholy" || tag == "emotional" || tag == "ballad") {
            if (valence < 0.0f) score += 0.45f * (-valence);
            if (arousal < 0.5f) score += 0.35f * (1.0f - arousal);
        }
        // Low arousal + high valence (calm, relax, relaxing, peaceful, meditative, soft, dream, warm)
        else if (tag == "calm" || tag == "relax" || tag == "relaxing" || tag == "peaceful" ||
                 tag == "meditative" || tag == "soft" || tag == "dream" || tag == "warm" || tag == "quiet") {
            if (valence >= 0.0f) score += 0.35f * valence;
            if (arousal < 0.6f) score += 0.40f * (1.0f - arousal);
        }
        // Cinematic / Epic
        else if (tag == "epic" || tag == "trailer" || tag == "cinematic" || tag == "movie" || tag == "soundscape" || tag == "space") {
            if (metrics.bassRatio > 0.25f || metrics.highRatio > 0.20f) score += 0.35f;
            if (metrics.rms > 0.15f) score += 0.25f;
        }
        // Groovy / Sport / Fast
        else if (tag == "groovy" || tag == "sport" || tag == "fast") {
            if (arousal > 0.6f) score += 0.45f;
        }

        score = std::clamp(score, 0.0f, 0.95f);
        if (score >= threshold) {
            results.push_back({tag, std::round(score * 1000.0f) / 1000.0f});
        }
    }

    std::sort(results.begin(), results.end(), [](const MoodResult& a, const MoodResult& b) {
        return a.score > b.score;
    });

    return results;
}

} // namespace reals::ai
