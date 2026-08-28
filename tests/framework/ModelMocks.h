#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include <reals/ai/FeatureExtractor.h>
#include <reals/util/Hash.h>

namespace reals::test {

struct TempoResult {
    float bpm = 0.0f;
    float confidence = 0.0f;
    std::vector<float> onsetsSec;
    std::string algorithm = "TempoCNN";
};

struct KeyResult {
    std::string keyName;
    std::string camelot;
    std::string openKey;
    float confidence = 0.0f;
    std::string mode; // "Major" or "Minor"
};

struct GenrePrediction {
    std::string genre;
    float confidence = 0.0f;
};

struct MoodPrediction {
    std::string mood;
    float confidence = 0.0f;
};

class ModelMocks {
public:
    static constexpr double kPi = 3.14159265358979323846;

    // TempoCNN & Onsets Detector
    static TempoResult detectTempo(const float* pcm, size_t frames, int sampleRate) {
        TempoResult res;
        if (!pcm || frames < 1024 || sampleRate <= 0) {
            return res;
        }

        std::vector<float> mono(pcm, pcm + frames);
        auto onsetEnv = reals::ai::FeatureExtractor::computeOnsetEnvelope(mono, sampleRate, 1024, 512);

        // Autocorrelation over onset novelty curve
        if (onsetEnv.size() >= 16) {
            const double hopSec = 512.0 / sampleRate;
            const size_t minLag = std::max(size_t(1), static_cast<size_t>(60.0 / (240.0 * hopSec))); // Max 240 BPM
            const size_t maxLag = std::min(onsetEnv.size() / 2, static_cast<size_t>(60.0 / (45.0 * hopSec)));  // Min 45 BPM

            std::vector<float> r(maxLag, 0.0f);
            float maxCorr = 0.0f;

            for (size_t lag = minLag; lag < maxLag; ++lag) {
                float num = 0.0f;
                float den0 = 0.0f;
                float denLag = 0.0f;
                for (size_t i = 0; i < onsetEnv.size() / 2; ++i) {
                    float o0 = onsetEnv[i];
                    float oLag = onsetEnv[i + lag];
                    num += o0 * oLag;
                    den0 += o0 * o0;
                    denLag += oLag * oLag;
                }
                float den = std::sqrt(den0) * std::sqrt(denLag);
                if (den > 1e-7f) {
                    r[lag] = num / den;
                    if (r[lag] > maxCorr) {
                        maxCorr = r[lag];
                    }
                }
            }

            if (maxCorr > 0.15f) {
                // Find first prominent peak to avoid multi-beat intervals (e.g. 58 BPM instead of 174 BPM)
                size_t bestLag = 0;
                const float thresh = std::max(0.35f, maxCorr * 0.70f);
                for (size_t lag = minLag + 1; lag < maxLag - 1; ++lag) {
                    if (r[lag] >= thresh && r[lag] >= r[lag - 1] && r[lag] >= r[lag + 1]) {
                        bestLag = lag;
                        break;
                    }
                }
                if (bestLag == 0) {
                    for (size_t lag = minLag; lag < maxLag; ++lag) {
                        if (r[lag] == maxCorr) {
                            bestLag = lag;
                            break;
                        }
                    }
                }

                if (bestLag > 0) {
                    float alpha = (bestLag > 0) ? r[bestLag - 1] : r[bestLag];
                    float beta = r[bestLag];
                    float gamma = (bestLag + 1 < maxLag) ? r[bestLag + 1] : r[bestLag];
                    float delta = 0.0f;
                    float denom = (alpha - 2.0f * beta + gamma);
                    if (std::abs(denom) > 1e-7f) {
                        delta = 0.5f * (alpha - gamma) / denom;
                    }
                    double refinedLag = static_cast<double>(bestLag) + std::clamp(delta, -0.5f, 0.5f);
                    double beatSec = refinedLag * hopSec;
                    res.bpm = static_cast<float>(std::round((60.0 / beatSec) * 10.0) / 10.0);
                    res.confidence = std::clamp(maxCorr, 0.72f, 0.98f);
                    res.algorithm = "TempoCNN";

                    // Generate onset timestamps
                    for (size_t i = 0; i < onsetEnv.size(); ++i) {
                        if (onsetEnv[i] > 0.4f) {
                            res.onsetsSec.push_back(static_cast<float>(i * hopSec));
                        }
                    }
                    return res;
                }
            }
        }

        // Fallback RhythmExtractor
        res.bpm = 120.0f;
        res.confidence = 0.60f;
        res.algorithm = "RhythmExtractor2013";
        return res;
    }

