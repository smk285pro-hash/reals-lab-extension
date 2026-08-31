#include "reals/platform/Path.h"

#include <cctype>
#include <cstdio>
#include <cwctype>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace reals::platform {

std::filesystem::path u8path(std::string_view utf8) {
#ifdef _WIN32
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0)
        return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return std::filesystem::path(w);
#else
    return std::filesystem::path(std::string(utf8));
#endif
}

std::string pathToUtf8(const std::filesystem::path& p) {
#ifdef _WIN32
    const std::wstring& w = p.native();
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
#else
    return p.string();
#endif
}

std::string dataDir() {
#ifdef _WIN32
    if (const char* appdata = std::getenv("APPDATA"))
        return joinPath(appdata, "RealsLab");
    return joinPath(pathToUtf8(fs::temp_directory_path()), "RealsLab");
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"))
        return joinPath(joinPath(home, "Library/Application Support"), "RealsLab");
    return "/tmp/RealsLab";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg)
        return joinPath(xdg, "RealsLab");
    if (const char* home = std::getenv("HOME"))
        return joinPath(joinPath(home, ".config"), "RealsLab");
    return "/tmp/RealsLab";
#endif
}

std::string tempDir() {
    std::error_code ec;
    auto p = fs::temp_directory_path(ec);
    if (!ec)
        return pathToUtf8(p);
#ifdef _WIN32
    if (const char* tmp = std::getenv("TEMP"))
        return std::string(tmp);
    if (const char* tmp2 = std::getenv("TMP"))
        return std::string(tmp2);
    return "C:\\Temp";
#else
    return "/tmp";
#endif
}

std::string defaultMusicDir() {
#ifdef _WIN32
    if (const char* profile = std::getenv("USERPROFILE"))
        return joinPath(profile, "Music");
    return {};
#else
    if (const char* home = std::getenv("HOME"))
        return joinPath(home, "Music");
    return {};
#endif
}

std::string joinPath(std::string_view a, std::string_view b) {
    if (a.empty())
        return std::string(b);
    if (b.empty())
        return std::string(a);
    std::string result(a);
#ifdef _WIN32
    constexpr char sep = '\\';
#else
    constexpr char sep = '/';
#endif
    if (result.back() == sep || b.front() == sep)
        result += std::string(b);
    else
        result += sep + std::string(b);
    return result;
}

std::string normalizePath(std::string_view path) {
    std::string result(path);
#ifdef _WIN32
    for (char& c : result)
        if (c == '/')
            c = '\\';
#else
    for (char& c : result)
        if (c == '\\')
            c = '/';
#endif
    return result;
}

bool ensureDir(std::string_view path) {
    std::error_code ec;
    fs::create_directories(u8path(path), ec);
    return !ec;
}

std::vector<std::string> listSubdirs(std::string_view dir) {
    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(u8path(dir), ec))
        if (entry.is_directory(ec))
            result.push_back(pathToUtf8(entry.path().filename()));
    return result;
}

std::vector<std::string> listFiles(std::string_view dir) {
    std::vector<std::string> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(u8path(dir), ec))
        if (entry.is_regular_file(ec))
            result.push_back(pathToUtf8(entry.path().filename()));
    return result;
}

