#pragma once

// WebView2 hosting for a Win32 window (Windows shell).
// Loads the local ui-web/ folder via a virtual host and exchanges JSON
// messages with the page (WebMessageReceived / PostWebMessageAsJson).
#ifdef _WIN32

#include <windows.h>

#include <functional>
#include <memory>
#include <string>

namespace reals::shell {

class WebViewHost {
public:
    WebViewHost();
    ~WebViewHost();

    // Non-blocking: COM callbacks complete on the calling thread's message pump.
    void create(HWND hwnd, const std::wstring& userDataFolder,
                const std::wstring& sourceFolder, // folder mapped to https://app.local
                std::function<void(bool ok)> onReady);

    void resize(LONG width, LONG height);
    void setVisible(bool visible);
    void postJson(const std::string& json);
    void postString(const std::string& str);
    void executeScript(const std::wstring& script,
                       std::function<void(const std::string&)> onComplete = nullptr);
    void setWebMessageHandler(std::function<void(const std::string&)> handler);

    [[nodiscard]] bool isReady() const { return m_ready; }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_ready = false;
};

} // namespace reals::shell

#endif // _WIN32
