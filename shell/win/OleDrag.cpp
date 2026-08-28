#include "OleDrag.h"

#include "reals/util/Log.h"

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>

namespace reals::shell {

namespace {
constexpr auto kTag = "drag";

class DropSource final : public IDropSource {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override {
        if (!pp)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropSource) {
            *pp = static_cast<IDropSource*>(this);
            AddRef();
            return S_OK;
        }
        *pp = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --m_ref;
        if (!n)
            delete this;
        return n;
    }
    HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
        if (escape)
            return DRAGDROP_S_CANCEL;
        if (!(keys & (MK_LBUTTON | MK_RBUTTON)))
            return DRAGDROP_S_DROP;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }

private:
    ULONG m_ref = 1;
};

class FileDataObject final : public IDataObject {
public:
    explicit FileDataObject(const std::wstring& path) : m_path(path) {
        for (auto& ch : m_path) {
            if (ch == L'/')
                ch = L'\\';
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override {
        if (!pp)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *pp = static_cast<IDataObject*>(this);
            AddRef();
            return S_OK;
        }
        *pp = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --m_ref;
        if (!n)
            delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE GetData(FORMATETC* fmt, STGMEDIUM* med) override {
        if (!fmt || !med)
            return E_POINTER;
        if (!(fmt->tymed & TYMED_HGLOBAL))
            return DV_E_TYMED;

        if (fmt->cfFormat == CF_HDROP) {
            const size_t bytes = sizeof(DROPFILES) + (m_path.size() + 2) * sizeof(wchar_t);
            const HGLOBAL mem = GlobalAlloc(GHND, bytes);
            if (!mem)
                return E_OUTOFMEMORY;
            auto* drop = static_cast<DROPFILES*>(GlobalLock(mem));
            if (!drop) {
                GlobalFree(mem);
                return E_OUTOFMEMORY;
            }
            drop->pFiles = sizeof(DROPFILES);
            drop->fWide = TRUE;
            wchar_t* dest =
                reinterpret_cast<wchar_t*>(reinterpret_cast<char*>(drop) + sizeof(DROPFILES));
            memcpy(dest, m_path.c_str(), (m_path.size() + 1) * sizeof(wchar_t));
            dest[m_path.size() + 1] = L'\0';
            GlobalUnlock(mem);
            med->tymed = TYMED_HGLOBAL;
            med->hGlobal = mem;
            med->pUnkForRelease = nullptr;
            return S_OK;
        }

        if (fmt->cfFormat == CF_UNICODETEXT) {
            const size_t bytes = (m_path.size() + 1) * sizeof(wchar_t);
            const HGLOBAL mem = GlobalAlloc(GHND, bytes);
            if (!mem)
                return E_OUTOFMEMORY;
            auto* dest = static_cast<wchar_t*>(GlobalLock(mem));
            if (!dest) {
                GlobalFree(mem);
                return E_OUTOFMEMORY;
            }
            memcpy(dest, m_path.c_str(), (m_path.size() + 1) * sizeof(wchar_t));
            GlobalUnlock(mem);
            med->tymed = TYMED_HGLOBAL;
            med->hGlobal = mem;
            med->pUnkForRelease = nullptr;
            return S_OK;
        }

        return DV_E_FORMATETC;
    }
    HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* fmt) override {
        if (!fmt)
            return E_POINTER;
        if (fmt->cfFormat != CF_HDROP && fmt->cfFormat != CF_UNICODETEXT)
            return DV_E_FORMATETC;
        if (!(fmt->tymed & TYMED_HGLOBAL))
            return DV_E_TYMED;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override {
        if (out)
            out->ptd = nullptr;
        return DATA_S_SAMEFORMATETC;
    }
    HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** pp) override {
        if (!pp)
            return E_POINTER;
        if (dir != DATADIR_GET)
            return E_NOTIMPL;
        FORMATETC fmt[2] = {{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL},
                            {CF_UNICODETEXT, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}};
        return SHCreateStdEnumFmtEtc(2, fmt, pp);
    }
    HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override {
        return OLE_E_ADVISENOTSUPPORTED;
    }
    HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
    HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }

private:
    std::wstring m_path;
    ULONG m_ref = 1;
};