#ifdef _WIN32
namespace {
bool isIgnoredDirName(std::string_view lowerName) {
    if (lowerName.empty()) return false;
    if (lowerName == ".git" || lowerName == ".svn" || lowerName == ".hg" || lowerName == "node_modules" ||
        lowerName == "$recycle.bin" || lowerName == "system volume information" || lowerName == ".vscode" ||
        lowerName == ".idea" || lowerName == "__pycache__" || lowerName == ".trash" || lowerName == ".reals" ||
        lowerName == "appdata" || lowerName == "application data" || lowerName == "windows" ||
        lowerName == "program files" || lowerName == "program files (x86)" || lowerName == "programdata") {
        return true;
    }
    return (lowerName.rfind('.', 0) == 0 && lowerName.length() > 1 && lowerName != ".");
}

void walkDirWin32Internal(const std::wstring& rootDirW, std::string_view rootDirU8,
                          int depth, int maxDepth, size_t maxFiles,
                          std::vector<DirEntryInfo>& list) {
    if (depth > maxDepth || list.size() >= maxFiles)
        return;

    std::wstring searchPattern = rootDirW;
    if (searchPattern.empty() || (searchPattern.back() != L'\\' && searchPattern.back() != L'/')) {
        searchPattern += L"\\*";
    } else {
        searchPattern += L"*";
    }
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileExW(searchPattern.c_str(), FindExInfoBasic, &fd,
                                   FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    std::vector<std::pair<std::wstring, std::string>> subdirs;
    do {
        if (fd.cFileName[0] == L'.' &&
            (fd.cFileName[1] == L'\0' || (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;

        const int nameLen = static_cast<int>(wcslen(fd.cFileName));
        if (nameLen <= 0)
            continue;

        bool isAscii = true;
        for (int i = 0; i < nameLen; ++i) {
            if (static_cast<unsigned short>(fd.cFileName[i]) >= 128) {
                isAscii = false;
                break;
            }
        }

        std::string name;
        std::string lowerName;
        if (isAscii) {
            name.resize(static_cast<size_t>(nameLen));
            lowerName.resize(static_cast<size_t>(nameLen));
            for (int i = 0; i < nameLen; ++i) {
                const char c = static_cast<char>(fd.cFileName[i]);
                name[i] = c;
                lowerName[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c;
            }
        } else {
            const int u8len = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, nameLen, nullptr, 0, nullptr, nullptr);
            if (u8len <= 0)
                continue;
            name.resize(static_cast<size_t>(u8len));
            WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, nameLen, name.data(), u8len, nullptr, nullptr);
            lowerName = toLowerUtf8(name);
        }

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (depth < maxDepth && !isIgnoredDirName(lowerName)) {
                std::wstring subW = rootDirW;
                if (subW.empty() || (subW.back() != L'\\' && subW.back() != L'/')) {
                    subW += L'\\';
                }
                subW += fd.cFileName;

                std::string subU8;
                if (!rootDirU8.empty()) {
                    subU8.reserve(rootDirU8.size() + 1 + name.size());
                    subU8.append(rootDirU8);
                    if (subU8.back() != '\\' && subU8.back() != '/') {
                        subU8.push_back('\\');
                    }
                    subU8.append(name);
                } else {
                    subU8 = name;
                }
                subdirs.emplace_back(std::move(subW), std::move(subU8));
            }
            continue;
        }

        DirEntryInfo info;
        info.name = std::move(name);
        info.lowerName = std::move(lowerName);
        info.isDirectory = false;

        if (!rootDirU8.empty()) {
            info.fullPath.reserve(rootDirU8.size() + 1 + info.name.size());
            info.fullPath.append(rootDirU8);
            if (info.fullPath.back() != '\\' && info.fullPath.back() != '/') {
                info.fullPath.push_back('\\');
            }
            info.fullPath.append(info.name);
        } else {
            info.fullPath = info.name;
        }

        ULARGE_INTEGER uliSize;
        uliSize.LowPart = fd.nFileSizeLow;
        uliSize.HighPart = fd.nFileSizeHigh;
        info.sizeBytes = uliSize.QuadPart;

        ULARGE_INTEGER uliTime;
        uliTime.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        uliTime.HighPart = fd.ftLastWriteTime.dwHighDateTime;
        if (uliTime.QuadPart >= 116444736000000000ULL) {
            info.modifiedEpoch = static_cast<long long>((uliTime.QuadPart - 116444736000000000ULL) / 10000000ULL);
        }

        list.push_back(std::move(info));
        if (list.size() >= maxFiles)
            break;

    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    for (const auto& [subW, subU8] : subdirs) {
        if (list.size() >= maxFiles)
            break;
        walkDirWin32Internal(subW, subU8, depth + 1, maxDepth, maxFiles, list);
    }
}
} // namespace

std::vector<DirEntryInfo> scanDirectoryRecursive(std::string_view rootDir, int maxDepth, size_t maxFiles) {
    std::vector<DirEntryInfo> list;
    list.reserve(1024);
    std::string normRoot = normalizePath(rootDir);
    std::wstring rootW = u8path(normRoot).wstring();
    if (rootW.empty())
        return list;
    walkDirWin32Internal(rootW, normRoot, 0, maxDepth, maxFiles, list);
    return list;
}
#else
std::vector<DirEntryInfo> scanDirectoryRecursive(std::string_view rootDir, int maxDepth, size_t maxFiles) {
    std::vector<DirEntryInfo> list;
    list.reserve(1024);
    std::error_code ec;
    auto u8dir = u8path(rootDir);
    if (!fs::exists(u8dir, ec) || ec || !fs::is_directory(u8dir, ec) || ec)
        return list;

    for (fs::recursive_directory_iterator it(u8dir, fs::directory_options::skip_permission_denied, ec), end;
         it != end && !ec; it.increment(ec)) {
        if (list.size() >= maxFiles)
            break;
        if (it.depth() > maxDepth) {
            it.disable_recursion_pending();
            continue;
        }
        std::error_code ec2;
        if (it->is_symlink(ec2))
            continue;
        if (it->is_directory(ec2)) {
            const std::string fname = toLowerUtf8(pathToUtf8(it->path().filename()));
            if (fname == ".git" || fname == "node_modules" || fname == ".svn" || fname == ".hg" ||
                fname == "$recycle.bin" || fname == "system volume information" || fname == ".vscode" ||
                fname == ".idea" || fname == "__pycache__" || fname == ".trash" || fname == ".reals" ||
                fname == "appdata" || fname == "application data" || fname == "windows" ||
                fname == "program files" || fname == "program files (x86)" || fname == "programdata" ||
                (fname.rfind(".", 0) == 0 && fname.length() > 1 && fname != ".")) {
                it.disable_recursion_pending();
            }
            continue;
        }
        if (!it->is_regular_file(ec2))
            continue;

        const auto& itemPath = it->path();
        DirEntryInfo info;
        info.name = pathToUtf8(itemPath.filename());
        info.lowerName = toLowerUtf8(info.name);
        info.fullPath = normalizePath(pathToUtf8(itemPath));
        info.isDirectory = false;
        info.sizeBytes = static_cast<unsigned long long>(it->file_size(ec2));
        const auto ftime = it->last_write_time(ec2);
        if (!ec2) {
            const auto duration = ftime.time_since_epoch();
            info.modifiedEpoch = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        }
        list.push_back(std::move(info));
    }
    return list;
}
#endif

std::string toLowerUtf8(std::string_view s) {
#ifdef _WIN32
    if (s.empty())
        return {};
    // Round-trip through UTF-16 so case folding is Unicode-aware (Vietnamese
    // diacritics, Turkish dotless i, etc.).
    const std::wstring w = u8path(s).wstring();
    if (w.empty())
        return std::string(s);
    std::wstring lowered = w;
    for (auto& c : lowered)
        c = std::towlower(c);
    return pathToUtf8(fs::path(lowered));
#else
    // POSIX: byte-wise ASCII lowering (locale-free, same as the previous
    // fallback behavior in BrowserModel).
    std::string result(s);
    for (char& c : result)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
#endif
}

FILE* openAppend(std::string_view utf8Path) {
#ifdef _WIN32
    const std::wstring w = u8path(utf8Path).wstring();
    if (w.empty())
        return nullptr;
    return _wfsopen(w.c_str(), L"a", _SH_DENYNO);
#else
    return std::fopen(std::string(utf8Path).c_str(), "a");
#endif
}

} // namespace reals::platform
