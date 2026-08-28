#include "reals/search/QueryParser.h"

#include <algorithm>
#include <cctype>
#include <sstream>

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

std::string trim(std::string_view str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    return std::string(str.substr(start, end - start));
}

bool safeParseFloat(std::string_view str, float& outVal) {
    if (str.empty()) return false;
    try {
        size_t idx = 0;
        std::string s(str);
        float v = std::stof(s, &idx);
        if (idx == 0) return false;
        outVal = v;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

void QueryParser::parseKey(std::string_view keyStr, std::string& outRoot, std::string& outMode) {
    outRoot.clear();
    outMode.clear();
    if (keyStr.empty()) return;

    std::string s = trim(keyStr);
    if (s.empty()) return;

    char note = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    if (note < 'A' || note > 'G') return;

    outRoot.push_back(note);
    size_t idx = 1;
    if (idx < s.size() && (s[idx] == '#' || s[idx] == 'b')) {
        outRoot.push_back(s[idx]);
        ++idx;
    }

    std::string rest = toLower(s.substr(idx));
    rest = trim(rest);

    if (rest.empty()) {
        outMode = "major";
    } else if (rest == "m" || rest == "min" || rest == "minor" || rest.rfind("min", 0) == 0) {
        outMode = "minor";
    } else if (rest == "maj" || rest == "major" || rest.rfind("maj", 0) == 0) {
        outMode = "major";
    } else {
        outMode = "major";
    }
}

std::string QueryParser::camelotToKey(std::string_view camelot) {
    std::string c = toLower(trim(camelot));
    if (c == "8b") return "C Major";
    if (c == "5a") return "A Minor";
    if (c == "3b") return "Db Major";
    if (c == "12a") return "Bb Minor";
    if (c == "10b") return "D Major";
    if (c == "7a") return "B Minor";
    if (c == "5b") return "Eb Major";
    if (c == "2a") return "C Minor";
    if (c == "12b") return "E Major";
    if (c == "9a") return "C# Minor";
    if (c == "7b") return "F Major";
    if (c == "4a") return "D Minor";
    if (c == "2b") return "F# Major";
    if (c == "11a") return "D# Minor";
    if (c == "9b") return "G Major";
    if (c == "6a") return "E Minor";
    if (c == "4b") return "Ab Major";
    if (c == "1a") return "F Minor";
    if (c == "11b") return "A Major";
    if (c == "8a") return "F# Minor";
    if (c == "6b") return "Bb Major";
    if (c == "3a") return "G Minor";
    if (c == "1b") return "B Major";
    if (c == "10a") return "G# Minor";
    return "";
}

std::string QueryParser::keyToCamelot(std::string_view keyRoot, std::string_view keyMode) {
    std::string root = trim(keyRoot);
    std::string mode = toLower(trim(keyMode));
    bool isMinor = (mode == "minor" || mode == "m");

    if (root == "C" || root == "B#") return isMinor ? "2A" : "8B";
    if (root == "C#" || root == "Db") return isMinor ? "9A" : "3B";
    if (root == "D") return isMinor ? "4A" : "10B";
    if (root == "D#" || root == "Eb") return isMinor ? "11A" : "5B";
    if (root == "E") return isMinor ? "6A" : "12B";
    if (root == "F") return isMinor ? "1A" : "7B";
    if (root == "F#" || root == "Gb") return isMinor ? "8A" : "2B";
    if (root == "G") return isMinor ? "3A" : "9B";
    if (root == "G#" || root == "Ab") return isMinor ? "10A" : "4B";
    if (root == "A") return isMinor ? "5A" : "11B";
    if (root == "A#" || root == "Bb") return isMinor ? "12A" : "6B";
    if (root == "B" || root == "Cb") return isMinor ? "7A" : "1B";
    return "";
}

ParsedQuery QueryParser::parse(std::string_view query) {
    ParsedQuery res;
    std::istringstream iss{std::string(query)};
    std::string token;
    std::vector<std::string> freeWords;

    while (iss >> token) {
        if (token.empty()) continue;

        if (token[0] == '/') {
            std::string body = token.substr(1);
            if (body.empty()) continue;

            if (body == "fav" || body == "favorite") {
                res.onlyFavorites = true;
            } else if (body.rfind("bpm:", 0) == 0) {
                std::string bpmVal = body.substr(4);
                auto dashPos = bpmVal.find('-');
                if (dashPos != std::string::npos) {
                    float minB = 0.0f;
                    float maxB = 0.0f;
                    if (safeParseFloat(bpmVal.substr(0, dashPos), minB) &&
                        safeParseFloat(bpmVal.substr(dashPos + 1), maxB)) {
                        res.minBpm = minB;
                        res.maxBpm = maxB;
                    }
                } else {
                    float singleB = 0.0f;
                    if (safeParseFloat(bpmVal, singleB)) {
                        res.minBpm = singleB - 2.0f;
                        res.maxBpm = singleB + 2.0f;
                    }
                }
            } else if (body.rfind("key:", 0) == 0) {
                std::string gVal = body.substr(4);
                if (!gVal.empty()) {
                    res.key = gVal;
                    parseKey(gVal, res.keyRoot, res.keyMode);
                    if (res.camelot.empty() && !res.keyRoot.empty()) {
                        res.camelot = keyToCamelot(res.keyRoot, res.keyMode);
                    }
                }
            } else if (body.rfind("camelot:", 0) == 0) {
                std::string cVal = body.substr(8);
                if (!cVal.empty()) {
                    res.camelot = cVal;
                }
            } else if (body.rfind("openkey:", 0) == 0) {
                std::string oVal = body.substr(8);
                if (!oVal.empty()) {
                    res.openKey = oVal;
                }
            } else if (body.rfind("genre:", 0) == 0) {
                std::string gVal = body.substr(6);
                if (!gVal.empty()) {
                    res.genre = gVal;
                }
            } else if (body.rfind("mood:", 0) == 0) {
                std::string mVal = body.substr(5);
                if (!mVal.empty()) {
                    res.mood = mVal;
                }
            } else {
                res.tags.push_back(body);
            }
        } else {
            freeWords.push_back(token);
        }
    }

    res.keywords = freeWords;
    std::ostringstream ss;
    for (size_t i = 0; i < freeWords.size(); ++i) {
        if (i > 0) ss << " ";
        ss << freeWords[i];
    }
    res.freeText = ss.str();
    return res;
}

db::QueryFilter ParsedQuery::toDbFilter(int limit, int offset) const {
    db::QueryFilter f;
    f.text = freeText;
    f.minBpm = minBpm;
    f.maxBpm = maxBpm;
    f.keyRoot = keyRoot;
    f.keyMode = keyMode;
    f.camelot = camelot;
    f.genre = genre;
    f.mood = mood;
    if (!tags.empty()) {
        f.userTag = tags[0];
    }
    f.limit = limit;
    f.offset = offset;
    return f;
}

std::string ParsedQuery::toSqlWhere() const {
    std::vector<std::string> clauses;
    if (minBpm > 0.0f) {
        clauses.push_back("bpm >= " + std::to_string(minBpm));
    }
    if (maxBpm > 0.0f) {
        clauses.push_back("bpm <= " + std::to_string(maxBpm));
    }
    if (!keyRoot.empty()) {
        clauses.push_back("key_root = '" + keyRoot + "'");
    }
    if (!keyMode.empty()) {
        clauses.push_back("key_mode = '" + keyMode + "'");
    }
    if (!camelot.empty()) {
        clauses.push_back("camelot = '" + camelot + "'");
    }
    if (!genre.empty()) {
        clauses.push_back("genre LIKE '%" + genre + "%'");
    }
    if (!mood.empty()) {
        clauses.push_back("mood LIKE '%" + mood + "%'");
    }
    if (!freeText.empty()) {
        clauses.push_back("(filename LIKE '%" + freeText + "%' OR path LIKE '%" + freeText + "%')");
    }

    if (clauses.empty()) return "";

    std::ostringstream ss;
    for (size_t i = 0; i < clauses.size(); ++i) {
        if (i > 0) ss << " AND ";
        ss << clauses[i];
    }
    return ss.str();
}

bool ParsedQuery::hasFilters() const {
    return !tags.empty() || minBpm > 0.0f || maxBpm > 0.0f ||
           !key.empty() || !camelot.empty() || !openKey.empty() ||
           !genre.empty() || !mood.empty() || onlyFavorites;
}

bool ParsedQuery::hasFreeText() const {
    return !freeText.empty();
}

} // namespace reals::search
