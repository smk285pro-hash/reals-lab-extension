#include "reals/ai/ClapEmbedder.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/ai/TempoDetector.h"
#include "reals/util/Hash.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace reals::ai {

namespace {

constexpr float kEpsilon = 1e-7f;

// Predefined semantic concept indices in the 512-dim embedding space
struct SemanticConcept {
    const char* keyword;
    int targetDim;
    float weight;
};

const SemanticConcept kConcepts[] = {
    {"dark", 10, 0.8f}, {"warm", 11, 0.7f}, {"bright", 12, 0.7f}, {"cold", 13, 0.6f},
    {"808", 20, 0.9f}, {"bass", 21, 0.8f}, {"sub", 22, 0.8f}, {"kick", 23, 0.9f},
    {"snare", 24, 0.9f}, {"hihat", 25, 0.8f}, {"clap", 26, 0.8f}, {"cymbals", 27, 0.7f},
    {"trap", 40, 0.85f}, {"drill", 41, 0.85f}, {"hip hop", 42, 0.8f}, {"lo-fi", 43, 0.9f},
    {"house", 44, 0.8f}, {"techno", 45, 0.8f}, {"ambient", 46, 0.9f}, {"cinematic", 47, 0.85f},
    {"synth", 60, 0.75f}, {"piano", 61, 0.85f}, {"guitar", 62, 0.85f}, {"acoustic", 63, 0.8f},
    {"vocal", 70, 0.85f}, {"choir", 71, 0.85f}, {"lead", 72, 0.75f}, {"pad", 73, 0.8f},
    {"aggressive", 80, 0.8f}, {"punchy", 81, 0.85f}, {"distorted", 82, 0.85f}, {"heavy", 83, 0.8f},
    {"chill", 90, 0.85f}, {"relaxing", 91, 0.85f}, {"dusty", 92, 0.8f}, {"vintage", 93, 0.8f},
    {"vinyl", 94, 0.85f}, {"reverb", 95, 0.75f}, {"delay", 96, 0.75f}, {"fast", 97, 0.7f}
};

std::string toLower(std::string_view str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

} // namespace

void ClapEmbedder::normalize(std::vector<float>& vec) {
    float sumSq = 0.0f;
    for (float v : vec) {
        sumSq += v * v;
    }
    const float norm = std::sqrt(sumSq);
    if (norm > kEpsilon) {
        const float invNorm = 1.0f / norm;
        for (float& v : vec) {
            v *= invNorm;
        }
    } else {
        // Fallback default unit vector
        if (!vec.empty()) {
            vec[0] = 1.0f;
        }
    }
}

float ClapEmbedder::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != kEmbeddingDim || b.size() != kEmbeddingDim) {
        return 0.0f;
    }
    float dot = 0.0f;
    for (size_t i = 0; i < kEmbeddingDim; ++i) {
        dot += a[i] * b[i];
    }
    return std::clamp(dot, -1.0f, 1.0f);
}

