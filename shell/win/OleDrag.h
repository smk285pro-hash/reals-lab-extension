#pragma once

// OLE CF_HDROP drag source so a file from the WebView can be dropped onto
// REAPER (or Explorer). Blocks on the calling thread via DoDragDrop.
// Also an IDropTarget so Explorer can drop a folder onto the host HWND
// (WebView2 AllowExternalDrop is off — otherwise the child eats the drop).
#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <string>
#include <vector>

namespace reals::shell {

void beginFileDrag(HWND owner, const std::wstring& path);

using FileDropCallback = std::function<void(const std::vector<std::wstring>& paths)>;
using FileDropHover = std::function<void(bool active)>;

bool registerFileDropTarget(HWND hwnd, FileDropCallback cb, FileDropHover hover = {});
void registerFileDropTargetTree(HWND root, FileDropCallback cb = {}, FileDropHover hover = {});
void revokeFileDropTarget(HWND hwnd);

} // namespace reals::shell
#endif
