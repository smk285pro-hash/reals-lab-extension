# Milestone 3: SQLite Database & Background Scanner Pool — SCOPE

## Objective
Implement Features 11, 12, 13 for Reals Lab with zero build warnings, C++20 compliance, clean layered architecture, and complete unit test coverage.

---

## 1. Feature Breakdown & Deliverables

### Feature 11: SQLite Library Database (`core/db/`, `libs/sqlite3/`)
- **Header/Amalgamation**:
  - `libs/sqlite3/sqlite3.h`
  - `libs/sqlite3/sqlite3.c`
  - Integrated into CMake as a clean C library or part of `reals_core` (MSVC `/utf-8`, `-DSQLITE_THREADSAFE=1`, `-DSQLITE_ENABLE_FTS5=1`).
- **Data Models** (`core/include/reals/db/`):
  - `SampleRecord.h`: `SampleRecord` struct (id, path, filename, filesize, modifiedTime, hash, durationSec, sampleRate, channels, bitDepth, bpm, keyRoot, keyMode, camelot, genre, mood, aiAnalyzed, createdAt, updatedAt), `AnalysisRecord` struct (sampleId, tempoConfidence, keyConfidence, genreTags, moodTags, embedding [std::vector<float>], analyzedAt), `QueryFilter` struct (text, genre, mood, keyRoot, keyMode, minBpm, maxBpm, aiAnalyzedOnly, userTag, limit, offset).
  - `Schema.h`: SQL schema DDL (`CREATE TABLE IF NOT EXISTS samples ...`, `CREATE TABLE IF NOT EXISTS analysis ...`, `CREATE TABLE IF NOT EXISTS user_tags ...`, index creation, pragmas: `PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA foreign_keys=ON;`).
  - `Database.h` / `Database.cpp`: `reals::db::Database` class.
    - Lifecycle: `open(const std::string& dbPath)`, `close()`, `isOpen() const`.
    - Auto-migration / initialization of tables and indices.
    - Sample CRUD: `upsertSample`, `getSampleById`, `getSampleByPath`, `getSampleByHash`, `deleteSample`, `deleteSampleByPath`, `getAllSamplePaths`, `getSampleCount`, `getAnalyzedCount`.
    - Analysis CRUD: `updateAnalysis`, `getAnalysis`, `getAllEmbeddings` (reads 512-dim float embeddings from BLOB).
    - User Tags: `addUserTag`, `removeUserTag`, `getUserTags`, `getTagsForSample`.
    - Filtered Queries: `querySamples(const QueryFilter& filter)`.
    - Transactions: RAII `Transaction` helper class, `beginTransaction`, `commitTransaction`, `rollbackTransaction`.
    - Thread-safety / Connection isolation: Safe prepared statements with auto-reset/finalize.

### Feature 12: File Hash Checksum Cache (`reals::util::Hash`)
- **Files**: `core/include/reals/util/Hash.h`, `core/src/util/Hash.cpp`.
- **Implementation**:
  - `xxHash64`: ultra-fast 64-bit non-cryptographic hash (designed for sample libraries, processing gigabytes/sec).
  - `sha256`: cryptographic 256-bit hash for model verification or high-assurance hashing.
  - Streaming file hashing: `hashFileXx64(const std::string& path, size_t maxBytesToRead = 0) -> std::string` (hex string or uint64).
  - Buffer hashing: `hashBufferXx64(const void* data, size_t size) -> uint64_t` / `hashBufferXx64Hex(...) -> std::string`.
  - Checksum metadata comparison: `isSameFile(const std::string& path, uint64_t cachedSize, int64_t cachedModTime, const std::string& cachedHash) -> bool`.

### Feature 13: Multi-Threaded Background Scanner (`reals::scanner::BackgroundScanner`)
- **Files**: `core/include/reals/scanner/BackgroundScanner.h`, `ScanJob.h`, `core/src/scanner/BackgroundScanner.cpp`.
- **Implementation**:
  - Multi-threaded worker pool: configurable thread count (default: hardware concurrency or 4).
  - Directory traversal: recursive scanning with path filtering (skips hidden folders, non-audio files, `.git`, `node_modules`, recycle bin).
  - Audio file inspection: extracts sample rate, channels, duration, bit depth using `miniaudio` or header parsing.
  - Database caching: checks file size + modified time; if unchanged, skips file hashing; if size/mtime changed or new, computes xxHash64 and upserts DB record.
  - Asynchronous event notification: thread-safe progress reporting (`scanner.progress`: total, processed, currentFile, addedCount, updatedCount, skippedCount, isComplete).
  - Full lifecycle controls: `startScan(const std::vector<std::string>& roots, ScanOptions options)`, `pauseScan()`, `resumeScan()`, `cancelScan()`, `waitForCompletion()`.

---

## 2. Unit Testing Suite (`tests/`)
- `tests/test_hash.cpp`: Validates xxHash64 and SHA256 test vectors, buffer vs stream consistency, large buffers, empty files.
- `tests/test_db.cpp`: Tests Database open/close, table creation, CRUD operations, transactions, 512-dim embedding BLOB roundtrip, user tags, query filters, foreign key cascades.
- `tests/test_scanner.cpp`: Tests directory scanning, audio file detection, change detection / skipping unchanged files, progress callback invocation, cancel/pause/resume, concurrency safety.
- Integrated into CTest (`ctest --preset windows`).

---

## 3. Architecture & Code Quality Constraints
- Layered architecture: `core/` contains no UI, no REAPER dependencies, no GLFW.
- Paths strictly through `reals::platform::` (`Path.h`).
- Thread safety: DB writes synchronized, scanner workers decoupled.
- Clean C++20 with zero compiler warnings under `/W4` (MSVC).