std::vector<float> ClapEmbedder::embedAudio(const float* pcm, size_t frames, int sampleRate) {
    std::vector<float> embedding(kEmbeddingDim, 0.0f);
    if (!pcm || frames == 0 || sampleRate <= 0) {
        embedding[0] = 1.0f;
        return embedding;
    }

    // 1. Resample to standard 48kHz mono
    constexpr int kTargetRate = 48000;
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetRate);
    if (audio.empty()) {
        embedding[0] = 1.0f;
        return embedding;
    }

    // Check if ONNX model is available
    if (ModelManager::instance().isModelAvailable("clap_audio") &&
        ModelManager::instance().ensureModelLoaded("clap_audio")) {
        std::vector<float> outputData;
        const std::vector<int64_t> shape = {1, static_cast<int64_t>(audio.size())};
        if (OnnxEngine::instance().runSingle("clap_audio", audio, shape, outputData) &&
            outputData.size() == kEmbeddingDim) {
            normalize(outputData);
            return outputData;
        }
    }

    // 2. Multi-feature acoustic semantic projection
    auto metrics = FeatureExtractor::computeMetrics(audio, kTargetRate);
    auto tempoRes = TempoDetector::detectAlgorithmic(pcm, frames, sampleRate);
    auto chroma = FeatureExtractor::computeGlobalChroma(audio, kTargetRate, 2048, 512);

    SpectrogramConfig cfg;
    cfg.sampleRate = kTargetRate;
    cfg.nFft = 2048;
    cfg.hopLength = 1024;
    cfg.nMels = 64;
    auto logMel = FeatureExtractor::computeLogMel(audio, cfg);

    // Dims 0..63: Average Mel band energy profile (mean-centered to capture spectral shape/timbre without DC loudness bias)
    if (!logMel.empty()) {
        std::vector<float> avgMel(64, 0.0f);
        for (const auto& frame : logMel) {
            for (int m = 0; m < 64 && m < static_cast<int>(frame.size()); ++m) {
                avgMel[m] += frame[m];
            }
        }
        float melSum = 0.0f;
        for (int m = 0; m < 64; ++m) {
            avgMel[m] /= static_cast<float>(logMel.size());
            melSum += avgMel[m];
        }
        const float melMean = melSum / 64.0f;
        for (int m = 0; m < 64; ++m) {
            embedding[m] = avgMel[m] - melMean;
        }
    }

    // Dims 64..75: 12-dimensional pitch chroma
    for (int c = 0; c < 12 && c < static_cast<int>(chroma.size()); ++c) {
        embedding[64 + c] = chroma[c];
    }

    // Dims 76..99: Dynamic and spectral descriptors
    embedding[76] = metrics.rms * 2.0f;
    embedding[77] = metrics.peak;
    embedding[78] = metrics.spectralCentroid / 5000.0f;
    embedding[79] = metrics.spectralRolloff / 8000.0f;
    embedding[80] = metrics.zeroCrossingRate * 10.0f;
    embedding[81] = metrics.bassRatio * 3.0f;
    embedding[82] = metrics.highRatio * 3.0f;
    embedding[83] = tempoRes.bpm / 200.0f;
    embedding[84] = tempoRes.confidence;

    // Cross-modality alignment with semantic concept dimensions
    // Low centroid + high bass = dark / warm / sub / 808
    if (metrics.bassRatio > 0.3f) {
        embedding[10] += 0.8f; // dark
        embedding[11] += 0.6f; // warm
        embedding[20] += 0.9f; // 808
        embedding[21] += 0.8f; // bass
        embedding[22] += 0.9f; // sub
    }
    // High centroid + high ZCR = bright / crunchy / distorted
    if (metrics.spectralCentroid > 2500.0f) {
        embedding[12] += 0.7f; // bright
    }
    if (metrics.zeroCrossingRate > 0.08f) {
        embedding[82] += 0.8f; // distorted / aggressive
    }
    // Moderate tempo + low centroid = lo-fi / chill / dusty
    if (tempoRes.bpm >= 70.0f && tempoRes.bpm <= 95.0f && metrics.spectralCentroid < 2200.0f) {
        embedding[43] += 0.85f; // lo-fi
        embedding[90] += 0.8f;  // chill
        embedding[92] += 0.7f;  // dusty
        embedding[94] += 0.75f; // vinyl
    }

    // Fill remaining high-dimensional representations via Type-II orthogonal DCT spectral expansion
    constexpr float kPi = 3.14159265358979323846f;
    for (size_t d = 100; d < kEmbeddingDim; ++d) {
        float val = 0.0f;
        const float k = static_cast<float>(d - 100 + 1);
        for (int m = 0; m < 64; ++m) {
            val += embedding[m] * std::cos((kPi / 64.0f) * (static_cast<float>(m) + 0.5f) * k);
        }
        embedding[d] = val * 0.1f;
    }

    normalize(embedding);
    return embedding;
}

std::vector<float> ClapEmbedder::embedText(const std::string& text) {
    std::vector<float> embedding(kEmbeddingDim, 0.0f);
    if (text.empty()) {
        embedding[0] = 1.0f;
        return embedding;
    }

    // Check if ONNX model is available
    if (ModelManager::instance().isModelAvailable("clap_text") &&
        ModelManager::instance().ensureModelLoaded("clap_text")) {
        // Tokenize text into integers
        std::vector<float> tokens(77, 0.0f);
        std::istringstream iss(text);
        std::string word;
        int idx = 0;
        while (iss >> word && idx < 77) {
            tokens[idx++] = static_cast<float>(util::xxhash64(word) % 49408);
        }
        std::vector<float> outputData;
        const std::vector<int64_t> shape = {1, 77};
        if (OnnxEngine::instance().runSingle("clap_text", tokens, shape, outputData) &&
            outputData.size() == kEmbeddingDim) {
            normalize(outputData);
            return outputData;
        }
    }

    // Semantic keyword projection
    const std::string lower = toLower(text);
    bool matchedAny = false;

    for (const auto& c : kConcepts) {
        if (lower.find(c.keyword) != std::string::npos) {
            matchedAny = true;
            embedding[c.targetDim] += c.weight;
            // Diffuse energy into adjacent dimensions
            for (int delta = -2; delta <= 2; ++delta) {
                int dim = c.targetDim + delta;
                if (dim >= 0 && dim < static_cast<int>(kEmbeddingDim)) {
                    embedding[dim] += c.weight * (1.0f / (1.0f + std::abs(delta)));
                }
            }
        }
    }

    // Word hash based projection for arbitrary unknown terms
    std::istringstream iss(lower);
    std::string token;
    while (iss >> token) {
        uint64_t h = util::xxhash64(token);
        size_t dim1 = (h % (kEmbeddingDim - 100)) + 100;
        size_t dim2 = ((h >> 16) % (kEmbeddingDim - 100)) + 100;
        embedding[dim1] += 0.5f;
        embedding[dim2] += 0.3f;
    }

    if (!matchedAny) {
        embedding[0] = 0.5f;
    }

    normalize(embedding);
    return embedding;
}

} // namespace reals::ai
