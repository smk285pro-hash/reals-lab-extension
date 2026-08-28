#pragma once

#include <string_view>

namespace reals::db {

constexpr int kCurrentSchemaVersion = 1;

constexpr std::string_view kCreateMetaTable = R"sql(
CREATE TABLE IF NOT EXISTS meta (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
)sql";

constexpr std::string_view kCreateSamplesTable = R"sql(
CREATE TABLE IF NOT EXISTS samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    path TEXT UNIQUE NOT NULL,
    filename TEXT NOT NULL,
    filesize INTEGER NOT NULL,
    modified_time INTEGER NOT NULL,
    hash TEXT NOT NULL,
    duration_sec REAL DEFAULT 0.0,
    sample_rate INTEGER DEFAULT 0,
    channels INTEGER DEFAULT 0,
    bit_depth INTEGER DEFAULT 16,
    bpm REAL DEFAULT 0.0,
    key_root TEXT DEFAULT '',
    key_mode TEXT DEFAULT '',
    camelot TEXT DEFAULT '',
    genre TEXT DEFAULT '',
    mood TEXT DEFAULT '',
    ai_analyzed INTEGER DEFAULT 0,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
)sql";

constexpr std::string_view kCreateAnalysisTable = R"sql(
CREATE TABLE IF NOT EXISTS analysis (
    sample_id INTEGER PRIMARY KEY REFERENCES samples(id) ON DELETE CASCADE,
    tempo_confidence REAL DEFAULT 0.0,
    key_confidence REAL DEFAULT 0.0,
    genre_tags TEXT DEFAULT '[]',
    mood_tags TEXT DEFAULT '[]',
    embedding BLOB,
    analyzed_at INTEGER NOT NULL
);
)sql";

constexpr std::string_view kCreateUserTagsTable = R"sql(
CREATE TABLE IF NOT EXISTS user_tags (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sample_id INTEGER NOT NULL REFERENCES samples(id) ON DELETE CASCADE,
    tag TEXT NOT NULL,
    UNIQUE(sample_id, tag)
);
)sql";

constexpr std::string_view kCreateIndices = R"sql(
CREATE INDEX IF NOT EXISTS idx_samples_path ON samples(path);
CREATE INDEX IF NOT EXISTS idx_samples_hash ON samples(hash);
CREATE INDEX IF NOT EXISTS idx_samples_bpm ON samples(bpm);
CREATE INDEX IF NOT EXISTS idx_samples_key ON samples(key_root, key_mode);
CREATE INDEX IF NOT EXISTS idx_samples_camelot ON samples(camelot);
CREATE INDEX IF NOT EXISTS idx_samples_analyzed ON samples(ai_analyzed);
CREATE INDEX IF NOT EXISTS idx_user_tags_sample ON user_tags(sample_id);
CREATE INDEX IF NOT EXISTS idx_user_tags_tag ON user_tags(tag);
)sql";

} // namespace reals::db
