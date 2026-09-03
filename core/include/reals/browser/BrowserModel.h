#pragma once

// Data model for the local file browser: roots, lazy folder listing, favorites,
// recents, color tags and search. Persisted as JSON in the data dir.
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace reals::browser {

struct FileEntry {
    std::string name;
    std::string lowerName;
    std::string path;
    std::string ext;     // lowercase, without dot
    unsigned long long sizeBytes = 0;
    long long modifiedEpoch = 0;
    bool isAudio = false;
    bool isDir = false;
    float bpm = 0.0f;
    std::string key;
    std::string camelot;
    double durationSec = 0.0;
};

class BrowserModel {
public:
    struct Root {
        std::string name;
        std::string path;
    };

    enum class Sort { Name, Size, Date };

    explicit BrowserModel(std::string storePath = {});

    void setStorePath(std::string storePath);
    void loadStore();
    void saveStore() const;

    // --- Roots ---------------------------------------------------------------
    // Snapshot getters: copy under m_storeMutex instead of returning references
    // to internals (MAJ-06) — a reader holding the old reference across a
    // concurrent mutation would race / dangle.
    [[nodiscard]] std::vector<Root> roots() const {
        const std::lock_guard lock(m_storeMutex);
        return m_roots;
    }
    // Returns false if `path` is already a root (no-op).
    bool addRoot(const std::string& name, const std::string& path);
    void removeRoot(size_t index);

    // --- Listing (cached) ------------------------------------------------------
    // Returns a snapshot of a directory listing (refreshes cache if stale).
    std::vector<FileEntry> listDir(const std::string& dir);
    void invalidate(const std::string& dir);
    void invalidateAll();

    // --- Favorites / recents / tags -------------------------------------------
    [[nodiscard]] bool isFavorite(const std::string& path) const;
    void toggleFavorite(const std::string& path);
    [[nodiscard]] std::vector<std::string> favorites() const {
        const std::lock_guard lock(m_storeMutex);
        return m_favorites;
    }

    // Resolves and returns full FileEntry metadata for all favorited files across
    // all roots and subdirectories. Non-existent or pruned files are skipped.
    [[nodiscard]] std::vector<FileEntry> getFavoriteEntries() const;

    [[nodiscard]] std::deque<std::string> recents() const {
        const std::lock_guard lock(m_storeMutex);
        return m_recents;
    }
    void addRecent(const std::string& path);
    void clearRecents();

    // 0 = none, 1..7 = palette color
    [[nodiscard]] int tagOf(const std::string& path) const;
    void setTag(const std::string& path, int colorIndex);
    [[nodiscard]] std::unordered_map<std::string, int> tags() const {
        const std::lock_guard lock(m_storeMutex);
        return m_tags;
    }

    // Keep favorites/tags/recents consistent after a rename or delete.
    void rewritePath(const std::string& from, const std::string& to);
    void forgetPath(const std::string& path);

    // --- Search ---------------------------------------------------------------
    // Case-insensitive name search under `base` (recursive), capped results.
    // `cancel` may be null. When set, the walk aborts as soon as it is true.
    std::vector<FileEntry> search(const std::string& base, const std::string& query,
                                  bool audioOnly, size_t maxResults,
                                  const std::atomic<bool>* cancel) const;
    std::vector<FileEntry> search(const std::string& base, const std::string& query,
                                  bool audioOnly, size_t maxResults = 400) const {
        return search(base, query, audioOnly, maxResults, nullptr);
    }

    // --- Sorting / filtering ----------------------------------------------------
    void setSort(Sort sort) { m_sort = sort; }
    [[nodiscard]] Sort sort() const { return m_sort; }

    [[nodiscard]] static bool isAudioExt(const std::string& fileName);
    [[nodiscard]] static bool isMediaExt(const std::string& fileName);

    [[nodiscard]] static std::string formatSize(unsigned long long bytes);
    [[nodiscard]] static std::string formatTime(long long epochSeconds);

private:
    std::vector<FileEntry>& buildListing(const std::string& dir);

    std::vector<Root> m_roots;
    std::unordered_map<std::string, std::vector<FileEntry>> m_cache;
    std::vector<std::string> m_favorites;
    std::deque<std::string> m_recents;
    std::unordered_map<std::string, int> m_tags; // path -> color index 0..7
    Sort m_sort = Sort::Name;
    std::string m_storePath;
    mutable std::recursive_mutex m_storeMutex;
};

} // namespace reals::browser
