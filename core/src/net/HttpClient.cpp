// HTTP client implementation (Windows transport: WinHTTP).
// The ONLY place in the project allowed to touch network APIs directly
// (AGENTS.md architecture rules). Other OSes: add a sibling implementation.
//
// Fixes over the previous inline LabApi transport:
//  - WinHttpCrackUrl uses pre-allocated buffers (no leaked GlobalAlloc blocks).
//  - Query string is preserved (extra-info appended to the path).
//  - File access uses wide APIs -> UTF-8 paths (Vietnamese names) work.
//  - downloadToFile streams to disk instead of buffering up to 2 GB in RAM.
#include "reals/net/HttpClient.h"

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

namespace reals::net {

namespace {

constexpr wchar_t kUserAgent[] = L"RealsLab/0.2";
constexpr size_t kMaxResponseBody = 64u << 20; // safety cap for in-memory bodies

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    w.pop_back();
    return w;
}

std::string fileNameOf(const std::string& path) {
    const size_t p = path.find_last_of("\\/");
    return p == std::string::npos ? path : path.substr(p + 1);
}

struct UrlParts {
    std::wstring host;
    std::wstring path; // path + query
    bool https = true;
};

// Pre-allocated component buffers: nothing for WinHTTP to allocate, so there
// is nothing to GlobalFree afterwards. Extra info (= "?query") is appended so
// callers may pass full URLs with parameters.
bool crackUrl(const std::string& url, UrlParts& out) {
    wchar_t hostBuf[512] = {};
    wchar_t pathBuf[2048] = {};
    wchar_t extraBuf[2048] = {};
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = static_cast<DWORD>(sizeof(hostBuf) / sizeof(hostBuf[0]));
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = static_cast<DWORD>(sizeof(pathBuf) / sizeof(pathBuf[0]));
    uc.lpszExtraInfo = extraBuf;
    uc.dwExtraInfoLength = static_cast<DWORD>(sizeof(extraBuf) / sizeof(extraBuf[0]));
    const std::wstring w = toWide(url);
    if (w.empty() || !WinHttpCrackUrl(w.c_str(), 0, 0, &uc))
        return false;
    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    out.path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    out.path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    out.https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    if (out.path.empty())
        out.path = L"/";
    return true;
}

void readResponse(HINTERNET hRequest, Response& res,
                  const std::function<bool(size_t, size_t)>& onProgress,
                  std::ostream* sink = nullptr) {
    DWORD status = 0, statusSize = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize,
                        WINHTTP_NO_HEADER_INDEX);
    res.statusCode = static_cast<long>(status);

    wchar_t ctBuf[256] = {};
    DWORD ctSize = sizeof(ctBuf);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_TYPE, WINHTTP_HEADER_NAME_BY_INDEX,
                            ctBuf, &ctSize, WINHTTP_NO_HEADER_INDEX)) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, ctBuf, -1, nullptr, 0, nullptr, nullptr);
        std::string ct(static_cast<size_t>(n), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ctBuf, -1, ct.data(), n, nullptr, nullptr);
        ct.pop_back();
        res.headers["Content-Type"] = ct;
    }

    DWORD totalKnown = 0;
    DWORD totalSize = sizeof(totalKnown);
    const bool hasTotal =
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &totalKnown, &totalSize,
                            WINHTTP_NO_HEADER_INDEX);

    size_t received = 0;
    DWORD avail = 0;
    for (;;) {
        if (!WinHttpQueryDataAvailable(hRequest, &avail) || avail == 0)
            break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, chunk.data(), avail, &read))
            break;
        if (read > 0) {
            received += read;
            if (sink)
                sink->write(chunk.data(), static_cast<std::streamsize>(read));
            else
                res.body.append(chunk, 0, read);
        }
        if (onProgress && !onProgress(received, hasTotal ? totalKnown : 0)) {
            res.error = "cancelled";
            return;
        }
        if (!sink && res.body.size() > kMaxResponseBody) {
            res.error = "response too large";
            return;
        }
    }
}

} // namespace

struct HttpClient::Impl {
    std::mutex mutex;
    HINTERNET session = nullptr;
    std::string baseUrl;
    std::string authToken;

