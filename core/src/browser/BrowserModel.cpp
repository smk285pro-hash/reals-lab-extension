#include "reals/browser/BrowserModel.h"

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace reals::browser {

namespace {
constexpr auto kTag = "browser";
constexpr size_t kMaxRecents = 20;

std::string lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string lowerUtf8(const std::string& s) {
#ifdef _WIN32
    if (s.empty()) return s;
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return lower(s);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    for (auto& c : w) c = std::towlower(c);
    int m = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (m <= 1) return lower(s);
    std::string out(static_cast<size_t>(m), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), m, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
#else
    return lower(s);
#endif
}

bool entryLess(const FileEntry& a, const FileEntry& b, BrowserModel::Sort sort) {
    if (a.isDir != b.isDir)
        return a.isDir; // folders first
    switch (sort) {
    case BrowserModel::Sort::Size:
        return a.sizeBytes != b.sizeBytes ? a.sizeBytes > b.sizeBytes : lowerUtf8(a.name) < lowerUtf8(b.name);
    case BrowserModel::Sort::Date:
        return a.modifiedEpoch != b.modifiedEpoch ? a.modifiedEpoch > b.modifiedEpoch
                                                  : lowerUtf8(a.name) < lowerUtf8(b.name);
    case BrowserModel::Sort::Name:
    default:
        return lowerUtf8(a.name) < lowerUtf8(b.name);
    }
}

long long toUnixEpoch(const fs::file_time_type& ftime) {
    const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    return std::chrono::duration_cast<std::chrono::seconds>(sysTime.time_since_epoch()).count();
}

FileEntry makeEntry(const fs::directory_entry& e) {
    FileEntry fe;
    std::error_code ec;
    fe.name = platform::pathToUtf8(e.path().filename());
    fe.path = platform::normalizePath(platform::pathToUtf8(e.path()));
    fe.isDir = e.is_directory(ec);
    if (!fe.isDir) {
        const size_t dot = fe.name.find_last_of('.');
        fe.ext = dot != std::string::npos ? lower(fe.name.substr(dot + 1)) : "";
        fe.isAudio = BrowserModel::isAudioExt(fe.name);
        fe.sizeBytes = static_cast<unsigned long long>(e.file_size(ec));
    }
    const auto ftime = e.last_write_time(ec);
    if (!ec)
        fe.modifiedEpoch = toUnixEpoch(ftime);
    return fe;
}
} // namespace

BrowserModel::BrowserModel() {
    // Default quick-access roots (FL-style). User-added roots persist via store.
    m_roots.push_back({"Music", platform::defaultMusicDir()});
#ifdef _WIN32
    if (const char* up = std::getenv("USERPROFILE")) {
        m_roots.push_back({"Desktop", platform::joinPath(up, "Desktop")});
        m_roots.push_back({"Downloads", platform::joinPath(up, "Downloads")});
    }
#else
    if (const char* home = std::getenv("HOME")) {
        m_roots.push_back({"Desktop", platform::joinPath(home, "Desktop")});
        m_roots.push_back({"Downloads", platform::joinPath(home, "Downloads")});
    }
#endif
    m_storePath = platform::joinPath(platform::dataDir(), "browser_store.json");
}

bool BrowserModel::isAudioExt(const std::string& fileName) {
    static const std::unordered_set<std::string> kAudio = {
        "wav", "wave", "mp3", "flac", "ogg", "oga", "aiff", "aif", "wma", "m4a", "aac", "opus",
        "mid", "midi", "w64", "caf", "sfz", "rex", "rx2"};
    const size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    return kAudio.count(lowerUtf8(fileName.substr(dot + 1))) > 0;
}

bool BrowserModel::isMediaExt(const std::string& fileName) {
    if (isAudioExt(fileName))
        return true;
    static const std::unordered_set<std::string> kMedia = {
        "mp4", "mkv", "mov", "avi", "webm", "wmv", "rpp", "rtracktemplate", "rfxchain"};
    const size_t dot = fileName.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    return kMedia.count(lowerUtf8(fileName.substr(dot + 1))) > 0;
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
    std::error_code ec;
    auto u8dir = platform::u8path(dir);
    if (!fs::exists(u8dir, ec) || ec || !fs::is_directory(u8dir, ec) || ec) {
        static std::vector<FileEntry> empty;
        empty.clear();
        return empty;
    }
    for (const auto& e : fs::directory_iterator(u8dir, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) break;
        std::error_code ec2;
        if (e.is_symlink(ec2))
            continue;
        if (!e.is_regular_file(ec2) && !e.is_directory(ec2))
            continue;
        list.push_back(makeEntry(e));
    }
    if (ec) {
        static std::vector<FileEntry> empty;
        empty.clear();
        return empty;
    }
    std::sort(list.begin(), list.end(),
              [this](const FileEntry& a, const FileEntry& b) { return entryLess(a, b, m_sort); });
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
        if (!it->is_regular_file(ec2))
            continue;
        const std::string name = platform::pathToUtf8(it->path().filename());
        if (lowerUtf8(name).find(q) == std::string::npos)
            continue;
        FileEntry fe = makeEntry(*it);
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
