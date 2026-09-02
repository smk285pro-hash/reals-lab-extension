#include "reals/db/Database.h"
#include "reals/db/Schema.h"
#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <sqlite3.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstring>
#include <sstream>

namespace reals::db {

namespace {
constexpr auto kTag = "db";

int64_t currentUnixTime() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

SampleRecord parseSampleRow(sqlite3_stmt* stmt) {
    SampleRecord rec;
    rec.id = sqlite3_column_int64(stmt, 0);

    const auto* pPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    rec.path = pPath ? pPath : "";

    const auto* pFilename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    rec.filename = pFilename ? pFilename : "";

    rec.filesize = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
    rec.modifiedTime = sqlite3_column_int64(stmt, 4);

    const auto* pHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    rec.hash = pHash ? pHash : "";

    rec.durationSec = sqlite3_column_double(stmt, 6);
    rec.sampleRate = sqlite3_column_int(stmt, 7);
    rec.channels = sqlite3_column_int(stmt, 8);
    rec.bitDepth = sqlite3_column_int(stmt, 9);
    rec.bpm = sqlite3_column_double(stmt, 10);

    const auto* pKeyRoot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
    rec.keyRoot = pKeyRoot ? pKeyRoot : "";

    const auto* pKeyMode = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
    rec.keyMode = pKeyMode ? pKeyMode : "";

    const auto* pCamelot = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 13));
    rec.camelot = pCamelot ? pCamelot : "";

    const auto* pGenre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 14));
    rec.genre = pGenre ? pGenre : "";

    const auto* pMood = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 15));
    rec.mood = pMood ? pMood : "";

    rec.aiAnalyzed = sqlite3_column_int(stmt, 16) != 0;
    rec.createdAt = sqlite3_column_int64(stmt, 17);
    rec.updatedAt = sqlite3_column_int64(stmt, 18);

    return rec;
}

// RAII helper for sqlite3_stmt
struct StmtGuard {
    sqlite3_stmt* stmt = nullptr;
    ~StmtGuard() {
        if (stmt)
            sqlite3_finalize(stmt);
    }
};

} // namespace

// ---- Transaction RAII -------------------------------------------------------

Database::Transaction::Transaction(Database& db)
    : m_db(&db), m_committed(false) {
    if (m_db)
        m_db->beginTransaction();
}

Database::Transaction::~Transaction() {
    if (m_db && !m_committed) {
        m_db->rollbackTransaction();
    }
}

Database::Transaction::Transaction(Transaction&& other) noexcept
    : m_db(other.m_db), m_committed(other.m_committed) {
    other.m_db = nullptr;
    other.m_committed = true;
}

Database::Transaction& Database::Transaction::operator=(Transaction&& other) noexcept {
    if (this != &other) {
        if (m_db && !m_committed) {
            m_db->rollbackTransaction();
        }
        m_db = other.m_db;
        m_committed = other.m_committed;
        other.m_db = nullptr;
        other.m_committed = true;
    }
    return *this;
}

bool Database::Transaction::commit() {
    if (m_db && !m_committed) {
        m_committed = m_db->commitTransaction();
        return m_committed;
    }
    return false;
}

void Database::Transaction::rollback() {
    if (m_db && !m_committed) {
        m_db->rollbackTransaction();
        m_committed = true;
    }
}

// ---- Database Implementation ------------------------------------------------

Database::Database() = default;

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept {
    const std::lock_guard lock(other.m_mutex);
    m_db = other.m_db;
    m_path = std::move(other.m_path);
    other.m_db = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(m_mutex, other.m_mutex);
        closeLocked();
        m_db = other.m_db;
        m_path = std::move(other.m_path);
        other.m_db = nullptr;
    }
    return *this;
}

