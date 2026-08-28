#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reals::ai {

class ClapEmbedder {
public:
    // Embedding vector dimension (CLAP standard)
    static constexpr size_t kEmbeddingDim = 512;

    // Extract 512-dimensional normalized float embedding from audio PCM
    [[nodiscard]] static std::vector<float> embedAudio(
        const float* pcm, size_t frames, int sampleRate);

    // Extract 512-dimensional normalized float embedding from text query
    [[nodiscard]] static std::vector<float> embedText(
        const std::string& text);

    // Calculate cosine similarity between two 512-dimensional embedding vectors
    [[nodiscard]] static float cosineSimilarity(
        const std::vector<float>& a, const std::vector<float>& b);

    // L2-normalize a vector in-place
    static void normalize(std::vector<float>& vec);
};

} // namespace reals::ai
