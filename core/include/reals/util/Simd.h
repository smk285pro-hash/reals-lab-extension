#pragma once

#include <cstddef>
#include <vector>

namespace reals::util {

class Simd {
public:
    static constexpr size_t kDefaultDim = 512;

    // CPU feature detection
    [[nodiscard]] static bool hasAvx2();
    [[nodiscard]] static bool hasSse2();

    // Dot product of vectors of size dim (using AVX2 / SSE2 / scalar fallback)
    [[nodiscard]] static float dotProduct(const float* a, const float* b, size_t dim = kDefaultDim);

    // Cosine similarity between arbitrary non-normalized vectors (handles norms and zero vectors safely)
    [[nodiscard]] static float cosineSimilarity(const float* a, const float* b, size_t dim = kDefaultDim);

    // Fast cosine similarity when both vectors are guaranteed to be pre-normalized to unit L2 norm
    [[nodiscard]] static float cosineSimilarityNormalized(const float* a, const float* b, size_t dim = kDefaultDim);

    // Compute L2 norm (magnitude) of a vector
    [[nodiscard]] static float l2Norm(const float* vec, size_t dim = kDefaultDim);

    // In-place L2 normalization of a float buffer
    static void normalize(float* vec, size_t dim = kDefaultDim);

    // In-place L2 normalization of std::vector
    static void normalize(std::vector<float>& vec);

    // Batch cosine similarity ranking against multiple candidate vectors
    static void batchCosineSimilarity(
        const float* queryVec,
        const float* const* candidates,
        size_t numCandidates,
        float* outScores,
        size_t dim = kDefaultDim,
        bool preNormalized = true);

    // Scalar reference implementations
    [[nodiscard]] static float dotProductScalar(const float* a, const float* b, size_t dim);
    [[nodiscard]] static float cosineSimilarityScalar(const float* a, const float* b, size_t dim);
};

} // namespace reals::util