    ~Impl() {
        if (session)
            WinHttpCloseHandle(session);
    }

    HINTERNET acquireSession() {
        const std::lock_guard lock(mutex);
        if (!session) {
            session = WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (session)
                WinHttpSetTimeouts(session, 15000, 30000, 600000, 600000);
        }
        return session;
    }

    std::string resolveUrl(const std::string& url) const {
        if (!url.empty() && url.front() == '/' && !baseUrl.empty())
            return baseUrl + url;
        return url;
    }
};

HttpClient::HttpClient() : m_impl(new Impl()) {}

HttpClient::~HttpClient() { delete m_impl; }

HttpClient& HttpClient::instance() {
    static HttpClient inst;
    return inst;
}

void HttpClient::setBaseUrl(std::string_view url) { m_impl->baseUrl = std::string(url); }

void HttpClient::setAuthToken(std::string_view token) { m_impl->authToken = std::string(token); }

Response HttpClient::send(const Request& req) {
    Response res;
    UrlParts up;
    if (!crackUrl(m_impl->resolveUrl(req.url), up)) {
        res.error = "bad url";
        return res;
    }

    HINTERNET session = m_impl->acquireSession();
    if (!session) {
        res.error = "no session";
        return res;
    }
    HINTERNET hConnect =
        WinHttpConnect(session, up.host.c_str(),
                       up.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        res.error = "connect failed";
        return res;
    }

    std::wostringstream hh;
    for (const auto& [k, v] : req.headers)
        hh << toWide(k) << L": " << toWide(v) << L"\r\n";
    if (!m_impl->authToken.empty() && req.headers.find("Authorization") == req.headers.end())
        hh << L"Authorization: Bearer " << toWide(m_impl->authToken) << L"\r\n";
    hh << L"Content-Type: application/json\r\n";
    const std::wstring headers = hh.str();

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, toWide(req.method).c_str(), up.path.c_str(), nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, up.https ? WINHTTP_FLAG_SECURE : 0);

    if (hRequest) {
        const std::string& body = req.body;
        BOOL sent =
            WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.size()),
                               WINHTTP_NO_REQUEST_DATA, 0, static_cast<DWORD>(body.size()), 0);
        if (sent && !body.empty()) {
            DWORD written = 0;
            sent = WinHttpWriteData(hRequest, body.data(), static_cast<DWORD>(body.size()),
                                    &written) &&
                   written == body.size();
        }
        if (sent && WinHttpReceiveResponse(hRequest, nullptr))
            readResponse(hRequest, res, req.onProgress);
        else if (res.error.empty())
            res.error = "request failed";
        WinHttpCloseHandle(hRequest);
    } else {
        res.error = "open request failed";
    }

    WinHttpCloseHandle(hConnect);
    return res;
}

long HttpClient::downloadToFile(const std::string& url, const std::string& destPath,
                                const std::function<bool(size_t, size_t)>& onProgress) {
    UrlParts up;
    if (!crackUrl(m_impl->resolveUrl(url), up))
        return 0;

    HINTERNET session = m_impl->acquireSession();
    if (!session)
        return 0;
    HINTERNET hConnect =
        WinHttpConnect(session, up.host.c_str(),
                       up.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect)
        return 0;

    long status = 0;
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"GET", up.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, up.https ? WINHTTP_FLAG_SECURE : 0);
    if (hRequest) {
        std::wostringstream hh;
        if (!m_impl->authToken.empty())
            hh << L"Authorization: Bearer " << toWide(m_impl->authToken) << L"\r\n";
        const std::wstring headers = hh.str();

        BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(),
                                       static_cast<DWORD>(headers.size()),
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
            DWORD statusDw = 0, statusSize = sizeof(statusDw);
            WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &statusDw, &statusSize,
                                WINHTTP_NO_HEADER_INDEX);
            if (statusDw >= 200 && statusDw < 300) {
                std::error_code ec;
                // UTF-8 safe path handling: fs::path(std::string) would go
                // through the ANSI code page on Windows and break Vietnamese
                // destination folders.
                const fs::path dest = platform::u8path(destPath);
                const fs::path parent = dest.parent_path();
                if (!parent.empty())
                    fs::create_directories(parent, ec);
                std::ofstream out(dest, std::ios::binary);
                if (out) {
                    Response res;
                    readResponse(hRequest, res, onProgress, &out);
                    out.flush();
                    status = (res.error.empty() && out.good()) ? res.statusCode : 0;
                }
            } else {
                status = static_cast<long>(statusDw);
            }
        }
        WinHttpCloseHandle(hRequest);
    }

    WinHttpCloseHandle(hConnect);
    return status;
}

