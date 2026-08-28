#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace reals::search {

struct SemanticResult {
    int64_t sampleId = 0;
    float score = 0.0f; // Cosine similarity [-1.0, 1.0]
};

struct EmbeddingEntry {
    int64_t sampleId = 0;
    std::vector<float> vector; // 512-dim unit float vector
};

class SemanticSearch {
public:
    SemanticSearch() = default;
    ~SemanticSearch() = default;

    SemanticSearch(const SemanticSearch&) = delete;
    SemanticSearch& operator=(const SemanticSearch&) = delete;
    SemanticSearch(SemanticSearch&&) noexcept;
    SemanticSearch& operator=(SemanticSearch&&) noexcept;

    // Load / set in-memory embedding dataset
    void setEmbeddings(std::vector<EmbeddingEntry> entries);
    void addEmbedding(int64_t sampleId, const std::vector<float>& embedding);
    void removeEmbedding(int64_t sampleId);
    void clear();

    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;

    // Rank indexed embeddings against a 512-dim query vector using SIMD cosine similarity
    [[nodiscard]] std::vector<SemanticResult> rank(
        const std::vector<float>& queryVec,
        size_t topK = 50,
        float minScore = -1.0f) const;

    // Rank ad-hoc candidates
    [[nodiscard]] static std::vector<SemanticResult> rankCandidates(
        const std::vector<float>& queryVec,
        const std::vector<EmbeddingEntry>& candidates,
        size_t topK = 50,
        float minScore = -1.0f);

    [[nodiscard]] static std::vector<SemanticResult> rankCandidates(
        const std::vector<float>& queryVec,
        const std::vector<std::pair<int64_t, std::vector<float>>>& candidates,
        size_t topK = 50,
        float minScore = -1.0f);

private:
    std::vector<EmbeddingEntry> m_entries;
    mutable std::mutex m_mutex;
};

} // namespace reals::search
