#include "reals/search/SemanticSearch.h"
#include "reals/util/Simd.h"

#include <algorithm>

namespace reals::search {

SemanticSearch::SemanticSearch(SemanticSearch&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.m_mutex);
    m_entries = std::move(other.m_entries);
}

SemanticSearch& SemanticSearch::operator=(SemanticSearch&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_entries = std::move(other.m_entries);
    }
    return *this;
}

void SemanticSearch::setEmbeddings(std::vector<EmbeddingEntry> entries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries = std::move(entries);
}

void SemanticSearch::addEmbedding(int64_t sampleId, const std::vector<float>& embedding) {
    if (embedding.size() != util::Simd::kDefaultDim) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_entries) {
        if (e.sampleId == sampleId) {
            e.vector = embedding;
            return;
        }
    }
    m_entries.push_back({sampleId, embedding});
}

void SemanticSearch::removeEmbedding(int64_t sampleId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
                       [sampleId](const EmbeddingEntry& e) { return e.sampleId == sampleId; }),
        m_entries.end());
}

void SemanticSearch::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

size_t SemanticSearch::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.size();
}

bool SemanticSearch::empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries.empty();
}

std::vector<SemanticResult> SemanticSearch::rank(
    const std::vector<float>& queryVec,
    size_t topK,
    float minScore) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return rankCandidates(queryVec, m_entries, topK, minScore);
}

std::vector<SemanticResult> SemanticSearch::rankCandidates(
    const std::vector<float>& queryVec,
    const std::vector<EmbeddingEntry>& candidates,
    size_t topK,
    float minScore) {
    if (queryVec.size() != util::Simd::kDefaultDim || candidates.empty() || topK == 0) {
        return {};
    }

    std::vector<SemanticResult> scored;
    scored.reserve(candidates.size());

    const float* qPtr = queryVec.data();

    for (const auto& entry : candidates) {
        if (entry.vector.size() != util::Simd::kDefaultDim) continue;
        float s = util::Simd::cosineSimilarity(qPtr, entry.vector.data(), util::Simd::kDefaultDim);
        if (s >= minScore) {
            scored.push_back({entry.sampleId, s});
        }
    }

    if (scored.empty()) {
        return {};
    }

    const size_t k = std::min(topK, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + k, scored.end(),
                      [](const SemanticResult& a, const SemanticResult& b) {
                          return a.score > b.score;
                      });

    scored.resize(k);
    return scored;
}

std::vector<SemanticResult> SemanticSearch::rankCandidates(
    const std::vector<float>& queryVec,
    const std::vector<std::pair<int64_t, std::vector<float>>>& candidates,
    size_t topK,
    float minScore) {
    if (queryVec.size() != util::Simd::kDefaultDim || candidates.empty() || topK == 0) {
        return {};
    }

    std::vector<SemanticResult> scored;
    scored.reserve(candidates.size());

    const float* qPtr = queryVec.data();

    for (const auto& [id, vec] : candidates) {
        if (vec.size() != util::Simd::kDefaultDim) continue;
        float s = util::Simd::cosineSimilarity(qPtr, vec.data(), util::Simd::kDefaultDim);
        if (s >= minScore) {
            scored.push_back({id, s});
        }
    }

    if (scored.empty()) {
        return {};
    }

    const size_t k = std::min(topK, scored.size());
    std::partial_sort(scored.begin(), scored.begin() + k, scored.end(),
                      [](const SemanticResult& a, const SemanticResult& b) {
                          return a.score > b.score;
                      });

    scored.resize(k);
    return scored;
}

} // namespace reals::search
