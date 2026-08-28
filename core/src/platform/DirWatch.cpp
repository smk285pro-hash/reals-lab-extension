#include "reals/platform/DirWatch.h"
#include "reals/platform/Path.h"

#include <filesystem>
#include "reals/util/Log.h"

#include <chrono>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace reals::platform {

namespace {
constexpr auto kTag = "watch";
}

DirWatch::DirWatch() = default;

DirWatch::~DirWatch() {
    stop();
}

void DirWatch::start(const std::string& dir, Fn onChange) {
    stop();
    m_dir = normalizePath(dir);
    m_cb = std::move(onChange);
    if (m_dir.empty() || !m_cb)
        return;
    m_run.store(true);
    m_th = std::thread([this] { threadMain(); });
}

void DirWatch::stop() {
    m_run.store(false);
#ifdef _WIN32
    if (void* iocp = m_iocp.load())
        PostQueuedCompletionStatus(static_cast<HANDLE>(iocp), 0, 0, nullptr);
#endif
    if (m_th.joinable())
        m_th.join();
#ifdef _WIN32
    m_iocp.store(nullptr);
#endif
    m_cb = nullptr;
    m_dir.clear();
}

void DirWatch::threadMain() {
#ifdef _WIN32
    const std::filesystem::path native = u8path(m_dir);
    const HANDLE dir = CreateFileW(native.c_str(), FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                   nullptr);
    if (dir == INVALID_HANDLE_VALUE) {
        LOG_WARN(kTag, "CreateFileW for watch failed");
        return;
    }
    const HANDLE iocp = CreateIoCompletionPort(dir, nullptr, 1, 1);
    if (!iocp) {
        CloseHandle(dir);
        LOG_WARN(kTag, "CreateIoCompletionPort failed");
        return;
    }
    m_iocp.store(iocp);
    alignas(DWORD) char buf[4096];
    OVERLAPPED ov{};
    const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                         FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE;
    auto arm = [&]() -> bool {
        ov = {};
        return ReadDirectoryChangesW(dir, buf, sizeof(buf), FALSE, filter, nullptr, &ov, nullptr) != 0 ||
               GetLastError() == ERROR_IO_PENDING;
    };
    if (!arm()) {
        m_iocp.store(nullptr);
        CloseHandle(iocp);
        CloseHandle(dir);
        return;
    }
    LOG_INFO(kTag, "watching directory");
    while (m_run.load()) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* pov = nullptr;
        // 200ms poll so stop() unblocks even if the iocp post raced.
        const BOOL ok = GetQueuedCompletionStatus(iocp, &bytes, &key, &pov, 200);
        if (!m_run.load())
            break;
        if (!ok) {
            if (GetLastError() == WAIT_TIMEOUT)
                continue;
            break;
        }
        if (key == 0)
            break;
        if (m_cb)
            m_cb(m_dir);
        if (!arm())
            break;
    }
    CancelIo(dir);
    m_iocp.store(nullptr);
    CloseHandle(iocp);
    CloseHandle(dir);
#else
    while (m_run.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
#endif
}

} // namespace reals::platform