bool Database::open(const std::string& dbPath) {
    const std::lock_guard lock(m_mutex);
    closeLocked();

    if (dbPath.empty()) {
        const std::string dir = platform::dataDir();
        platform::ensureDir(dir);
        m_path = platform::joinPath(dir, "library.db");
    } else {
        m_path = dbPath;
        if (m_path != ":memory:") {
            const auto parent = platform::u8path(m_path).parent_path();
            if (!parent.empty()) {
                platform::ensureDir(platform::pathToUtf8(parent));
            }
        }
    }

    const int rc = sqlite3_open_v2(m_path.c_str(),
                                  &m_db,
                                  SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                                  nullptr);
    if (rc != SQLITE_OK || !m_db) {
        LOG_ERROR(kTag, "Failed to open sqlite db at: " + m_path + ", error: " + (m_db ? sqlite3_errmsg(m_db) : "null"));
        if (m_db) {
            sqlite3_close(m_db);
            m_db = nullptr;
        }
        return false;
    }

    // Set performance and safety pragmas
    char* errmsg = nullptr;
    sqlite3_busy_timeout(m_db, 5000);
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = nullptr;
    }
    sqlite3_exec(m_db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = nullptr;
    }
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &errmsg);
    if (errmsg) {
        sqlite3_free(errmsg);
        errmsg = nullptr;
    }

    if (!initSchema()) {
        LOG_ERROR(kTag, "Failed to initialize database schema.");
        closeLocked(); // open() already holds m_mutex
        return false;
    }

    LOG_INFO(kTag, "Database opened successfully at: " + m_path);
    return true;
}

void Database::close() {
    // Serialized against queries like every other public method (MAJ-05).
    // Callers that already hold m_mutex (open, move-assign) use closeLocked()
    // instead — a plain lock here would self-deadlock on this non-recursive
    // mutex.
    const std::lock_guard lock(m_mutex);
    closeLocked();
}

void Database::closeLocked() {
    if (m_db) {
        sqlite3_close_v2(m_db);
        m_db = nullptr;
    }
    m_path.clear();
}

bool Database::isOpen() const {
    const std::lock_guard lock(m_mutex);
    return m_db != nullptr;
}

std::string Database::dbPath() const {
    const std::lock_guard lock(m_mutex);
    return m_path;
}

