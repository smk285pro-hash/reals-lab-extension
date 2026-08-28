#include "reals/util/Simd.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(_MSC_VER)
    #include <intrin.h>
    #include <immintrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #include <cpuid.h>
    #include <immintrin.h>
#endif

namespace reals::util {

namespace {

constexpr float kEpsilon = 1e-9f;

struct CpuFeatures {
    bool avx2 = false;
    bool sse2 = false;
    bool fma = false;

    CpuFeatures() {
#if defined(_MSC_VER)
        int cpuInfo[4] = {0};
        __cpuid(cpuInfo, 0);
        int numIds = cpuInfo[0];

        if (numIds >= 1) {
            __cpuid(cpuInfo, 1);
            sse2 = (cpuInfo[3] & (1 << 26)) != 0;
            fma = (cpuInfo[2] & (1 << 12)) != 0;
        }

        if (numIds >= 7) {
            __cpuidex(cpuInfo, 7, 0);
            avx2 = (cpuInfo[1] & (1 << 5)) != 0;
        }
#elif defined(__GNUC__) || defined(__clang__)
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            sse2 = (edx & (1 << 26)) != 0;
            fma = (ecx & (1 << 12)) != 0;
        }
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            avx2 = (ebx & (1 << 5)) != 0;
        }
        #endif
#endif
    }
};

const CpuFeatures& getCpuFeatures() {
    static const CpuFeatures s_features;
    return s_features;
}

#if defined(__AVX2__) || defined(_MSC_VER)
inline float hsum256_ps(__m256 v) {
    __m128 vlow = _mm256_castps256_ps128(v);
    __m128 vhigh = _mm256_extractf128_ps(v, 1);
    __m128 v128 = _mm_add_ps(vlow, vhigh);
    __m128 shuf = _mm_movehl_ps(v128, v128);
    __m128 sums = _mm_add_ps(v128, shuf);
    __m128 shuf2 = _mm_shuffle_ps(sums, sums, 1);
    sums = _mm_add_ss(sums, shuf2);
    return _mm_cvtss_f32(sums);
}

float dotProductAvx2(const float* a, const float* b, size_t dim) {
    __m256 acc0 = _mm256_setzero_ps();
    __m256 acc1 = _mm256_setzero_ps();
    __m256 acc2 = _mm256_setzero_ps();
    __m256 acc3 = _mm256_setzero_ps();

    size_t i = 0;
    // Process 32 floats per iteration
    for (; i + 31 < dim; i += 32) {
        __m256 a0 = _mm256_loadu_ps(a + i);
        __m256 b0 = _mm256_loadu_ps(b + i);
        __m256 a1 = _mm256_loadu_ps(a + i + 8);
        __m256 b1 = _mm256_loadu_ps(b + i + 8);
        __m256 a2 = _mm256_loadu_ps(a + i + 16);
        __m256 b2 = _mm256_loadu_ps(b + i + 16);
        __m256 a3 = _mm256_loadu_ps(a + i + 24);
        __m256 b3 = _mm256_loadu_ps(b + i + 24);

        acc0 = _mm256_add_ps(acc0, _mm256_mul_ps(a0, b0));
        acc1 = _mm256_add_ps(acc1, _mm256_mul_ps(a1, b1));
        acc2 = _mm256_add_ps(acc2, _mm256_mul_ps(a2, b2));
        acc3 = _mm256_add_ps(acc3, _mm256_mul_ps(a3, b3));
    }

    acc0 = _mm256_add_ps(acc0, acc1);
    acc2 = _mm256_add_ps(acc2, acc3);
    acc0 = _mm256_add_ps(acc0, acc2);

    for (; i + 7 < dim; i += 8) {
        __m256 a0 = _mm256_loadu_ps(a + i);
        __m256 b0 = _mm256_loadu_ps(b + i);
        acc0 = _mm256_add_ps(acc0, _mm256_mul_ps(a0, b0));
    }

    float dot = hsum256_ps(acc0);

    for (; i < dim; ++i) {
        dot += a[i] * b[i];
    }
    return dot;
}

