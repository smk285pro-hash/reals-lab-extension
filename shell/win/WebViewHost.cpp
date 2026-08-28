#include "WebViewHost.h"

#include <windows.h>

#include <wrl.h>

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <fstream>

using namespace Microsoft::WRL;

namespace reals::shell {

namespace {
constexpr auto kTag = "webview";
constexpr wchar_t kVirtualHost[] = L"app.local";
constexpr wchar_t kEntryPoint[] = L"https://app.local/index.html";

std::string toNarrow(const wchar_t* wide) {
    if (!wide || !*wide)
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, s.data(), n, nullptr, nullptr);
    s.pop_back();
    return s;
}
std::wstring toWide(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    w.pop_back();
    return w;
}

// Fingerprint of the mapped ui-web folder. Customers keep the disk cache
// across launches; we only wipe it when the UI actually changed.
std::wstring uiFingerprint(const std::wstring& sourceFolder) {
    WIN32_FILE_ATTRIBUTE_DATA index{};
    WIN32_FILE_ATTRIBUTE_DATA css{};
    WIN32_FILE_ATTRIBUTE_DATA js{};
    GetFileAttributesExW((sourceFolder + L"\\index.html").c_str(), GetFileExInfoStandard, &index);
    GetFileAttributesExW((sourceFolder + L"\\app.css").c_str(), GetFileExInfoStandard, &css);
    GetFileAttributesExW((sourceFolder + L"\\app.js").c_str(), GetFileExInfoStandard, &js);
    wchar_t buf[192];
    std::swprintf(buf, 192, L"%08lX%08lX-%08lX%08lX-%08lX%08lX-%08lX-%08lX-%08lX",
                  index.ftLastWriteTime.dwHighDateTime, index.ftLastWriteTime.dwLowDateTime,
                  css.ftLastWriteTime.dwHighDateTime, css.ftLastWriteTime.dwLowDateTime,
                  js.ftLastWriteTime.dwHighDateTime, js.ftLastWriteTime.dwLowDateTime,
                  index.nFileSizeLow, css.nFileSizeLow, js.nFileSizeLow);
    return buf;
}

std::wstring stampPath() {
    return toWide(reals::platform::joinPath(reals::platform::dataDir(), "ui-cache.stamp"));
}

bool uiChangedSinceLastLaunch(const std::wstring& sourceFolder) {
    const std::wstring now = uiFingerprint(sourceFolder);
    const std::wstring path = stampPath();
    std::wstring prev;
    {
        std::ifstream in{std::filesystem::path(path)};
        if (in) {
            std::string line;
            std::getline(in, line);
            prev = toWide(line);
        }
    }
    const bool firstLaunch = prev.empty();
    if (!firstLaunch && prev == now)
        return false;
    std::ofstream out{std::filesystem::path(path), std::ios::trunc};
    if (out)
        out << toNarrow(now.c_str());
    // First launch: disk cache is empty, skip the wipe.
    return !firstLaunch;
}
} // namespace

struct WebViewHost::Impl {
    HWND hwnd = nullptr;
    ComPtr<ICoreWebView2Environment> environment;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> web;
    EventRegistrationToken messageToken{};
    EventRegistrationToken navToken{};
    bool messageHandlerAdded = false;
    DWORD navStartTick = 0;
    std::function<void(const std::string&)> onMessage;
    std::function<void(const std::string&)> onNavigateLog;

    ~Impl() {
        if (web) {
            if (messageToken.value != 0) {
                web->remove_WebMessageReceived(messageToken);
                messageToken.value = 0;
            }
            if (navToken.value != 0) {
                web->remove_NavigationCompleted(navToken);
                navToken.value = 0;
            }
        }
        if (controller) {
            controller->Close();
            controller.Reset();
        }
        if (web) {
            web.Reset();
        }
    }

