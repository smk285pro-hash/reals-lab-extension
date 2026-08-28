#include "reals/platform/Path.h"

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
    const std::wstring w = p.wstring();
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    s.pop_back();
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

} // namespace reals::platform