void normAndDotAvx2(const float* a, const float* b, size_t dim, float& outDot, float& outNormA, float& outNormB) {
    __m256 dotAcc = _mm256_setzero_ps();
    __m256 normAAcc = _mm256_setzero_ps();
    __m256 normBAcc = _mm256_setzero_ps();

    size_t i = 0;
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        dotAcc = _mm256_add_ps(dotAcc, _mm256_mul_ps(va, vb));
        normAAcc = _mm256_add_ps(normAAcc, _mm256_mul_ps(va, va));
        normBAcc = _mm256_add_ps(normBAcc, _mm256_mul_ps(vb, vb));
    }

    outDot = hsum256_ps(dotAcc);
    outNormA = hsum256_ps(normAAcc);
    outNormB = hsum256_ps(normBAcc);

    for (; i < dim; ++i) {
        outDot += a[i] * b[i];
        outNormA += a[i] * a[i];
        outNormB += b[i] * b[i];
    }
}
#endif

#if defined(__SSE2__) || defined(_MSC_VER)
inline float hsum128_ps(__m128 v) {
    __m128 shuf = _mm_movehl_ps(v, v);
    __m128 sums = _mm_add_ps(v, shuf);
    __m128 shuf2 = _mm_shuffle_ps(sums, sums, 1);
    sums = _mm_add_ss(sums, shuf2);
    return _mm_cvtss_f32(sums);
}

float dotProductSse2(const float* a, const float* b, size_t dim) {
    __m128 acc0 = _mm_setzero_ps();
    __m128 acc1 = _mm_setzero_ps();
    __m128 acc2 = _mm_setzero_ps();
    __m128 acc3 = _mm_setzero_ps();

    size_t i = 0;
    for (; i + 15 < dim; i += 16) {
        __m128 a0 = _mm_loadu_ps(a + i);
        __m128 b0 = _mm_loadu_ps(b + i);
        __m128 a1 = _mm_loadu_ps(a + i + 4);
        __m128 b1 = _mm_loadu_ps(b + i + 4);
        __m128 a2 = _mm_loadu_ps(a + i + 8);
        __m128 b2 = _mm_loadu_ps(b + i + 8);
        __m128 a3 = _mm_loadu_ps(a + i + 12);
        __m128 b3 = _mm_loadu_ps(b + i + 12);

        acc0 = _mm_add_ps(acc0, _mm_mul_ps(a0, b0));
        acc1 = _mm_add_ps(acc1, _mm_mul_ps(a1, b1));
        acc2 = _mm_add_ps(acc2, _mm_mul_ps(a2, b2));
        acc3 = _mm_add_ps(acc3, _mm_mul_ps(a3, b3));
    }

    acc0 = _mm_add_ps(acc0, acc1);
    acc2 = _mm_add_ps(acc2, acc3);
    acc0 = _mm_add_ps(acc0, acc2);

    for (; i + 3 < dim; i += 4) {
        __m128 a0 = _mm_loadu_ps(a + i);
        __m128 b0 = _mm_loadu_ps(b + i);
        acc0 = _mm_add_ps(acc0, _mm_mul_ps(a0, b0));
    }

    float dot = hsum128_ps(acc0);
    for (; i < dim; ++i) {
        dot += a[i] * b[i];
    }
    return dot;
}

void normAndDotSse2(const float* a, const float* b, size_t dim, float& outDot, float& outNormA, float& outNormB) {
    __m128 dotAcc = _mm_setzero_ps();
    __m128 normAAcc = _mm_setzero_ps();
    __m128 normBAcc = _mm_setzero_ps();

    size_t i = 0;
    for (; i + 3 < dim; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 vb = _mm_loadu_ps(b + i);
        dotAcc = _mm_add_ps(dotAcc, _mm_mul_ps(va, vb));
        normAAcc = _mm_add_ps(normAAcc, _mm_mul_ps(va, va));
        normBAcc = _mm_add_ps(normBAcc, _mm_mul_ps(vb, vb));
    }

    outDot = hsum128_ps(dotAcc);
    outNormA = hsum128_ps(normAAcc);
    outNormB = hsum128_ps(normBAcc);

    for (; i < dim; ++i) {
        outDot += a[i] * b[i];
        outNormA += a[i] * a[i];
        outNormB += b[i] * b[i];
    }
}
#endif

} // namespace