    void attachHandler() {
        if (!web || messageHandlerAdded || !onMessage)
            return;
        messageHandlerAdded = true;
        web->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                    LPWSTR raw = nullptr;
                    if (args->TryGetWebMessageAsString(&raw) == S_OK && raw) {
                        if (onMessage)
                            onMessage(toNarrow(raw));
                        CoTaskMemFree(raw);
                    } else if (args->get_WebMessageAsJson(&raw) == S_OK && raw) {
                        if (onMessage)
                            onMessage(toNarrow(raw));
                        CoTaskMemFree(raw);
                    }
                    return S_OK;
                })
                .Get(),
            &messageToken);
    }
};

WebViewHost::WebViewHost() : m_impl(std::make_unique<Impl>()) {}
WebViewHost::~WebViewHost() = default;

void WebViewHost::create(HWND hwnd, const std::wstring& userDataFolder,
                         const std::wstring& sourceFolder,
                         std::function<void(bool ok)> onReady) {
    if (!m_impl)
        m_impl = std::make_unique<Impl>();
    m_impl->hwnd = hwnd;

    {
        const std::wstring check = sourceFolder + L"\\index.html";
        const bool exists =
            GetFileAttributesW(check.c_str()) != INVALID_FILE_ATTRIBUTES;
        char msg[MAX_PATH + 96];
        std::snprintf(msg, sizeof(msg), "ui-web dir: %ls (index.html %s)", sourceFolder.c_str(),
                      exists ? "FOUND" : "MISSING!");
        LOG_INFO(kTag, msg);
    }

    auto options = Make<CoreWebView2EnvironmentOptions>();
    options->put_AdditionalBrowserArguments(L"--disable-http-cache");
    const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), options.Get(),
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, sourceFolder, onReady](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    char msg[96];
                    std::snprintf(msg, sizeof(msg),
                                  "environment creation failed (hr 0x%08lX) — WebView2 Runtime "
                                  "missing? https://developer.microsoft.com/microsoft-edge/webview2",
                                  static_cast<unsigned long>(result));
                    LOG_ERROR(kTag, msg);
                    onReady(false);
                    return S_OK;
                }
                m_impl->environment = env;
                LOG_INFO(kTag, "environment ready");

                env->CreateCoreWebView2Controller(
                    m_impl->hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, sourceFolder, onReady](HRESULT result,
                                                      ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) {
                                char msg[96];
                                std::snprintf(msg, sizeof(msg),
                                              "controller creation failed (hr 0x%08lX)",
                                              static_cast<unsigned long>(result));
                                LOG_ERROR(kTag, msg);
                                onReady(false);
                                return S_OK;
                            }
                            m_impl->controller = controller;
                            controller->get_CoreWebView2(&m_impl->web);

                            // Match --bg-app so the first paint is not a black flash.
                            ComPtr<ICoreWebView2Controller2> controller2;
                            if (SUCCEEDED(m_impl->controller.As(&controller2)) && controller2) {
                                const COREWEBVIEW2_COLOR bg{0, 0, 0, 0};
                                controller2->put_DefaultBackgroundColor(bg);
                            }

                            // Default TRUE: WebView2 child HWND eats Explorer drops and
                            // JS never sees folder paths. Host IDropTarget handles CF_HDROP.
                            ComPtr<ICoreWebView2Controller4> controller4;
                            if (SUCCEEDED(m_impl->controller.As(&controller4)) && controller4)
                                controller4->put_AllowExternalDrop(FALSE);

                            // Map ui-web folder to https://app.local
                            ComPtr<ICoreWebView2_3> web3;
                            if (SUCCEEDED(m_impl->web.As(&web3)) && web3) {
                                const HRESULT hrMap = web3->SetVirtualHostNameToFolderMapping(
                                    kVirtualHost, sourceFolder.c_str(),
                                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                char msg[96];
                                std::snprintf(msg, sizeof(msg),
                                              "virtual host mapping hr 0x%08lX",
                                              static_cast<unsigned long>(hrMap));
                                LOG_INFO(kTag, msg);
                            } else {
                                LOG_ERROR(kTag, "ICoreWebView2_3 not available (old runtime)");
                            }

                            // Customer path: keep the disk cache. Wipe only when ui-web
                            // actually changed since the last launch (stamp in %APPDATA%).
                            if (uiChangedSinceLastLaunch(sourceFolder)) {
                                LOG_INFO(kTag, "ui-web changed — clearing disk cache");
                                ComPtr<ICoreWebView2_13> web13;
                                ComPtr<ICoreWebView2Profile> profile;
                                ComPtr<ICoreWebView2Profile2> profile2;
                                if (SUCCEEDED(m_impl->web.As(&web13)) && web13 &&
                                    SUCCEEDED(web13->get_Profile(&profile)) && profile &&
                                    SUCCEEDED(profile.As(&profile2)) && profile2) {
                                    profile2->ClearBrowsingData(
                                        static_cast<COREWEBVIEW2_BROWSING_DATA_KINDS>(
                                            COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE |
                                            COREWEBVIEW2_BROWSING_DATA_KINDS_CACHE_STORAGE |
                                            COREWEBVIEW2_BROWSING_DATA_KINDS_ALL_DOM_STORAGE),
                                        nullptr);
                                }
                            } else {
                                LOG_INFO(kTag, "ui-web unchanged — keeping disk cache");
                            }

                            // Navigation diagnostics (ms since Navigate).
                            m_impl->web->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs*
                                               args) -> HRESULT {
                                        BOOL success = FALSE;
                                        args->get_IsSuccess(&success);
                                        LPWSTR uri = nullptr;
                                        m_impl->web->get_Source(&uri);
                                        const DWORD elapsed =
                                            m_impl->navStartTick
                                                ? (GetTickCount() - m_impl->navStartTick)
                                                : 0;
                                        char msg[512];
                                        std::snprintf(msg, sizeof(msg),
                                                      "navigation %s in %lums: %ls",
                                                      success ? "OK" : "FAILED",
                                                      static_cast<unsigned long>(elapsed),
                                                      uri ? uri : L"?");
                                        LOG_INFO(kTag, msg);
                                        if (uri)
                                            CoTaskMemFree(uri);
                                        return S_OK;
                                    })
                                    .Get(),
                                &m_impl->navToken);

                            RECT rc{};
                            GetClientRect(m_impl->hwnd, &rc);
                            m_impl->controller->put_Bounds(rc);
                            // Hidden until the host asks to show — lets REAPER load
                            // pre-warm the environment without a flash.
                            m_impl->controller->put_IsVisible(FALSE);

                            m_impl->attachHandler();
                            m_impl->navStartTick = GetTickCount();
                            m_impl->web->Navigate(kEntryPoint);
                            m_ready = true;
                            LOG_INFO(kTag, "webview ready");
                            onReady(true);
                            return S_OK;
                        })
                        .Get());
                return S_OK;
            })
            .Get());

    if (FAILED(hr)) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "CreateCoreWebView2EnvironmentWithOptions failed (hr 0x%08lX)",
                      static_cast<unsigned long>(hr));
        LOG_ERROR(kTag, msg);
        onReady(false);
    }
}

void WebViewHost::resize(const LONG width, const LONG height) {
    if (m_impl && m_impl->controller) {
        RECT rc{0, 0, width, height};
        m_impl->controller->put_Bounds(rc);
    }
}

void WebViewHost::setVisible(const bool visible) {
    if (m_impl && m_impl->controller)
        m_impl->controller->put_IsVisible(visible ? TRUE : FALSE);
}

void WebViewHost::postJson(const std::string& json) {
    if (m_impl && m_impl->web) {
        const std::wstring w = toWide(json);
        m_impl->web->PostWebMessageAsJson(w.c_str());
    }
}

void WebViewHost::setWebMessageHandler(std::function<void(const std::string&)> handler) {
    if (!m_impl)
        m_impl = std::make_unique<Impl>();
    m_impl->onMessage = std::move(handler);

    // The handler may be set before the webview exists — attach when ready via
    // a deferred check from the caller, or attach now if already available.
    m_impl->attachHandler();
}


} // namespace reals::shell
