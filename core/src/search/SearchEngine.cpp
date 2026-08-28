#include "reals/search/SearchEngine.h"
#include "reals/ai/ClapEmbedder.h"
#include "reals/platform/Path.h"
#include "reals/util/Simd.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace reals::search {

namespace {

std::string toLower(std::string_view str) {
    std::string out;
    out.reserve(str.size());
    for (char c : str) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

float computeMatchScore(const db::SampleRecord& rec,
                        const std::vector<std::string>& keywords,
                        const std::vector<std::string>& tags,
                        std::vector<std::string>& outMatchedTags) {
    if (keywords.empty() && tags.empty()) return 1.0f;

    std::string lowerFilename = toLower(rec.filename);
    std::string lowerPath = toLower(rec.path);
    std::string lowerGenre = toLower(rec.genre);
    std::string lowerMood = toLower(rec.mood);

    float matchedCount = 0.0f;
    float totalTerms = static_cast<float>(keywords.size() + tags.size());

    for (const auto& kw : keywords) {
        std::string lowerKw = toLower(kw);
        bool hit = false;
        if (lowerFilename.find(lowerKw) != std::string::npos) {
            matchedCount += 1.0f;
            hit = true;
        } else if (lowerGenre.find(lowerKw) != std::string::npos) {
            matchedCount += 0.9f;
            hit = true;
        } else if (lowerMood.find(lowerKw) != std::string::npos) {
            matchedCount += 0.8f;
            hit = true;
        } else if (lowerPath.find(lowerKw) != std::string::npos) {
            matchedCount += 0.6f;
            hit = true;
        }
        if (hit) {
            outMatchedTags.push_back(kw);
        }
    }

    for (const auto& tag : tags) {
        std::string lowerTag = toLower(tag);
        bool hit = false;
        if (lowerGenre.find(lowerTag) != std::string::npos) {
            matchedCount += 1.0f;
            hit = true;
        } else if (lowerMood.find(lowerTag) != std::string::npos) {
            matchedCount += 1.0f;
            hit = true;
        } else if (lowerFilename.find(lowerTag) != std::string::npos) {
            matchedCount += 1.0f;
            hit = true;
        } else if (lowerPath.find(lowerTag) != std::string::npos) {
            matchedCount += 0.8f;
            hit = true;
        }
        if (hit) {
            outMatchedTags.push_back(tag);
        }
    }

    return std::clamp(matchedCount / (totalTerms > 0.0f ? totalTerms : 1.0f), 0.0f, 1.0f);
}

} // namespace

SearchEngine::SearchEngine(std::shared_ptr<db::Database> db)
    : m_db(std::move(db)) {
    if (m_db && m_db->isOpen()) {
        refreshIndex();
    }
}

void SearchEngine::setDatabase(std::shared_ptr<db::Database> db) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_db = std::move(db);
    if (m_db && m_db->isOpen()) {
        auto embs = m_db->getAllEmbeddings();
        std::vector<EmbeddingEntry> entries;
        entries.reserve(embs.size());
        for (auto& [id, vec] : embs) {
            entries.push_back({id, std::move(vec)});
        }
        m_semanticSearch.setEmbeddings(std::move(entries));
    }
}

std::shared_ptr<db::Database> SearchEngine::database() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_db;
}

SemanticSearch& SearchEngine::semanticSearch() {
    return m_semanticSearch;
}

bool SearchEngine::refreshIndex() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || !m_db->isOpen()) {
        return false;
    }
    auto embs = m_db->getAllEmbeddings();
    std::vector<EmbeddingEntry> entries;
    entries.reserve(embs.size());
    for (auto& [id, vec] : embs) {
        entries.push_back({id, std::move(vec)});
    }
    m_semanticSearch.setEmbeddings(std::move(entries));
    return true;
}

