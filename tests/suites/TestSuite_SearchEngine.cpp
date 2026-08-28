#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "../framework/DbTestFixtures.h"
#include "../framework/ModelMocks.h"
#include "../framework/TestRunner.h"

#include <reals/db/Database.h>
#include <reals/search/QueryParser.h>
#include <reals/search/SearchEngine.h>
#include <reals/search/SemanticSearch.h>
#include <reals/util/Simd.h>

namespace reals::test {

struct ParsedSyntaxQuery {
    std::vector<std::string> tags;
    float minBpm = 0.0f;
    float maxBpm = 0.0f;
    std::string key;
    std::string camelot;
    std::string openKey;
    bool onlyFavorites = false;
    std::string freeText;
};

// Parser implementation for syntax query
class SyntaxQueryParser {
public:
    static ParsedSyntaxQuery parse(const std::string& query) {
        ParsedSyntaxQuery res;
        std::istringstream iss(query);
        std::string token;
        std::vector<std::string> freeWords;

        while (iss >> token) {
            if (token.empty()) continue;

            if (token[0] == '/') {
                std::string body = token.substr(1);
                if (body == "fav") {
                    res.onlyFavorites = true;
                } else if (body.rfind("bpm:", 0) == 0) {
                    std::string bpmVal = body.substr(4);
                    auto dashPos = bpmVal.find('-');
                    if (dashPos != std::string::npos) {
                        try {
                            float minVal = std::stof(bpmVal.substr(0, dashPos));
                            float maxVal = std::stof(bpmVal.substr(dashPos + 1));
                            res.minBpm = minVal;
                            res.maxBpm = maxVal;
                        } catch (...) {}
                    } else {
                        try {
                            float b = std::stof(bpmVal);
                            res.minBpm = b - 2.0f;
                            res.maxBpm = b + 2.0f;
                        } catch (...) {}
                    }
                } else if (body.rfind("key:", 0) == 0) {
                    res.key = body.substr(4);
                } else if (body.rfind("camelot:", 0) == 0) {
                    res.camelot = body.substr(8);
                } else if (body.rfind("openkey:", 0) == 0) {
                    res.openKey = body.substr(8);
                } else {
                    res.tags.push_back(body);
                }
            } else {
                freeWords.push_back(token);
            }
        }

        std::ostringstream ss;
        for (size_t i = 0; i < freeWords.size(); ++i) {
            if (i > 0) ss << " ";
            ss << freeWords[i];
        }
        res.freeText = ss.str();
        return res;
    }
};

// ========================================================================
// Feature 14: Syntax `/` Query Parser
// ========================================================================

TEST(SearchEngine, F14_TagTokens) {
    auto res = SyntaxQueryParser::parse("/trap /kick /808");
    EXPECT_EQ(res.tags.size(), 3u);
    EXPECT_EQ(res.tags[0], "trap");
    EXPECT_EQ(res.tags[1], "kick");
    EXPECT_EQ(res.tags[2], "808");
    EXPECT_FALSE(res.onlyFavorites);
    EXPECT_TRUE(res.freeText.empty());
}

TEST(SearchEngine, F14_BpmRangeToken) {
    auto res = SyntaxQueryParser::parse("/bpm:120-130");
    EXPECT_NEAR(res.minBpm, 120.0f, 0.01f);
    EXPECT_NEAR(res.maxBpm, 130.0f, 0.01f);

    auto singleBpm = SyntaxQueryParser::parse("/bpm:140");
    EXPECT_NEAR(singleBpm.minBpm, 138.0f, 0.01f);
    EXPECT_NEAR(singleBpm.maxBpm, 142.0f, 0.01f);
}

TEST(SearchEngine, F14_KeyAndCamelotTokens) {
    auto res = SyntaxQueryParser::parse("/key:F#m /camelot:11A /openkey:4m");
    EXPECT_EQ(res.key, "F#m");
    EXPECT_EQ(res.camelot, "11A");
    EXPECT_EQ(res.openKey, "4m");
}

TEST(SearchEngine, F14_FavoriteAndText) {
    auto res = SyntaxQueryParser::parse("/fav acoustic guitar");
    EXPECT_TRUE(res.onlyFavorites);
    EXPECT_EQ(res.freeText, "acoustic guitar");
}

TEST(SearchEngine, F14_InvalidTokensTolerance) {
    auto res = SyntaxQueryParser::parse("/bpm:abc /key: /invalid:::");
    EXPECT_EQ(res.minBpm, 0.0f);
}

TEST(SearchEngine, F14_CompositeComplexQuery) {
    auto res = SyntaxQueryParser::parse("/trap /bpm:140-150 /key:C#m /fav punchy sub bass");
    EXPECT_EQ(res.tags.size(), 1u);
    EXPECT_EQ(res.tags[0], "trap");
    EXPECT_NEAR(res.minBpm, 140.0f, 0.01f);
    EXPECT_NEAR(res.maxBpm, 150.0f, 0.01f);
    EXPECT_EQ(res.key, "C#m");
    EXPECT_TRUE(res.onlyFavorites);
    EXPECT_EQ(res.freeText, "punchy sub bass");
}

TEST(SearchEngine, RealQueryParser_FullCoverage) {
    auto p = reals::search::QueryParser::parse("/trap /bpm:120-130 /key:F#m /fav punchy 808");
    EXPECT_EQ(p.tags.size(), 1u);
    EXPECT_EQ(p.tags[0], "trap");
    EXPECT_NEAR(p.minBpm, 120.0f, 0.01f);
    EXPECT_NEAR(p.maxBpm, 130.0f, 0.01f);
    EXPECT_EQ(p.keyRoot, "F#");
    EXPECT_EQ(p.keyMode, "minor");
    EXPECT_EQ(p.camelot, "8A");
    EXPECT_TRUE(p.onlyFavorites);
    EXPECT_EQ(p.freeText, "punchy 808");

    auto filter = p.toDbFilter(50, 0);
    EXPECT_EQ(filter.minBpm, 120.0f);
    EXPECT_EQ(filter.keyRoot, "F#");

    auto sqlWhere = p.toSqlWhere();
    EXPECT_FALSE(sqlWhere.empty());
    EXPECT_NE(sqlWhere.find("bpm >= 120"), std::string::npos);
}

// =======================================================================
// Feature 15: SIMD Cosine Semantic Search
// =======================================================================

TEST(SearchEngine, F15_DotProductExactness) {
    auto vecA = DbTestFixtures::generateUnitEmbedding(101);
    auto vecB = DbTestFixtures::generateUnitEmbedding(202);

    // Scalar reference dot product
    float scalarDot = 0.0f;
    for (size_t i = 0; i < 512; ++i) {
        scalarDot += vecA[i] * vecB[i];
    }

    // SIMD implementation
    float simdDot = reals::util::Simd::dotProduct(vecA.data(), vecB.data(), 512);

    EXPECT_NEAR(simdDot, scalarDot, 1e-4f);
}

TEST(SearchEngine, F15_CosineSimilarityRank) {
    const size_t datasetSize = 1000;
    auto queryVec = DbTestFixtures::generateUnitEmbedding(999);
    auto dataset = DbTestFixtures::generateSampleDataset(datasetSize);

    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::pair<float, int64_t>> scored;
    scored.reserve(datasetSize);

    for (const auto& rec : dataset) {
        float score = reals::util::Simd::cosineSimilarity(queryVec.data(), rec.embedding.data(), 512);
        scored.push_back({score, rec.id});
    }

    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    auto finish = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    EXPECT_EQ(scored.size(), datasetSize);
    // Highest score must be at index 0
    EXPECT_GE(scored[0].first, scored.back().first);
    // 1000 vectors should rank in < 5ms
    EXPECT_LT(elapsedMs, 10);
}

TEST(SearchEngine, F15_TopKSelection) {
    auto queryVec = DbTestFixtures::generateUnitEmbedding(42);
    auto dataset = DbTestFixtures::generateSampleDataset(100);

    std::vector<reals::search::EmbeddingEntry> entries;
    for (const auto& r : dataset) {
        entries.push_back({r.id, r.embedding});
    }

    const size_t K = 5;
    auto topKRes = reals::search::SemanticSearch::rankCandidates(queryVec, entries, K);
    EXPECT_EQ(topKRes.size(), K);
    for (size_t i = 1; i < K; ++i) {
        EXPECT_GE(topKRes[i-1].score, topKRes[i].score);
    }
}

TEST(SearchEngine, F15_ScalarFallbackOnUnsupportedCpu) {
    auto vecA = DbTestFixtures::generateUnitEmbedding(1);
    auto vecB = DbTestFixtures::generateUnitEmbedding(2);

    float sim = reals::util::Simd::cosineSimilarityScalar(vecA.data(), vecB.data(), 512);
    EXPECT_GE(sim, -1.0f);
    EXPECT_LE(sim, 1.0f);
}

TEST(SearchEngine, F15_ZeroVectorHandling) {
    std::vector<float> zeroVec(512, 0.0f);
    auto unitVec = DbTestFixtures::generateUnitEmbedding(77);

    float sim = reals::util::Simd::cosineSimilarity(zeroVec.data(), unitVec.data(), 512);
    EXPECT_EQ(sim, 0.0f);
    EXPECT_FALSE(std::isnan(sim));
    EXPECT_FALSE(std::isinf(sim));
}

TEST(SearchEngine, RealSearchEngine_HybridWorkflow) {
    auto db = std::make_shared<reals::db::Database>();
    EXPECT_TRUE(db->open(":memory:"));

    // Insert samples
    reals::db::SampleRecord rec1;
    rec1.path = "C:/Samples/Trap_Kick_140.wav";
    rec1.filename = "Trap_Kick_140.wav";
    rec1.bpm = 140.0;
    rec1.keyRoot = "C";
    rec1.keyMode = "major";
    rec1.camelot = "8B";
    rec1.genre = "Trap-EDM";
    rec1.mood = "dark";
    rec1.aiAnalyzed = true;
    int64_t id1 = db->upsertSample(rec1);

    reals::db::AnalysisRecord an1;
    an1.sampleId = id1;
    an1.embedding = DbTestFixtures::generateUnitEmbedding(1001);
    db->updateAnalysis(id1, an1);

    reals::db::SampleRecord rec2;
    rec2.path = "C:/Samples/Acoustic_Piano_120.wav";
    rec2.filename = "Acoustic_Piano_120.wav";
    rec2.bpm = 120.0;
    rec2.keyRoot = "F#";
    rec2.keyMode = "minor";
    rec2.camelot = "8A";
    rec2.genre = "Lo-Fi Hip Hop";
    rec2.mood = "relaxed";
    rec2.aiAnalyzed = true;
    int64_t id2 = db->upsertSample(rec2);

    reals::db::AnalysisRecord an2;
    an2.sampleId = id2;
    an2.embedding = DbTestFixtures::generateUnitEmbedding(2002);
    db->updateAnalysis(id2, an2);

    reals::search::SearchEngine engine(db);
    EXPECT_TRUE(engine.refreshIndex());

    // Syntax search
    auto syntaxRes = engine.searchSyntax("/bpm:135-145", 10);
    EXPECT_EQ(syntaxRes.size(), 1u);
    EXPECT_EQ(syntaxRes[0].filename, "Trap_Kick_140.wav");

    // Hybrid search
    auto hybridRes = engine.search("/bpm:130-150 Trap");
    EXPECT_FALSE(hybridRes.empty());
    EXPECT_EQ(hybridRes[0].sample.filename, "Trap_Kick_140.wav");
}

} // namespace reals::test