    // Essentia EDMA Key & Ensemble Voting (Temperley & Krumhansl)
    static KeyResult detectKey(const float* pcm, size_t frames, int sampleRate) {
        KeyResult res;
        if (!pcm || frames < 1024 || sampleRate <= 0) {
            res.keyName = "C Major";
            res.camelot = "8B";
            res.openKey = "1d";
            res.mode = "Major";
            res.confidence = 0.0f;
            return res;
        }

        std::vector<float> mono(pcm, pcm + frames);
        auto chroma = reals::ai::FeatureExtractor::computeGlobalChroma(mono, sampleRate, 4096, 512);

        // Krumhansl Major & Minor Key Profiles
        static const float kMajorProfile[12] = {6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f};
        static const float kMinorProfile[12] = {6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f};
        static const char* kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

        static const char* kMajorCamelot[12] = {"8B", "3B", "10B", "5B", "12B", "7B", "2B", "9B", "4B", "11B", "6B", "1B"};
        static const char* kMinorCamelot[12] = {"5A", "12A", "7A", "2A", "9A", "4A", "11A", "6A", "1A", "8A", "3A", "10A"};

        static const char* kMajorOpenKey[12] = {"1d", "8d", "3d", "10d", "5d", "12d", "7d", "2d", "9d", "4d", "11d", "6d"};
        static const char* kMinorOpenKey[12] = {"10m", "5m", "12m", "7m", "2m", "9m", "4m", "11m", "6m", "1m", "8m", "3m"};

        float chromaMean = 0.0f;
        float maxChroma = 0.0f;
        for (int i = 0; i < 12; ++i) {
            chromaMean += chroma[i];
            maxChroma = std::max(maxChroma, chroma[i]);
        }
        chromaMean /= 12.0f;

        float majMean = 0.0f;
        float minMean = 0.0f;
        for (int i = 0; i < 12; ++i) {
            majMean += kMajorProfile[i];
            minMean += kMinorProfile[i];
        }
        majMean /= 12.0f;
        minMean /= 12.0f;

        float bestScore = -1e9f;
        int bestRoot = 0;
        bool isMinor = false;

        // Zero-mean Pearson correlation across 12 chromatic shifts with tonic gating
        for (int r = 0; r < 12; ++r) {
            if (maxChroma > 0.05f && chroma[r] < 0.25f * maxChroma) {
                continue;
            }

            float numMaj = 0.0f, denMajC = 0.0f, denMajP = 0.0f;
            float numMin = 0.0f, denMinC = 0.0f, denMinP = 0.0f;
            for (int i = 0; i < 12; ++i) {
                int chIdx = (r + i) % 12;
                float cDiff = chroma[chIdx] - chromaMean;
                float pMajDiff = kMajorProfile[i] - majMean;
                float pMinDiff = kMinorProfile[i] - minMean;

                numMaj += cDiff * pMajDiff;
                denMajC += cDiff * cDiff;
                denMajP += pMajDiff * pMajDiff;

                numMin += cDiff * pMinDiff;
                denMinC += cDiff * cDiff;
                denMinP += pMinDiff * pMinDiff;
            }
            float rMaj = (denMajC > 1e-7f && denMajP > 1e-7f) ? (numMaj / (std::sqrt(denMajC) * std::sqrt(denMajP))) : -1.0f;
            float rMin = (denMinC > 1e-7f && denMinP > 1e-7f) ? (numMin / (std::sqrt(denMinC) * std::sqrt(denMinP))) : -1.0f;

            // EDMA harmonic triad alignment boost
            bool hasFifth = chroma[(r + 7) % 12] > 0.25f * maxChroma;
            bool hasMaj3rd = chroma[(r + 4) % 12] > 0.25f * maxChroma;
            bool hasMin3rd = chroma[(r + 3) % 12] > 0.25f * maxChroma;

            if (hasMaj3rd && hasFifth) rMaj += 0.35f;
            if (hasMin3rd && hasFifth) rMin += 0.35f;

            if (rMaj > bestScore) {
                bestScore = rMaj;
                bestRoot = r;
                isMinor = false;
            }
            if (rMin > bestScore) {
                bestScore = rMin;
                bestRoot = r;
                isMinor = true;
            }
        }

        res.mode = isMinor ? "Minor" : "Major";
        res.keyName = std::string(kNoteNames[bestRoot]) + " " + res.mode;
        res.camelot = isMinor ? kMinorCamelot[bestRoot] : kMajorCamelot[bestRoot];
        res.openKey = isMinor ? kMinorOpenKey[bestRoot] : kMajorOpenKey[bestRoot];
        res.confidence = 0.92f;
        return res;
    }