std::vector<SearchResult> SearchEngine::search(
    const std::string& query,
    const SearchOptions& options) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_db || !m_db->isOpen()) {
        return {};
    }

    ParsedQuery parsed = QueryParser::parse(query);
    db::QueryFilter filter = parsed.toDbFilter(1000, 0);

    if (options.onlyFavorites || parsed.onlyFavorites) {
        filter.userTag = "favorite";
    }

    std::vector<db::SampleRecord> candidates = m_db->querySamples(filter);
    if (candidates.empty()) {
        // If strict tag or freeText filter yielded nothing, relax text/tag and search broader
        db::QueryFilter relaxedFilter = filter;
        relaxedFilter.text.clear();
        relaxedFilter.userTag.clear();
        candidates = m_db->querySamples(relaxedFilter);
    }

    if (candidates.empty()) {
        return {};
    }

    std::string normBase;
    if (!options.basePath.empty()) {
        normBase = toLower(platform::normalizePath(options.basePath));
    }

    std::vector<float> queryEmb;
    bool hasQueryEmb = false;
    if (options.enableSemantic && parsed.hasFreeText()) {
        queryEmb = ai::ClapEmbedder::embedText(parsed.freeText);
        hasQueryEmb = (queryEmb.size() == util::Simd::kDefaultDim);
    }

    std::vector<SearchResult> results;
    results.reserve(candidates.size());

    for (auto& cand : candidates) {
        if (!normBase.empty()) {
            std::string candNorm = toLower(platform::normalizePath(cand.path));
            if (candNorm.rfind(normBase, 0) != 0) {
                continue;
            }
        }

        SearchResult res;
        res.sample = std::move(cand);

        res.textScore = computeMatchScore(res.sample, parsed.keywords, parsed.tags, res.matchedTags);

        if (hasQueryEmb) {
            auto analysisOpt = m_db->getAnalysis(res.sample.id);
            if (analysisOpt && analysisOpt->embedding.size() == util::Simd::kDefaultDim) {
                res.semanticScore = util::Simd::cosineSimilarity(
                    queryEmb.data(),
                    analysisOpt->embedding.data(),
                    util::Simd::kDefaultDim);
            }
        }

        if (hasQueryEmb) {
            float semPart = std::max(0.0f, res.semanticScore);
            res.combinedScore = (1.0f - options.semanticWeight) * res.textScore + options.semanticWeight * semPart;
        } else {
            res.combinedScore = res.textScore > 0.0f ? res.textScore : 1.0f;
        }

        if (hasQueryEmb && res.semanticScore < options.minSemanticScore && res.textScore <= 0.0f) {
            continue;
        }

        results.push_back(std::move(res));
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.combinedScore > b.combinedScore;
    });

    const size_t maxCount = static_cast<size_t>(options.limit > 0 ? options.limit : 50);
    if (results.size() > maxCount) {
        results.resize(maxCount);
    }
    return results;
}

std::vector<db::SampleRecord> SearchEngine::searchSyntax(
    const std::string& query,
    int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || !m_db->isOpen()) {
        return {};
    }

    ParsedQuery parsed = QueryParser::parse(query);
    db::QueryFilter filter = parsed.toDbFilter(limit, 0);
    return m_db->querySamples(filter);
}

std::vector<SearchResult> SearchEngine::searchSemantic(
    const std::string& textQuery,
    int limit,
    float minScore) {
    if (textQuery.empty()) return {};
    auto emb = ai::ClapEmbedder::embedText(textQuery);
    return searchSemanticVector(emb, limit, minScore);
}

std::vector<SearchResult> SearchEngine::searchSemanticVector(
    const std::vector<float>& queryEmbedding,
    int limit,
    float minScore) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || !m_db->isOpen() || queryEmbedding.size() != util::Simd::kDefaultDim) {
        return {};
    }

    auto ranked = m_semanticSearch.rank(queryEmbedding, static_cast<size_t>(limit), minScore);
    std::vector<SearchResult> results;
    results.reserve(ranked.size());

    for (const auto& item : ranked) {
        auto sampleOpt = m_db->getSampleById(item.sampleId);
        if (sampleOpt) {
            SearchResult res;
            res.sample = *sampleOpt;
            res.semanticScore = item.score;
            res.combinedScore = item.score;
            results.push_back(std::move(res));
        }
    }
    return results;
}

