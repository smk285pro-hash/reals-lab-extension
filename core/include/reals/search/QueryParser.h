#pragma once

#include "reals/db/SampleRecord.h"

#include <string>
#include <string_view>
#include <vector>

namespace reals::search {

struct ParsedQuery {
    std::vector<std::string> tags;
    float minBpm = 0.0f;
    float maxBpm = 0.0f;
    std::string key;
    std::string keyRoot;
    std::string keyMode;
    std::string camelot;
    std::string openKey;
    std::string genre;
    std::string mood;
    bool onlyFavorites = false;
    std::string freeText;
    std::vector<std::string> keywords;

    [[nodiscard]] db::QueryFilter toDbFilter(int limit = 100, int offset = 0) const;
    [[nodiscard]] std::string toSqlWhere() const;
    [[nodiscard]] bool hasFilters() const;
    [[nodiscard]] bool hasFreeText() const;
};

class QueryParser {
public:
    // Parse an input query containing /syntax tokens and residual free text
    [[nodiscard]] static ParsedQuery parse(std::string_view query);

    // Normalize musical key string into root (e.g. "F#") and mode ("major" / "minor")
    static void parseKey(std::string_view keyStr, std::string& outRoot, std::string& outMode);

    // Convert Camelot notation (e.g. "11A", "8B") to Key notation (e.g. "F# Minor", "C Major")
    [[nodiscard]] static std::string camelotToKey(std::string_view camelot);

    // Convert Key notation to Camelot notation
    [[nodiscard]] static std::string keyToCamelot(std::string_view keyRoot, std::string_view keyMode);
};

} // namespace reals::search
