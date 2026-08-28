#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace reals::db {

// Core sample metadata record matching the SQLite `samples` table.
struct SampleRecord {
    int64_t id = 0;
    std::string path;
    std::string filename;
    uint64_t filesize = 0;
    int64_t modifiedTime = 0;
    std::string hash;
    double durationSec = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitDepth = 16;
    double bpm = 0.0;
    std::string keyRoot;     // e.g. "C", "F#", "Ab"
    std::string keyMode;     // "major", "minor"
    std::string camelot;     // "8A", "11B", etc.
    std::string genre;       // Top genre label
    std::string mood;        // Top mood label
    bool aiAnalyzed = false;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

// AI analysis metadata record matching the SQLite `analysis` table.
struct AnalysisRecord {
    int64_t sampleId = 0;
    double tempoConfidence = 0.0;
    double keyConfidence = 0.0;
    std::vector<std::string> genreTags; // Top genre predictions with confidence
    std::vector<std::string> moodTags;  // Multi-label mood tags
    std::vector<float> embedding;       // 512-dim float vector (2048 bytes) for CLAP search
    int64_t analyzedAt = 0;
};

// Query filter options for filtering samples in the database.
struct QueryFilter {
    std::string text;                   // Substring search in filename or path
    std::string genre;                  // Specific genre match
    std::string mood;                   // Specific mood match
    std::string keyRoot;                // Specific key root (e.g. "F#")
    std::string keyMode;                // Specific mode ("major", "minor")
    std::string camelot;                // Specific camelot key (e.g. "8A")
    double minBpm = 0.0;                // Minimum BPM filter (0 = no min)
    double maxBpm = 0.0;                // Maximum BPM filter (0 = no max)
    std::optional<bool> aiAnalyzed;     // Filter by AI analysis state
    std::string userTag;                // Filter by assigned user tag
    int limit = 100;                    // Max results
    int offset = 0;                     // Result pagination offset
};

} // namespace reals::db