namespace {

float computeHarmonicBonus(const db::SampleRecord& src, const db::SampleRecord& target) {
    float bonus = 0.0f;
    // Camelot key compatibility
    if (!src.camelot.empty() && !target.camelot.empty()) {
        if (src.camelot == target.camelot) {
            bonus += 0.06f; // Exact same key
        } else {
            try {
                int srcNum = std::stoi(src.camelot);
                char srcLetter = src.camelot.back();
                int tgtNum = std::stoi(target.camelot);
                char tgtLetter = target.camelot.back();

                int diff = std::abs(srcNum - tgtNum);
                if (diff == 11) diff = 1; // 12 and 1 are adjacent

                if (srcLetter == tgtLetter && diff == 1) {
                    bonus += 0.04f; // Adjacent key on circle of fifths (e.g. 8A -> 7A or 9A)
                } else if (srcLetter != tgtLetter && diff == 0) {
                    bonus += 0.04f; // Relative Major/Minor (e.g. 8A -> 8B)
                } else if (srcLetter != tgtLetter && diff == 1) {
                    bonus += 0.02f; // Diagonal key (e.g. 8A -> 7B or 9B)
                }
            } catch (...) {}
        }
    }

    // BPM proximity compatibility
    if (src.bpm > 30.0 && target.bpm > 30.0) {
        double ratio = src.bpm / target.bpm;
        if (std::abs(ratio - 1.0) <= 0.04) {
            bonus += 0.03f; // Same BPM (within 4%)
        } else if (std::abs(ratio - 2.0) <= 0.06 || std::abs(ratio - 0.5) <= 0.03) {
            bonus += 0.02f; // Half / double time
        }
    }

    // Same category / instrument tag bonus
    if (!src.genre.empty() && !target.genre.empty() && toLower(src.genre) == toLower(target.genre)) {
        bonus += 0.03f;
    }

    return bonus;
}

} // namespace

std::vector<SearchResult> SearchEngine::searchSimilar(
    int64_t sampleId,
    int limit,
    float minScore) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db || !m_db->isOpen() || sampleId <= 0) {
        return {};
    }

    auto srcOpt = m_db->getSampleById(sampleId);
    if (!srcOpt.has_value()) {
        return {};
    }
    const auto& srcSample = srcOpt.value();

    auto analysisOpt = m_db->getAnalysis(sampleId);
    std::vector<float> queryVec;
    if (analysisOpt && analysisOpt->embedding.size() == util::Simd::kDefaultDim) {
        queryVec = analysisOpt->embedding;
    } else {
        queryVec = ai::ClapEmbedder::embedText(srcSample.genre + " " + srcSample.filename);
    }

    if (queryVec.size() != util::Simd::kDefaultDim) {
        return {};
    }

    auto allEmbs = m_db->getAllEmbeddings();
    std::vector<SearchResult> results;
    results.reserve(allEmbs.size());

    for (const auto& [id, emb] : allEmbs) {
        if (id == sampleId) continue; // Exclude self
        if (emb.size() != util::Simd::kDefaultDim) continue;

        float cosSim = util::Simd::cosineSimilarity(
            queryVec.data(),
            emb.data(),
            util::Simd::kDefaultDim);

        auto targetOpt = m_db->getSampleById(id);
        if (!targetOpt.has_value()) continue;
        const auto& targetSample = targetOpt.value();

        float bonus = computeHarmonicBonus(srcSample, targetSample);
        float finalScore = std::clamp(cosSim + bonus, -1.0f, 1.0f);

        if (finalScore >= minScore) {
            SearchResult res;
            res.sample = targetSample;
            res.semanticScore = cosSim;
            res.combinedScore = finalScore;
            results.push_back(std::move(res));
        }
    }

    std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.combinedScore > b.combinedScore;
    });

    const size_t maxTake = static_cast<size_t>(limit > 0 ? limit : 50);
    if (results.size() > maxTake) {
        results.resize(maxTake);
    }
    return results;
}

std::vector<SearchResult> SearchEngine::searchSimilarByPath(
    const std::string& filePath,
    int limit,
    float minScore) {
    if (!m_db || !m_db->isOpen() || filePath.empty()) {
        return {};
    }
    auto sOpt = m_db->getSampleByPath(filePath);
    if (sOpt.has_value()) {
        return searchSimilar(sOpt->id, limit, minScore);
    }
    return {};
}

} // namespace reals::search
