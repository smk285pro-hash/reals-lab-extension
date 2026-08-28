#pragma once

#include "reals/db/Database.h"
#include "reals/search/QueryParser.h"
#include "reals/search/SemanticSearch.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace reals::search {

struct SearchResult {
    db::SampleRecord sample;
    float semanticScore = 0.0f;
    float textScore = 0.0f;
    float combinedScore = 0.0f;
    std::vector<std::string> matchedTags;
};

struct SearchOptions {
    int limit = 50;
    int offset = 0;
    std::string basePath;            // Optional directory path filter
    bool enableSemantic = true;
    bool enableSyntax = true;
    float minSemanticScore = 0.0f;
    float semanticWeight = 0.5f;     // Weight balance: 0.0 = pure keyword/metadata, 1.0 = pure semantic
    bool onlyFavorites = false;
};

class SearchEngine {
public:
    explicit SearchEngine(std::shared_ptr<db::Database> db = nullptr);
    ~SearchEngine() = default;

    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;

    void setDatabase(std::shared_ptr<db::Database> db);
    [[nodiscard]] std::shared_ptr<db::Database> database() const;

    // Refresh semantic search embedding index from database
    bool refreshIndex();

    // Execute hybrid search (syntax token parsing + DB query + CLAP semantic ranking)
    [[nodiscard]] std::vector<SearchResult> search(
        const std::string& query,
        const SearchOptions& options = {});

    // Pure syntax query search (returns matching sample records)
    [[nodiscard]] std::vector<db::SampleRecord> searchSyntax(
        const std::string& query,
        int limit = 50);

    // Pure semantic search by natural language text query
    [[nodiscard]] std::vector<SearchResult> searchSemantic(
        const std::string& textQuery,
        int limit = 50,
        float minScore = 0.0f);

    // Pure semantic search by vector embedding
    [[nodiscard]] std::vector<SearchResult> searchSemanticVector(
        const std::vector<float>& queryEmbedding,
        int limit = 50,
        float minScore = 0.0f);

    // Find similar samples based on acoustic vector embedding + harmonic Camelot compatibility
    [[nodiscard]] std::vector<SearchResult> searchSimilar(
        int64_t sampleId,
        int limit = 50,
        float minScore = 0.30f);

    [[nodiscard]] std::vector<SearchResult> searchSimilarByPath(
        const std::string& filePath,
        int limit = 50,
        float minScore = 0.30f);

    [[nodiscard]] SemanticSearch& semanticSearch();

private:
    std::shared_ptr<db::Database> m_db;
    SemanticSearch m_semanticSearch;
    mutable std::mutex m_mutex;
};

} // namespace reals::search
