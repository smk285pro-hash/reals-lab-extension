#include "reals/browser/BrowserModel.h"

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace reals::browser {

namespace {
constexpr auto kTag = "browser";
constexpr size_t kMaxRecents = 20;

// Unicode-aware lowercase via the platform layer (MAJ-01 — no direct Win32
// calls in core modules).
std::string lowerUtf8(const std::string& s) {
    return platform::toLowerUtf8(s);
}

inline bool isAudioExtRaw(std::string_view extLower) {
    static const std::unordered_set<std::string_view> kAudio = {
        "wav", "wave", "mp3", "flac", "ogg", "oga", "aiff", "aif", "wma", "m4a", "aac", "opus",
        "mid", "midi", "w64", "caf", "sfz", "rex", "rx2"};
    return kAudio.count(extLower) > 0;
}

inline bool isMediaExtRaw(std::string_view extLower) {
    if (isAudioExtRaw(extLower))
        return true;
    static const std::unordered_set<std::string_view> kMedia = {
        "mp4", "mkv", "mov", "avi", "webm", "wmv", "rpp", "rtracktemplate", "rfxchain"};
    return kMedia.count(extLower) > 0;
}

inline bool matchMediaExt(std::string_view name, std::string& outExtLower, bool& outIsAudio) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos)
        return false;
    std::string_view extView = name.substr(dot + 1);
    if (extView.empty() || extView.size() > 16)
        return false;
    char buf[18];
    for (size_t i = 0; i < extView.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(extView[i]);
        buf[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
    std::string_view lowerExtView(buf, extView.size());
    outIsAudio = isAudioExtRaw(lowerExtView);
    if (outIsAudio || isMediaExtRaw(lowerExtView)) {
        outExtLower.assign(buf, extView.size());
        return true;
    }
    return false;
}

inline std::string toLowerAscii(std::string_view s) {
    std::string out;
    out.resize(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        out[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
    return out;
}

inline std::string fastLower(std::string_view s) {
    bool hasNonAscii = false;
    for (unsigned char c : s) {
        if (c >= 128) {
            hasNonAscii = true;
            break;
        }
    }
    if (!hasNonAscii) {
        std::string out;
        out.resize(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            out[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
        }
        return out;
    }
    return lowerUtf8(std::string(s));
}

bool entryLess(const FileEntry& a, const FileEntry& b, BrowserModel::Sort sort) {
    if (a.isDir != b.isDir)
        return a.isDir; // folders first
    switch (sort) {
    case BrowserModel::Sort::Size:
        return a.sizeBytes != b.sizeBytes ? a.sizeBytes > b.sizeBytes : a.lowerName < b.lowerName;
    case BrowserModel::Sort::Date:
        return a.modifiedEpoch != b.modifiedEpoch ? a.modifiedEpoch > b.modifiedEpoch
                                                  : a.lowerName < b.lowerName;
    case BrowserModel::Sort::Name:
    default:
        return a.lowerName < b.lowerName;
    }
}

long long toUnixEpoch(const fs::file_time_type& ftime) {
    const auto duration = ftime.time_since_epoch();
    const auto s = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
#if defined(_WIN32) && defined(_MSC_VER)
    if (s >= 11644473600LL)
        return s - 11644473600LL;
    return s;
#else
    const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    return std::chrono::duration_cast<std::chrono::seconds>(sysTime.time_since_epoch()).count();
#endif
}

FileEntry makeEntry(const fs::directory_entry& e, std::string_view precomputedName = {}, std::string_view precomputedPath = {}) {
    FileEntry fe;
    std::error_code ec;
    fe.name = precomputedName.empty() ? platform::pathToUtf8(e.path().filename()) : std::string(precomputedName);
    fe.lowerName = fastLower(fe.name);
    fe.path = precomputedPath.empty() ? platform::normalizePath(platform::pathToUtf8(e.path())) : std::string(precomputedPath);
    fe.isDir = e.is_directory(ec);
    if (!fe.isDir) {
        matchMediaExt(fe.name, fe.ext, fe.isAudio);
        fe.sizeBytes = static_cast<unsigned long long>(e.file_size(ec));
    }
    const auto ftime = e.last_write_time(ec);
    if (!ec)
        fe.modifiedEpoch = toUnixEpoch(ftime);
    return fe;
}

bool isIgnoredDir(std::string_view name) {
    const std::string lower = fastLower(name);
    static const std::unordered_set<std::string_view> kIgnored = {
        ".git", ".svn", ".hg", "node_modules", "$recycle.bin", "system volume information",
        ".vscode", ".idea", "__pycache__", ".trash", ".reals", "appdata", "application data",
        "windows", "program files", "program files (x86)", "programdata"
    };
    return kIgnored.count(lower) > 0 || (lower.rfind('.', 0) == 0 && lower.length() > 1 && lower != ".");
}
} // namespace

BrowserModel::BrowserModel(std::string storePath) {
    if (!storePath.empty()) {
        m_storePath = std::move(storePath);
    } else {
        m_storePath = platform::joinPath(platform::dataDir(), "browser_store.json");
    }
    loadStore();
}

void BrowserModel::setStorePath(std::string storePath) {
    const std::lock_guard lock(m_storeMutex);
    m_storePath = std::move(storePath);
    loadStore();
}

bool BrowserModel::isAudioExt(const std::string& fileName) {
    std::string ext;
    bool isAudio = false;
    if (matchMediaExt(fileName, ext, isAudio))
        return isAudio;
    return false;
}

bool BrowserModel::isMediaExt(const std::string& fileName) {
    std::string ext;
    bool isAudio = false;
    return matchMediaExt(fileName, ext, isAudio);
}

std::string BrowserModel::formatSize(const unsigned long long bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f GB", static_cast<double>(bytes) / (1024.0 * 1048576));
    else if (bytes >= 1024ull * 1024)
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / 1048576.0);
    else if (bytes >= 1024ull)
        std::snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B", bytes);
    return buf;
}

std::string BrowserModel::formatTime(const long long epochSeconds) {
    const std::time_t t = static_cast<std::time_t>(epochSeconds);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &tm);
    return buf;
}

void BrowserModel::loadStore() {
    const std::lock_guard lock(m_storeMutex);
    std::ifstream in(platform::u8path(m_storePath));
    if (!in)
        return;
    try {
        nlohmann::json j;
        in >> j;
        if (j.contains("favorites"))
            m_favorites = j["favorites"].get<std::vector<std::string>>();
        if (j.contains("recents"))
            m_recents = j["recents"].get<std::deque<std::string>>();
        if (j.contains("tags"))
            for (auto it = j["tags"].begin(); it != j["tags"].end(); ++it)
                m_tags[it.key()] = it.value().get<int>();
        if (j.contains("roots")) {
            m_roots.clear();
            for (const auto& r : j["roots"])
                m_roots.push_back({r.value("name", ""), r.value("path", "")});
        }
    } catch (const std::exception&) {
        LOG_WARN(kTag, "store parse failed — starting fresh");
    }
}

void BrowserModel::saveStore() const {
    const std::lock_guard lock(m_storeMutex);
    nlohmann::json j;
    j["favorites"] = m_favorites;
    j["recents"] = m_recents;
    j["tags"] = m_tags;
    nlohmann::json roots = nlohmann::json::array();
    for (const auto& r : m_roots)
        roots.push_back({{"name", r.name}, {"path", r.path}});
    j["roots"] = roots;
    const std::string tmpPath = m_storePath + ".tmp";
    {
        std::ofstream out(platform::u8path(tmpPath));
        if (!out) return;
        out << j.dump(2);
        if (!out) return;
    }
    std::error_code ec;
    fs::rename(platform::u8path(tmpPath), platform::u8path(m_storePath), ec);
    if (ec) {
        std::error_code ec2;
        fs::remove(platform::u8path(m_storePath), ec2);
        fs::rename(platform::u8path(tmpPath), platform::u8path(m_storePath), ec2);
    }
}

bool BrowserModel::addRoot(const std::string& name, const std::string& path) {
    const std::lock_guard lock(m_storeMutex);
    const std::string normPath = platform::normalizePath(path);
#ifdef _WIN32
    const std::string lowerNorm = lowerUtf8(normPath);
    for (const auto& r : m_roots)
        if (lowerUtf8(platform::normalizePath(r.path)) == lowerNorm)
            return false;
#else
    for (const auto& r : m_roots)
        if (r.path == normPath)
            return false;
#endif
    m_roots.push_back({name, normPath});
    saveStore();
    return true;
}

void BrowserModel::removeRoot(const size_t index) {
    const std::lock_guard lock(m_storeMutex);
    if (index < m_roots.size()) {
        m_roots.erase(m_roots.begin() + static_cast<long>(index));
        saveStore();
    }
}

std::vector<FileEntry>& BrowserModel::buildListing(const std::string& dir) {
    std::vector<FileEntry> list;
    constexpr size_t kMaxFiles = 5000;
    constexpr int kMaxDepth = 6;
    auto entries = platform::scanDirectoryRecursive(dir, kMaxDepth, kMaxFiles);
    if (!entries.empty()) {
        list.reserve(entries.size());
        for (auto& raw : entries) {
            if (raw.isDirectory)
                continue;
            std::string extLower;
            bool isAudio = false;
            if (!matchMediaExt(raw.name, extLower, isAudio))
                continue;

            FileEntry fe;
            fe.name = std::move(raw.name);
            fe.lowerName = std::move(raw.lowerName);
            fe.path = std::move(raw.fullPath);
            fe.ext = std::move(extLower);
            fe.isAudio = isAudio;
            fe.isDir = false;
            fe.sizeBytes = raw.sizeBytes;
            fe.modifiedEpoch = raw.modifiedEpoch;
            list.push_back(std::move(fe));
        }
        std::sort(list.begin(), list.end(),
                  [this](const FileEntry& a, const FileEntry& b) { return entryLess(a, b, m_sort); });
    }
    return m_cache[dir] = std::move(list);
}

std::vector<FileEntry> BrowserModel::listDir(const std::string& dir) {
    const std::lock_guard lock(m_storeMutex);
    const auto it = m_cache.find(dir);
    if (it != m_cache.end())
        return it->second;
    return buildListing(dir);
}

void BrowserModel::invalidate(const std::string& dir) {
    const std::lock_guard lock(m_storeMutex);
    m_cache.erase(dir);
}

void BrowserModel::invalidateAll() {
    const std::lock_guard lock(m_storeMutex);
    m_cache.clear();
}

bool BrowserModel::isFavorite(const std::string& path) const {
    return std::find(m_favorites.begin(), m_favorites.end(), path) != m_favorites.end();
}

void BrowserModel::toggleFavorite(const std::string& path) {
    const std::lock_guard lock(m_storeMutex);
    const auto it = std::find(m_favorites.begin(), m_favorites.end(), path);
    if (it != m_favorites.end())
        m_favorites.erase(it);
    else
        m_favorites.push_back(path);
    saveStore();
}

std::vector<FileEntry> BrowserModel::getFavoriteEntries() const {
    const std::lock_guard lock(m_storeMutex);
    std::vector<FileEntry> entries;
    entries.reserve(m_favorites.size());
    for (const auto& path : m_favorites) {
        std::error_code ec;
        auto u8p = platform::u8path(path);
        if (fs::exists(u8p, ec) && !fs::is_directory(u8p, ec)) {
            fs::directory_entry de(u8p, ec);
            if (!ec) {
                entries.push_back(makeEntry(de));
            }
        }
    }
    std::sort(entries.begin(), entries.end(),
              [this](const FileEntry& a, const FileEntry& b) { return entryLess(a, b, m_sort); });
    return entries;
}

void BrowserModel::addRecent(const std::string& path) {
    const std::lock_guard lock(m_storeMutex);
    const auto it = std::find(m_recents.begin(), m_recents.end(), path);
    if (it != m_recents.end())
        m_recents.erase(it);
    m_recents.push_front(path);
    if (m_recents.size() > kMaxRecents)
        m_recents.pop_back();
    saveStore();
}

void BrowserModel::clearRecents() {
    const std::lock_guard lock(m_storeMutex);
    m_recents.clear();
    saveStore();
}

int BrowserModel::tagOf(const std::string& path) const {
    const auto it = m_tags.find(path);
    return it != m_tags.end() ? it->second : 0;
}

void BrowserModel::setTag(const std::string& path, const int colorIndex) {
    const std::lock_guard lock(m_storeMutex);
    if (colorIndex <= 0)
        m_tags.erase(path);
    else
        m_tags[path] = colorIndex;
    saveStore();
}

void BrowserModel::rewritePath(const std::string& from, const std::string& to) {
    const std::lock_guard lock(m_storeMutex);
    bool dirty = false;
    auto isChildOrSelf = [&](const std::string& p) -> bool {
        if (p == from) return true;
#ifdef _WIN32
        std::string lp = lowerUtf8(platform::normalizePath(p));
        std::string lf = lowerUtf8(platform::normalizePath(from));
        if (lp == lf) return true;
        if (lp.size() > lf.size() && lp.rfind(lf, 0) == 0) {
            char sep = lp[lf.size()];
            return sep == '/' || sep == '\\';
        }
        return false;
#else
        std::string np = platform::normalizePath(p);
        std::string nf = platform::normalizePath(from);
        if (np == nf) return true;
        if (np.size() > nf.size() && np.rfind(nf, 0) == 0) {
            char sep = np[nf.size()];
            return sep == '/' || sep == '\\';
        }
        return false;
#endif
    };
    auto replacePrefix = [&](std::string& p) {
        if (p == from) {
            p = to;
            return true;
        }
        std::string np = platform::normalizePath(p);
        std::string nf = platform::normalizePath(from);
        std::string nt = platform::normalizePath(to);
#ifdef _WIN32
        std::string lp = lowerUtf8(np);
        std::string lf = lowerUtf8(nf);
        if (lp.size() > lf.size() && lp.rfind(lf, 0) == 0 && (lp[lf.size()] == '/' || lp[lf.size()] == '\\')) {
            std::string suffix = p.substr(from.size());
            // Handle case where from may have different slash style, use original p's suffix
            // Find the actual prefix length in original p that corresponds to from
            // For simplicity, use normalized version's suffix
            std::string normSuffix = np.substr(nf.size());
            p = to + normSuffix;
            return true;
        }
#else
        if (np.size() > nf.size() && np.rfind(nf, 0) == 0 && (np[nf.size()] == '/' || np[nf.size()] == '\\')) {
            std::string suffix = np.substr(nf.size());
            p = nt + suffix;
            return true;
        }
#endif
        return false;
    };

    for (auto& f : m_favorites) {
        if (isChildOrSelf(f)) {
            if (replacePrefix(f)) dirty = true;
        }
    }
    for (auto& r : m_recents) {
        if (isChildOrSelf(r)) {
            if (replacePrefix(r)) dirty = true;
        }
    }
    // Tags: need to handle keys that are child paths
    std::vector<std::pair<std::string,int>> toAdd;
    std::vector<std::string> toRemove;
    for (const auto& [k, v] : m_tags) {
        if (isChildOrSelf(k)) {
            std::string newKey = k;
            if (replacePrefix(newKey)) {
                toAdd.emplace_back(newKey, v);
                toRemove.push_back(k);
                dirty = true;
            }
        }
    }
    for (auto& k : toRemove) m_tags.erase(k);
    for (auto& kv : toAdd) m_tags[kv.first] = kv.second;

    if (dirty)
        saveStore();
}

void BrowserModel::forgetPath(const std::string& path) {
    const std::lock_guard lock(m_storeMutex);
    bool dirty = false;
    auto isChildOrSelf = [&](const std::string& p) -> bool {
        if (p == path) return true;
#ifdef _WIN32
        std::string lp = lowerUtf8(platform::normalizePath(p));
        std::string lf = lowerUtf8(platform::normalizePath(path));
        if (lp == lf) return true;
        if (lp.size() > lf.size() && lp.rfind(lf, 0) == 0) {
            char sep = lp[lf.size()];
            return sep == '/' || sep == '\\';
        }
        return false;
#else
        std::string np = platform::normalizePath(p);
        std::string nf = platform::normalizePath(path);
        if (np == nf) return true;
        if (np.size() > nf.size() && np.rfind(nf, 0) == 0) {
            char sep = np[nf.size()];
            return sep == '/' || sep == '\\';
        }
        return false;
#endif
    };

    auto oldFavSize = m_favorites.size();
    m_favorites.erase(std::remove_if(m_favorites.begin(), m_favorites.end(), isChildOrSelf), m_favorites.end());
    if (m_favorites.size() != oldFavSize) dirty = true;

    auto oldRecSize = m_recents.size();
    m_recents.erase(std::remove_if(m_recents.begin(), m_recents.end(), isChildOrSelf), m_recents.end());
    if (m_recents.size() != oldRecSize) dirty = true;

    std::vector<std::string> tagToRemove;
    for (const auto& [k, v] : m_tags) {
        if (isChildOrSelf(k)) tagToRemove.push_back(k);
    }
    for (auto& k : tagToRemove) {
        m_tags.erase(k);
        dirty = true;
    }

    if (dirty)
        saveStore();
}

std::vector<FileEntry> BrowserModel::search(const std::string& base, const std::string& query,
                                             const bool audioOnly, const size_t maxResults,
                                             const std::atomic<bool>* cancel) const {
    std::vector<FileEntry> results;
    const std::string q = lowerUtf8(query);
    if (q.empty())
        return results;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(platform::u8path(base),
                                             fs::directory_options::skip_permission_denied, ec),
         end;
         it != end; it.increment(ec)) {
        if (cancel && cancel->load())
            break;
        if (results.size() >= maxResults)
            break;
        std::error_code ec2;
        if (it->is_symlink(ec2))
            continue;
        if (it->is_directory(ec2)) {
            const std::string dname = platform::pathToUtf8(it->path().filename());
            if (isIgnoredDir(dname)) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file(ec2))
            continue;
        const auto& itemPath = it->path();
        const std::string name = platform::pathToUtf8(itemPath.filename());
        if (lowerUtf8(name).find(q) == std::string::npos)
            continue;
        const std::string fullPath = platform::normalizePath(platform::pathToUtf8(itemPath));
        FileEntry fe = makeEntry(*it, name, fullPath);
        if (audioOnly && !fe.isAudio)
            continue;
        results.push_back(std::move(fe));
    }
    // Sort results consistently with listDir (folders first, then name)
    std::sort(results.begin(), results.end(),
              [this](const FileEntry& a, const FileEntry& b) { return entryLess(a, b, m_sort); });
    return results;
}

} // namespace reals::browser