    // Discogs-MAEST 400 Subgenres Classifier
    static std::vector<GenrePrediction> classifyGenres(const float* pcm, size_t frames, int sampleRate) {
        std::vector<float> mono(pcm, pcm + frames);
        auto metrics = reals::ai::FeatureExtractor::computeMetrics(mono, sampleRate);

        std::vector<GenrePrediction> preds;
        if (metrics.bassRatio > 0.40f) {
            preds.push_back({"Trap-EDM", 0.42f});
            preds.push_back({"Dubstep", 0.28f});
            preds.push_back({"Hip Hop", 0.15f});
            preds.push_back({"Future Bass", 0.10f});
            preds.push_back({"Drum & Bass", 0.05f});
        } else if (metrics.spectralCentroid > 3000.0f) {
            preds.push_back({"Synthwave", 0.38f});
            preds.push_back({"Tech House", 0.29f});
            preds.push_back({"Minimal Techno", 0.18f});
            preds.push_back({"Electro Pop", 0.10f});
            preds.push_back({"Deep House", 0.05f});
        } else {
            preds.push_back({"Lo-Fi Hip Hop", 0.35f});
            preds.push_back({"Ambient", 0.30f});
            preds.push_back({"Acoustic Pop", 0.18f});
            preds.push_back({"Neo Soul", 0.12f});
            preds.push_back({"Downtempo", 0.05f});
        }
        return preds;
    }

    // Mood-Jamendo Multi-Label Classifier (56 emotions)
    static std::vector<MoodPrediction> classifyMoods(const float* pcm, size_t frames, int sampleRate) {
        std::vector<float> mono(pcm, pcm + frames);
        auto metrics = reals::ai::FeatureExtractor::computeMetrics(mono, sampleRate);

        std::vector<MoodPrediction> moods;
        const float crestFactor = (metrics.rms > 1e-5f) ? (metrics.peak / metrics.rms) : 0.0f;
        const bool isAggressiveOrRhythmic = (crestFactor > 2.0f && metrics.peak > 0.4f) ||
                                            (metrics.rms > 0.35f && metrics.bassRatio > 0.35f);

        if (isAggressiveOrRhythmic) {
            moods.push_back({"dark", 0.85f});
            moods.push_back({"aggressive", 0.78f});
            moods.push_back({"energetic", 0.72f});
            moods.push_back({"heavy", 0.65f});
            moods.push_back({"punchy", 0.60f});
        } else {
            moods.push_back({"relaxed", 0.82f});
            moods.push_back({"calm", 0.76f});
            moods.push_back({"atmospheric", 0.68f});
            moods.push_back({"spacey", 0.55f});
            moods.push_back({"sad", 0.40f});
        }
        return moods;
    }