bool Database::initSchema() {
    if (!m_db)
        return false;

    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, kCreateMetaTable.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LOG_ERROR(kTag, "Create meta table error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }
    if (sqlite3_exec(m_db, kCreateSamplesTable.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LOG_ERROR(kTag, "Create samples table error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }
    if (sqlite3_exec(m_db, kCreateAnalysisTable.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LOG_ERROR(kTag, "Create analysis table error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }
    if (sqlite3_exec(m_db, kCreateUserTagsTable.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LOG_ERROR(kTag, "Create user_tags table error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }
    if (sqlite3_exec(m_db, kCreateIndices.data(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        LOG_ERROR(kTag, "Create indices error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }

    // Set or verify schema version
    const std::string insertVer =
        "INSERT OR REPLACE INTO meta (key, value) VALUES ('schema_version', '" +
        std::to_string(kCurrentSchemaVersion) + "');";
    sqlite3_exec(m_db, insertVer.c_str(), nullptr, nullptr, &errmsg);
    if (errmsg)
        sqlite3_free(errmsg);

    return true;
}

bool Database::beginTransaction() {
    const std::lock_guard lock(m_mutex);
    if (!m_db) return false;
    return sqlite3_exec(m_db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::commitTransaction() {
    const std::lock_guard lock(m_mutex);
    if (!m_db) return false;
    return sqlite3_exec(m_db, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::rollbackTransaction() {
    const std::lock_guard lock(m_mutex);
    if (!m_db) return false;
    return sqlite3_exec(m_db, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

Database::Transaction Database::makeTransaction() {
    return Transaction(*this);
}

int64_t Database::upsertSample(const SampleRecord& rec) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return -1;

    const int64_t now = currentUnixTime();
    const int64_t createdAt = rec.createdAt > 0 ? rec.createdAt : now;
    const int64_t updatedAt = now;

    // Check if sample with rec.path already exists
    sqlite3_stmt* checkStmt = nullptr;
    const char* checkSql = "SELECT id, created_at FROM samples WHERE path = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, checkSql, -1, &checkStmt, nullptr) != SQLITE_OK)
        return -1;
    StmtGuard checkGuard{checkStmt};

    sqlite3_bind_text(checkStmt, 1, rec.path.c_str(), -1, SQLITE_STATIC);
    int64_t existingId = -1;
    int64_t existingCreatedAt = createdAt;
    if (sqlite3_step(checkStmt) == SQLITE_ROW) {
        existingId = sqlite3_column_int64(checkStmt, 0);
        existingCreatedAt = sqlite3_column_int64(checkStmt, 1);
    }

    if (existingId > 0) {
        // Update existing record
        sqlite3_stmt* updateStmt = nullptr;
        const char* updateSql = R"sql(
UPDATE samples SET
    filename = ?,
    filesize = ?,
    modified_time = ?,
    hash = ?,
    duration_sec = ?,
    sample_rate = ?,
    channels = ?,
    bit_depth = ?,
    bpm = ?,
    key_root = ?,
    key_mode = ?,
    camelot = ?,
    genre = ?,
    mood = ?,
    ai_analyzed = ?,
    updated_at = ?
WHERE id = ?;
)sql";
        if (sqlite3_prepare_v2(m_db, updateSql, -1, &updateStmt, nullptr) != SQLITE_OK)
            return -1;
        StmtGuard updateGuard{updateStmt};

        sqlite3_bind_text(updateStmt, 1, rec.filename.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(updateStmt, 2, static_cast<int64_t>(rec.filesize));
        sqlite3_bind_int64(updateStmt, 3, rec.modifiedTime);
        sqlite3_bind_text(updateStmt, 4, rec.hash.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_double(updateStmt, 5, rec.durationSec);
        sqlite3_bind_int(updateStmt, 6, rec.sampleRate);
        sqlite3_bind_int(updateStmt, 7, rec.channels);
        sqlite3_bind_int(updateStmt, 8, rec.bitDepth);
        sqlite3_bind_double(updateStmt, 9, rec.bpm);
        sqlite3_bind_text(updateStmt, 10, rec.keyRoot.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(updateStmt, 11, rec.keyMode.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(updateStmt, 12, rec.camelot.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(updateStmt, 13, rec.genre.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(updateStmt, 14, rec.mood.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(updateStmt, 15, rec.aiAnalyzed ? 1 : 0);
        sqlite3_bind_int64(updateStmt, 16, updatedAt);
        sqlite3_bind_int64(updateStmt, 17, existingId);

        if (sqlite3_step(updateStmt) != SQLITE_DONE)
            return -1;

        return existingId;
    }

    // Insert new record
    sqlite3_stmt* insertStmt = nullptr;
    const char* insertSql = R"sql(
INSERT INTO samples (
    path, filename, filesize, modified_time, hash,
    duration_sec, sample_rate, channels, bit_depth,
    bpm, key_root, key_mode, camelot, genre, mood,
    ai_analyzed, created_at, updated_at
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
)sql";
    if (sqlite3_prepare_v2(m_db, insertSql, -1, &insertStmt, nullptr) != SQLITE_OK)
        return -1;
    StmtGuard insertGuard{insertStmt};

    sqlite3_bind_text(insertStmt, 1, rec.path.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 2, rec.filename.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(insertStmt, 3, static_cast<int64_t>(rec.filesize));
    sqlite3_bind_int64(insertStmt, 4, rec.modifiedTime);
    sqlite3_bind_text(insertStmt, 5, rec.hash.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_double(insertStmt, 6, rec.durationSec);
    sqlite3_bind_int(insertStmt, 7, rec.sampleRate);
    sqlite3_bind_int(insertStmt, 8, rec.channels);
    sqlite3_bind_int(insertStmt, 9, rec.bitDepth);
    sqlite3_bind_double(insertStmt, 10, rec.bpm);
    sqlite3_bind_text(insertStmt, 11, rec.keyRoot.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 12, rec.keyMode.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 13, rec.camelot.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 14, rec.genre.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(insertStmt, 15, rec.mood.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(insertStmt, 16, rec.aiAnalyzed ? 1 : 0);
    sqlite3_bind_int64(insertStmt, 17, existingCreatedAt);
    sqlite3_bind_int64(insertStmt, 18, updatedAt);

    if (sqlite3_step(insertStmt) != SQLITE_DONE)
        return -1;

    return sqlite3_last_insert_rowid(m_db);
}

std::optional<SampleRecord> Database::getSampleById(int64_t id) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, path, filename, filesize, modified_time, hash, duration_sec, "
                      "sample_rate, channels, bit_depth, bpm, key_root, key_mode, camelot, genre, "
                      "mood, ai_analyzed, created_at, updated_at FROM samples WHERE id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return parseSampleRow(stmt);
    }
    return std::nullopt;
}

std::optional<SampleRecord> Database::getSampleByPath(const std::string& path) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, path, filename, filesize, modified_time, hash, duration_sec, "
                      "sample_rate, channels, bit_depth, bpm, key_root, key_mode, camelot, genre, "
                      "mood, ai_analyzed, created_at, updated_at FROM samples WHERE path = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;
    StmtGuard guard{stmt};

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return parseSampleRow(stmt);
    }
    return std::nullopt;
}

std::unordered_map<std::string, SampleRecord> Database::getSamplesByPaths(const std::vector<std::string>& paths) {
    std::unordered_map<std::string, SampleRecord> results;
    if (paths.empty()) return results;

    const std::lock_guard lock(m_mutex);
    if (!m_db) return results;

    constexpr size_t kChunkSize = 400;
    for (size_t offset = 0; offset < paths.size(); offset += kChunkSize) {
        size_t count = std::min(kChunkSize, paths.size() - offset);
        if (count == 0) break;

        std::string sql = "SELECT id, path, filename, filesize, modified_time, hash, duration_sec, "
                          "sample_rate, channels, bit_depth, bpm, key_root, key_mode, camelot, genre, "
                          "mood, ai_analyzed, created_at, updated_at FROM samples WHERE path IN (";
        for (size_t i = 0; i < count; ++i) {
            if (i > 0) sql += ",";
            sql += "?";
        }
        sql += ");";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            continue;
        }
        StmtGuard guard{stmt};

        for (size_t i = 0; i < count; ++i) {
            sqlite3_bind_text(stmt, static_cast<int>(i + 1), paths[offset + i].c_str(), -1, SQLITE_STATIC);
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            auto rec = parseSampleRow(stmt);
            results.emplace(rec.path, std::move(rec));
        }
    }
    return results;
}

std::optional<SampleRecord> Database::getSampleByHash(const std::string& hash) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, path, filename, filesize, modified_time, hash, duration_sec, "
                      "sample_rate, channels, bit_depth, bpm, key_root, key_mode, camelot, genre, "
                      "mood, ai_analyzed, created_at, updated_at FROM samples WHERE hash = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;
    StmtGuard guard{stmt};

    sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return parseSampleRow(stmt);
    }
    return std::nullopt;
}

bool Database::deleteSample(int64_t id) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM samples WHERE id = ?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, id);
    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool Database::deleteSampleByPath(const std::string& path) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM samples WHERE path = ?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    StmtGuard guard{stmt};

    sqlite3_bind_text(stmt, 1, path.c_str(), -1, SQLITE_STATIC);
    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool Database::updateAnalysis(int64_t sampleId, const AnalysisRecord& analysis) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return false;

    const int64_t now = analysis.analyzedAt > 0 ? analysis.analyzedAt : currentUnixTime();
    const std::string genreJson = nlohmann::json(analysis.genreTags).dump();
    const std::string moodJson = nlohmann::json(analysis.moodTags).dump();

    sqlite3_stmt* stmt = nullptr;
    const char* sql = R"sql(
INSERT INTO analysis (
    sample_id, tempo_confidence, key_confidence,
    genre_tags, mood_tags, embedding, analyzed_at
) VALUES (?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(sample_id) DO UPDATE SET
    tempo_confidence = excluded.tempo_confidence,
    key_confidence = excluded.key_confidence,
    genre_tags = excluded.genre_tags,
    mood_tags = excluded.mood_tags,
    embedding = excluded.embedding,
    analyzed_at = excluded.analyzed_at;
)sql";

    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, sampleId);
    sqlite3_bind_double(stmt, 2, analysis.tempoConfidence);
    sqlite3_bind_double(stmt, 3, analysis.keyConfidence);
    sqlite3_bind_text(stmt, 4, genreJson.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, moodJson.c_str(), -1, SQLITE_STATIC);

    if (!analysis.embedding.empty()) {
        sqlite3_bind_blob(stmt, 6,
                          analysis.embedding.data(),
                          static_cast<int>(analysis.embedding.size() * sizeof(float)),
                          SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    sqlite3_bind_int64(stmt, 7, now);

    if (sqlite3_step(stmt) != SQLITE_DONE)
        return false;

    // Update ai_analyzed flag on sample record
    sqlite3_stmt* updateSample = nullptr;
    const char* updateSql = "UPDATE samples SET ai_analyzed = 1, updated_at = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(m_db, updateSql, -1, &updateSample, nullptr) == SQLITE_OK) {
        StmtGuard uguard{updateSample};
        sqlite3_bind_int64(updateSample, 1, now);
        sqlite3_bind_int64(updateSample, 2, sampleId);
        sqlite3_step(updateSample);
    }

    return true;
}

std::optional<AnalysisRecord> Database::getAnalysis(int64_t sampleId) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return std::nullopt;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT sample_id, tempo_confidence, key_confidence, genre_tags, mood_tags, "
                      "embedding, analyzed_at FROM analysis WHERE sample_id = ? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, sampleId);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        AnalysisRecord rec;
        rec.sampleId = sqlite3_column_int64(stmt, 0);
        rec.tempoConfidence = sqlite3_column_double(stmt, 1);
        rec.keyConfidence = sqlite3_column_double(stmt, 2);

        const auto* pGenre = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (pGenre) {
            try {
                rec.genreTags = nlohmann::json::parse(pGenre).get<std::vector<std::string>>();
            } catch (...) {
            }
        }

        const auto* pMood = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (pMood) {
            try {
                rec.moodTags = nlohmann::json::parse(pMood).get<std::vector<std::string>>();
            } catch (...) {
            }
        }

        const void* pBlob = sqlite3_column_blob(stmt, 5);
        const int blobBytes = sqlite3_column_bytes(stmt, 5);
        if (pBlob && blobBytes > 0 && (blobBytes % sizeof(float) == 0)) {
            const size_t count = static_cast<size_t>(blobBytes) / sizeof(float);
            const auto* fptr = static_cast<const float*>(pBlob);
            rec.embedding.assign(fptr, fptr + count);
        }

        rec.analyzedAt = sqlite3_column_int64(stmt, 6);
        return rec;
    }

    return std::nullopt;
}

std::vector<std::pair<int64_t, std::vector<float>>> Database::getAllEmbeddings() {
    const std::lock_guard lock(m_mutex);
    std::vector<std::pair<int64_t, std::vector<float>>> results;
    if (!m_db)
        return results;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT sample_id, embedding FROM analysis WHERE embedding IS NOT NULL;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return results;
    StmtGuard guard{stmt};

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t sampleId = sqlite3_column_int64(stmt, 0);
        const void* pBlob = sqlite3_column_blob(stmt, 1);
        const int blobBytes = sqlite3_column_bytes(stmt, 1);
        if (pBlob && blobBytes > 0 && (blobBytes % sizeof(float) == 0)) {
            const size_t count = static_cast<size_t>(blobBytes) / sizeof(float);
            const auto* fptr = static_cast<const float*>(pBlob);
            results.emplace_back(sampleId, std::vector<float>(fptr, fptr + count));
        }
    }

    return results;
}

bool Database::addUserTag(int64_t sampleId, const std::string& tag) {
    const std::lock_guard lock(m_mutex);
    if (!m_db || tag.empty())
        return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR IGNORE INTO user_tags (sample_id, tag) VALUES (?, ?);";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, sampleId);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_STATIC);
    return sqlite3_step(stmt) == SQLITE_DONE;
}

bool Database::removeUserTag(int64_t sampleId, const std::string& tag) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM user_tags WHERE sample_id = ? AND tag = ?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, sampleId);
    sqlite3_bind_text(stmt, 2, tag.c_str(), -1, SQLITE_STATIC);
    return sqlite3_step(stmt) == SQLITE_DONE;
}

std::vector<std::string> Database::getUserTags(int64_t sampleId) {
    const std::lock_guard lock(m_mutex);
    std::vector<std::string> tags;
    if (!m_db)
        return tags;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT tag FROM user_tags WHERE sample_id = ? ORDER BY tag ASC;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return tags;
    StmtGuard guard{stmt};

    sqlite3_bind_int64(stmt, 1, sampleId);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto* pTag = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (pTag)
            tags.emplace_back(pTag);
    }

    return tags;
}

std::vector<SampleRecord> Database::querySamples(const QueryFilter& filter) {
    const std::lock_guard lock(m_mutex);
    std::vector<SampleRecord> results;
    if (!m_db)
        return results;

    std::ostringstream sql;
    sql << "SELECT DISTINCT s.id, s.path, s.filename, s.filesize, s.modified_time, s.hash, "
           "s.duration_sec, s.sample_rate, s.channels, s.bit_depth, s.bpm, s.key_root, s.key_mode, "
           "s.camelot, s.genre, s.mood, s.ai_analyzed, s.created_at, s.updated_at "
           "FROM samples s ";

    if (!filter.userTag.empty()) {
        sql << "JOIN user_tags ut ON ut.sample_id = s.id ";
    }

    std::vector<std::string> clauses;
    if (!filter.text.empty()) {
        clauses.push_back("(s.filename LIKE ? OR s.path LIKE ?)");
    }
    if (!filter.genre.empty()) {
        clauses.push_back("s.genre LIKE ?");
    }
    if (!filter.mood.empty()) {
        clauses.push_back("s.mood LIKE ?");
    }
    if (!filter.keyRoot.empty()) {
        clauses.push_back("s.key_root = ?");
    }
    if (!filter.keyMode.empty()) {
        clauses.push_back("s.key_mode = ?");
    }
    if (!filter.camelot.empty()) {
        clauses.push_back("s.camelot = ?");
    }
    if (filter.minBpm > 0.0) {
        clauses.push_back("s.bpm >= ?");
    }
    if (filter.maxBpm > 0.0) {
        clauses.push_back("s.bpm <= ?");
    }
    if (filter.aiAnalyzed.has_value()) {
        clauses.push_back("s.ai_analyzed = ?");
    }
    if (!filter.userTag.empty()) {
        clauses.push_back("ut.tag = ?");
    }

    if (!clauses.empty()) {
        sql << "WHERE ";
        for (size_t i = 0; i < clauses.size(); ++i) {
            if (i > 0)
                sql << " AND ";
            sql << clauses[i];
        }
    }

    sql << " ORDER BY s.filename COLLATE NOCASE ASC LIMIT ? OFFSET ?;";

    sqlite3_stmt* stmt = nullptr;
    const std::string sqlStr = sql.str();
    if (sqlite3_prepare_v2(m_db, sqlStr.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        LOG_ERROR(kTag, "querySamples prepare error: " + std::string(sqlite3_errmsg(m_db)));
        return results;
    }
    StmtGuard guard{stmt};

    int paramIdx = 1;
    std::string textLike;
    if (!filter.text.empty()) {
        textLike = "%" + filter.text + "%";
        sqlite3_bind_text(stmt, paramIdx++, textLike.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, paramIdx++, textLike.c_str(), -1, SQLITE_STATIC);
    }
    std::string genreLike;
    if (!filter.genre.empty()) {
        genreLike = "%" + filter.genre + "%";
        sqlite3_bind_text(stmt, paramIdx++, genreLike.c_str(), -1, SQLITE_STATIC);
    }
    std::string moodLike;
    if (!filter.mood.empty()) {
        moodLike = "%" + filter.mood + "%";
        sqlite3_bind_text(stmt, paramIdx++, moodLike.c_str(), -1, SQLITE_STATIC);
    }
    if (!filter.keyRoot.empty()) {
        sqlite3_bind_text(stmt, paramIdx++, filter.keyRoot.c_str(), -1, SQLITE_STATIC);
    }
    if (!filter.keyMode.empty()) {
        sqlite3_bind_text(stmt, paramIdx++, filter.keyMode.c_str(), -1, SQLITE_STATIC);
    }
    if (!filter.camelot.empty()) {
        sqlite3_bind_text(stmt, paramIdx++, filter.camelot.c_str(), -1, SQLITE_STATIC);
    }
    if (filter.minBpm > 0.0) {
        sqlite3_bind_double(stmt, paramIdx++, filter.minBpm);
    }
    if (filter.maxBpm > 0.0) {
        sqlite3_bind_double(stmt, paramIdx++, filter.maxBpm);
    }
    if (filter.aiAnalyzed.has_value()) {
        sqlite3_bind_int(stmt, paramIdx++, filter.aiAnalyzed.value() ? 1 : 0);
    }
    if (!filter.userTag.empty()) {
        sqlite3_bind_text(stmt, paramIdx++, filter.userTag.c_str(), -1, SQLITE_STATIC);
    }

    sqlite3_bind_int(stmt, paramIdx++, filter.limit > 0 ? filter.limit : 100);
    sqlite3_bind_int(stmt, paramIdx++, filter.offset >= 0 ? filter.offset : 0);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(parseSampleRow(stmt));
    }

    return results;
}

size_t Database::getSampleCount() {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return 0;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM samples;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    StmtGuard guard{stmt};

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    return 0;
}

size_t Database::getAnalyzedCount() {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return 0;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM samples WHERE ai_analyzed = 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    StmtGuard guard{stmt};

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        return static_cast<size_t>(sqlite3_column_int64(stmt, 0));
    }
    return 0;
}

std::vector<std::pair<int64_t, std::string>> Database::getAllSamplePaths() {
    const std::lock_guard lock(m_mutex);
    std::vector<std::pair<int64_t, std::string>> paths;
    if (!m_db)
        return paths;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, path FROM samples ORDER BY id ASC;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return paths;
    StmtGuard guard{stmt};

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int64_t id = sqlite3_column_int64(stmt, 0);
        const auto* pPath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (pPath) {
            paths.emplace_back(id, pPath);
        }
    }

    return paths;
}

bool Database::executeRaw(const std::string& sql) {
    const std::lock_guard lock(m_mutex);
    if (!m_db)
        return false;
    char* errmsg = nullptr;
    const int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR(kTag, "executeRaw error: " + std::string(errmsg ? errmsg : ""));
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

} // namespace reals::db