bool Simd::hasAvx2() {
    return getCpuFeatures().avx2;
}

bool Simd::hasSse2() {
    return getCpuFeatures().sse2;
}

float Simd::dotProductScalar(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        dot += a[i] * b[i];
    }
    return dot;
}

float Simd::cosineSimilarityScalar(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float dot = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;
    for (size_t i = 0; i < dim; ++i) {
        dot += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }
    const float denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom <= kEpsilon) return 0.0f;
    return std::clamp(dot / denom, -1.0f, 1.0f);
}

float Simd::dotProduct(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (getCpuFeatures().avx2) {
        return dotProductAvx2(a, b, dim);
    }
#endif

#if defined(__SSE2__) || defined(_MSC_VER)
    if (getCpuFeatures().sse2) {
        return dotProductSse2(a, b, dim);
    }
#endif

    return dotProductScalar(a, b, dim);
}

float Simd::cosineSimilarity(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;

    float dot = 0.0f;
    float normA = 0.0f;
    float normB = 0.0f;

#if defined(__AVX2__) || defined(_MSC_VER)
    if (getCpuFeatures().avx2) {
        normAndDotAvx2(a, b, dim, dot, normA, normB);
    } else
#endif
#if defined(__SSE2__) || defined(_MSC_VER)
    if (getCpuFeatures().sse2) {
        normAndDotSse2(a, b, dim, dot, normA, normB);
    } else
#endif
    {
        for (size_t i = 0; i < dim; ++i) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
    }

    const float denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom <= kEpsilon || std::isnan(denom) || std::isinf(denom)) {
        return 0.0f;
    }
    return std::clamp(dot / denom, -1.0f, 1.0f);
}

float Simd::cosineSimilarityNormalized(const float* a, const float* b, size_t dim) {
    if (!a || !b || dim == 0) return 0.0f;
    float dot = dotProduct(a, b, dim);
    if (std::isnan(dot) || std::isinf(dot)) return 0.0f;
    return std::clamp(dot, -1.0f, 1.0f);
}

float Simd::l2Norm(const float* vec, size_t dim) {
    if (!vec || dim == 0) return 0.0f;
    float dot = dotProduct(vec, vec, dim);
    return std::sqrt(std::max(0.0f, dot));
}

void Simd::normalize(float* vec, size_t dim) {
    if (!vec || dim == 0) return;
    const float norm = l2Norm(vec, dim);
    if (norm > kEpsilon) {
        const float invNorm = 1.0f / norm;
        for (size_t i = 0; i < dim; ++i) {
            vec[i] *= invNorm;
        }
    } else {
        std::memset(vec, 0, dim * sizeof(float));
    }
}

void Simd::normalize(std::vector<float>& vec) {
    normalize(vec.data(), vec.size());
}

void Simd::batchCosineSimilarity(
    const float* queryVec,
    const float* const* candidates,
    size_t numCandidates,
    float* outScores,
    size_t dim,
    bool preNormalized) {
    if (!queryVec || !candidates || !outScores || numCandidates == 0 || dim == 0) {
        return;
    }

    if (preNormalized) {
        for (size_t i = 0; i < numCandidates; ++i) {
            outScores[i] = candidates[i] ? cosineSimilarityNormalized(queryVec, candidates[i], dim) : 0.0f;
        }
    } else {
        for (size_t i = 0; i < numCandidates; ++i) {
            outScores[i] = candidates[i] ? cosineSimilarity(queryVec, candidates[i], dim) : 0.0f;
        }
    }
}

} // namespace reals::util
