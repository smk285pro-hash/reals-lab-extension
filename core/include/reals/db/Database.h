#pragma once

#include "reals/db/SampleRecord.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct sqlite3;

namespace reals::db {

class Database {
public:
    // RAII Transaction helper
    class Transaction {
    public:
        explicit Transaction(Database& db);
        ~Transaction();

        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&&) noexcept;
        Transaction& operator=(Transaction&&) noexcept;

        bool commit();
        void rollback();

    private:
        Database* m_db = nullptr;
        bool m_committed = false;
    };

    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;

    // Open database. If dbPath is empty, opens %APPDATA%/RealsLab/library.db.
    // Supports ":memory:" for fast in-memory test databases.
    bool open(const std::string& dbPath = "");
    void close();
    [[nodiscard]] bool isOpen() const;
    [[nodiscard]] std::string dbPath() const;

    // Transaction primitives
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    [[nodiscard]] Transaction makeTransaction();

    // Sample record operations
    // Returns the sample's ID (newly inserted or existing/updated ID), or -1 on error.
    int64_t upsertSample(const SampleRecord& rec);
    [[nodiscard]] std::optional<SampleRecord> getSampleById(int64_t id);
    [[nodiscard]] std::optional<SampleRecord> getSampleByPath(const std::string& path);
    [[nodiscard]] std::unordered_map<std::string, SampleRecord> getSamplesByPaths(const std::vector<std::string>& paths);
    [[nodiscard]] std::optional<SampleRecord> getSampleByHash(const std::string& hash);
    bool deleteSample(int64_t id);
    bool deleteSampleByPath(const std::string& path);

    // AI Analysis operations (with 512-dim float vector BLOB serialization)
    bool updateAnalysis(int64_t sampleId, const AnalysisRecord& analysis);
    [[nodiscard]] std::optional<AnalysisRecord> getAnalysis(int64_t sampleId);

    // Retrieve all sample embeddings for fast in-memory SIMD cosine semantic search (M4)
    [[nodiscard]] std::vector<std::pair<int64_t, std::vector<float>>> getAllEmbeddings();

    // User tags operations
    bool addUserTag(int64_t sampleId, const std::string& tag);
    bool removeUserTag(int64_t sampleId, const std::string& tag);
    [[nodiscard]] std::vector<std::string> getUserTags(int64_t sampleId);

    // Search and querying
    [[nodiscard]] std::vector<SampleRecord> querySamples(const QueryFilter& filter = {});

    // Utility & stats
    [[nodiscard]] size_t getSampleCount();
    [[nodiscard]] size_t getAnalyzedCount();
    [[nodiscard]] std::vector<std::pair<int64_t, std::string>> getAllSamplePaths();

    // Execute raw SQL statement (for maintenance/testing)
    bool executeRaw(const std::string& sql);

private:
    bool initSchema();
    // Close without taking m_mutex — the caller must already hold it.
    // Used by open() and the move-assignment, which lock m_mutex themselves.
    void closeLocked();

    sqlite3* m_db = nullptr;
    std::string m_path;
    mutable std::mutex m_mutex;
};

} // namespace reals::db