Response HttpClient::uploadFile(const std::string& url, const std::string& fieldName,
                                const std::string& filePath,
                                const std::map<std::string, std::string>& extraFields) {
    Response res;
    UrlParts up;
    if (!crackUrl(m_impl->resolveUrl(url), up)) {
        res.error = "bad url";
        return res;
    }

    HANDLE hFile = CreateFileW(toWide(filePath).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        res.error = "cannot open file";
        return res;
    }

    LARGE_INTEGER li{};
    if (!GetFileSizeEx(hFile, &li)) {
        CloseHandle(hFile);
        res.error = "cannot stat file";
        return res;
    }
    const unsigned long long fileBytes = static_cast<unsigned long long>(li.QuadPart);

    constexpr const char* kBoundary = "----RealsLabBoundary7MA4YWxkTrZu0gW";
    std::ostringstream head;
    head << "--" << kBoundary << "\r\nContent-Disposition: form-data; name=\"" << fieldName
         << "\"; filename=\"" << fileNameOf(filePath)
         << "\"\r\nContent-Type: application/octet-stream\r\n\r\n";
    const std::string headPart = head.str();

    std::string extraPart;
    for (const auto& [k, v] : extraFields) {
        extraPart += "\r\n--" + std::string(kBoundary) +
                     "\r\nContent-Disposition: form-data; name=\"" + k + "\"\r\n\r\n" + v;
    }
    const std::string footPart = "\r\n--" + std::string(kBoundary) + "--\r\n";
    const unsigned long long totalLen =
        headPart.size() + extraPart.size() + fileBytes + footPart.size();

    HINTERNET session = m_impl->acquireSession();
    if (!session) {
        CloseHandle(hFile);
        res.error = "no session";
        return res;
    }
    HINTERNET hConnect =
        WinHttpConnect(session, up.host.c_str(),
                       up.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) {
        CloseHandle(hFile);
        res.error = "connect failed";
        return res;
    }

    std::wostringstream hh;
    hh << L"Content-Type: multipart/form-data; boundary=" << toWide(kBoundary) << L"\r\n";
    if (!m_impl->authToken.empty())
        hh << L"Authorization: Bearer " << toWide(m_impl->authToken) << L"\r\n";
    const std::wstring headers = hh.str();

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", up.path.c_str(), nullptr,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            up.https ? WINHTTP_FLAG_SECURE : 0);

    if (hRequest) {
        BOOL sent =
            WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.size()),
                               WINHTTP_NO_REQUEST_DATA, 0, static_cast<DWORD>(totalLen), 0);

        auto writeAll = [&](const void* data, DWORD len) -> BOOL {
            if (len == 0)
                return TRUE;
            DWORD written = 0;
            return WinHttpWriteData(hRequest, data, len, &written) && written == len ? TRUE : FALSE;
        };

        if (sent)
            sent = writeAll(headPart.data(), static_cast<DWORD>(headPart.size()));
        if (sent) {
            char chunk[1 << 20];
            DWORD read = 0;
            while (sent && ReadFile(hFile, chunk, sizeof(chunk), &read, nullptr) && read > 0)
                sent = writeAll(chunk, read);
        }
        if (sent)
            sent = writeAll(extraPart.data(), static_cast<DWORD>(extraPart.size()));
        if (sent)
            sent = writeAll(footPart.data(), static_cast<DWORD>(footPart.size()));

        if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
            const Request dummy;
            readResponse(hRequest, res, dummy.onProgress);
        } else if (res.error.empty()) {
            res.error = "upload failed";
        }
        WinHttpCloseHandle(hRequest);
    } else {
        res.error = "open request failed";
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hConnect);
    return res;
}

} // namespace reals::net