    // CLAP 512-dim Audio & Text Embedder
    static std::vector<float> embedAudio(const float* pcm, size_t frames, int sampleRate) {
        std::vector<float> vec(512, 0.0f);
        if (!pcm || frames == 0) return vec;

        std::vector<float> mono(pcm, pcm + frames);
        auto chroma = reals::ai::FeatureExtractor::computeGlobalChroma(mono, sampleRate);
        auto metrics = reals::ai::FeatureExtractor::computeMetrics(mono, sampleRate);

        // Project audio features deterministically into 512-dim unit sphere
        uint64_t h = reals::util::xxhash64(mono.data(), std::min(frames, size_t(1024)) * sizeof(float));
        std::mt19937 rng(static_cast<uint32_t>(h));
        std::normal_distribution<float> dist(0.0f, 1.0f);

        float normSq = 0.0f;
        for (size_t i = 0; i < 512; ++i) {
            float base = dist(rng);
            if (i < 12) base += chroma[i] * 2.0f;
            if (i == 12) base += metrics.bassRatio * 3.0f;
            if (i == 13) base += (metrics.spectralCentroid / 5000.0f) * 2.0f;
            vec[i] = base;
            normSq += base * base;
        }

        float invNorm = 1.0f / std::sqrt(normSq);
        for (size_t i = 0; i < 512; ++i) {
            vec[i] *= invNorm;
        }
        return vec;
    }

    static std::vector<float> embedText(const std::string& query) {
        std::vector<float> vec(512, 0.0f);
        if (query.empty()) return vec;

        uint64_t h = reals::util::xxhash64(query);
        std::mt19937 rng(static_cast<uint32_t>(h));
        std::normal_distribution<float> dist(0.0f, 1.0f);

        float normSq = 0.0f;
        for (size_t i = 0; i < 512; ++i) {
            float base = dist(rng);
            vec[i] = base;
            normSq += base * base;
        }

        float invNorm = 1.0f / std::sqrt(normSq);
        for (size_t i = 0; i < 512; ++i) {
            vec[i] *= invNorm;
        }
        return vec;
    }

    // SIMD Cosine Similarity calculation with scalar fallback
    static float cosineSimilarity(const float* a, const float* b, size_t dim = 512) {
        if (!a || !b || dim == 0) return 0.0f;

        float dot = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;

        size_t i = 0;
        // Loop unrolling for 8 floats at a time
        for (; i + 7 < dim; i += 8) {
            dot += a[i+0]*b[i+0] + a[i+1]*b[i+1] + a[i+2]*b[i+2] + a[i+3]*b[i+3] +
                   a[i+4]*b[i+4] + a[i+5]*b[i+5] + a[i+6]*b[i+6] + a[i+7]*b[i+7];
            normA += a[i+0]*a[i+0] + a[i+1]*a[i+1] + a[i+2]*a[i+2] + a[i+3]*a[i+3] +
                     a[i+4]*a[i+4] + a[i+5]*a[i+5] + a[i+6]*a[i+6] + a[i+7]*a[i+7];
            normB += b[i+0]*b[i+0] + b[i+1]*b[i+1] + b[i+2]*b[i+2] + b[i+3]*b[i+3] +
                     b[i+4]*b[i+4] + b[i+5]*b[i+5] + b[i+6]*b[i+6] + b[i+7]*b[i+7];
        }

        for (; i < dim; ++i) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }

        float denom = std::sqrt(normA) * std::sqrt(normB);
        if (denom <= 1e-9f) return 0.0f;
        return std::clamp(dot / denom, -1.0f, 1.0f);
    }
};

} // namespace reals::test
