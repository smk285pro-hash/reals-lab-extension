# SCOPE — Milestone 4: Syntax & SIMD Semantic Search Engine

## Scope Requirements
1. Feature 14: Syntax / Query Parser (eals::search::QueryParser)
   - Header: core/include/reals/search/QueryParser.h
   - Source: core/src/search/QueryParser.cpp
   - Tokenizes and parses /tag, /bpm:min-max (or /bpm:val), /key:val, /camelot:, /openkey:, /fav, and free-text keywords.
   - Handles tolerance for malformed tokens without throwing exceptions.
   - Generates structured query filters compatible with eals::db::QueryFilter and search criteria.

2. Feature 15: SIMD Cosine Semantic Search (eals::util::Simd, eals::search::SemanticSearch, eals::search::SearchEngine)
   - eals::util::Simd:
     - Header: core/include/reals/util/Simd.h
     - Source: core/src/util/Simd.cpp
     - AVX2 / SSE2 vector dot-product and cosine similarity calculation over 512-dim float vectors with scalar fallback.
     - L2 normalization, dot product, cosine similarity functions.
   - eals::search::SemanticSearch:
     - Header: core/include/reals/search/SemanticSearch.h
     - Source: core/src/search/SemanticSearch.cpp
     - Cosine similarity ranking against library embeddings from eals::db::Database or in-memory vector store.
     - Top-K selection with confidence/similarity scores.
   - eals::search::SearchEngine:
     - Header: core/include/reals/search/SearchEngine.h
     - Source: core/src/search/SearchEngine.cpp
     - Hybrid search coordinating syntax SQL filtering, keyword matching, and semantic CLAP ranking.
     - Indexing and embedding cache integration.

3. CMake & Integration:
   - Ensure CMakeLists.txt builds search and simd modules as part of eals_core.
   - Add unit tests / E2E tests for QueryParser, Simd, SemanticSearch, and SearchEngine.
   - Zero compiler warnings under C++20.
   - Ensure ctest --preset windows passes all test suites.