FORMATETC hdropFormat() {
    return FORMATETC{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
}

FileDropCallback g_dropCb;
FileDropHover g_hoverCb;
std::vector<HWND> g_dropHwnds;
bool g_internalDrag = false;

void setHover(bool on) {
    if (g_hoverCb)
        g_hoverCb(on);
}

class FileDropTarget final : public IDropTarget {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** pp) override {
        if (!pp)
            return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDropTarget) {
            *pp = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        *pp = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --m_ref;
        if (!n)
            delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
        m_ok = !g_internalDrag && data && data->QueryGetData(&m_fmt) == S_OK;
        if (effect)
            *effect = m_ok ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        if (m_ok)
            setHover(true);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override {
        if (effect)
            *effect = m_ok && !g_internalDrag ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DragLeave() override {
        if (m_ok)
            setHover(false);
        m_ok = false;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD, POINTL, DWORD* effect) override {
        if (effect)
            *effect = DROPEFFECT_NONE;
        setHover(false);
        if (g_internalDrag || !m_ok || !data || !g_dropCb) {
            m_ok = false;
            return S_OK;
        }
        STGMEDIUM med{};
        if (FAILED(data->GetData(&m_fmt, &med)) || !med.hGlobal) {
            m_ok = false;
            return S_OK;
        }
        std::vector<std::wstring> paths;
        const HDROP drop = static_cast<HDROP>(med.hGlobal);
        const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < n; ++i) {
            const UINT len = DragQueryFileW(drop, i, nullptr, 0);
            if (!len)
                continue;
            std::wstring p(static_cast<size_t>(len) + 1, L'\0');
            DragQueryFileW(drop, i, p.data(), len + 1);
            p.resize(len);
            if (!p.empty())
                paths.push_back(std::move(p));
        }
        ReleaseStgMedium(&med);
        if (effect && !paths.empty())
            *effect = DROPEFFECT_COPY;
        m_ok = false;
        if (!paths.empty())
            g_dropCb(paths);
        {
            char msg[64];
            std::snprintf(msg, sizeof(msg), "drop %zu path(s)", paths.size());
            LOG_INFO(kTag, msg);
        }
        return S_OK;
    }

private:
    FORMATETC m_fmt = hdropFormat();
    bool m_ok = false;
    ULONG m_ref = 1;
};

BOOL CALLBACK enumRegister(HWND child, LPARAM) {
    registerFileDropTarget(child, {}, {});
    EnumChildWindows(child, enumRegister, 0);
    return TRUE;
}
} // namespace

void beginFileDrag(HWND /*owner*/, const std::wstring& path) {
    if (path.empty())
        return;
    // WebView2 often holds mouse capture; OLE needs it released.
    ReleaseCapture();
    g_internalDrag = true;
    auto* src = new DropSource();
    auto* data = new FileDataObject(path);
    DWORD effect = 0;
    const HRESULT hr =
        DoDragDrop(data, src, DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &effect);
    g_internalDrag = false;
    char msg[96];
    std::snprintf(msg, sizeof(msg), "DoDragDrop hr 0x%08lX effect %lu",
                  static_cast<unsigned long>(hr), static_cast<unsigned long>(effect));
    LOG_INFO(kTag, msg);
    src->Release();
    data->Release();
}

bool registerFileDropTarget(HWND hwnd, FileDropCallback cb, FileDropHover hover) {
    if (!hwnd)
        return false;
    if (cb)
        g_dropCb = std::move(cb);
    if (hover)
        g_hoverCb = std::move(hover);
    if (!g_dropCb)
        return false;
    for (HWND h : g_dropHwnds) {
        if (h == hwnd)
            return true;
    }
    auto* target = new FileDropTarget();
    HRESULT hr = RegisterDragDrop(hwnd, target);
    if (hr == DRAGDROP_E_ALREADYREGISTERED) {
        // WebView2 registers its own target on the child HWND. Steal it so
        // Explorer folder drops reach us (JS cannot read folder paths).
        RevokeDragDrop(hwnd);
        hr = RegisterDragDrop(hwnd, target);
    }
    target->Release();
    if (FAILED(hr)) {
        char msg[80];
        std::snprintf(msg, sizeof(msg), "RegisterDragDrop hr 0x%08lX",
                      static_cast<unsigned long>(hr));
        LOG_ERROR(kTag, msg);
        return false;
    }
    g_dropHwnds.push_back(hwnd);
    return true;
}

void registerFileDropTargetTree(HWND root, FileDropCallback cb, FileDropHover hover) {
    if (!root)
        return;
    registerFileDropTarget(root, std::move(cb), std::move(hover));
    EnumChildWindows(root, enumRegister, 0);
}

void revokeFileDropTarget(HWND hwnd) {
    if (hwnd) {
        RevokeDragDrop(hwnd);
        for (auto it = g_dropHwnds.begin(); it != g_dropHwnds.end();) {
            if (*it == hwnd)
                it = g_dropHwnds.erase(it);
            else
                ++it;
        }
        return;
    }
    for (HWND h : g_dropHwnds)
        RevokeDragDrop(h);
    g_dropHwnds.clear();
}

} // namespace reals::shell
