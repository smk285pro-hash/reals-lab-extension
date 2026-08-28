## 2026-08-26T19:39:41Z

<original_task>
This is a single self-contained fix; keep it small and focused.

Sửa lỗi và hoàn thiện 3 tính năng tương tác File Browser trong Reals Lab (REAPER extension / WebView2):
1. Fix OLE Drag & Drop sample từ Browser ra REAPER track/timeline/desktop.
2. Hỗ trợ kéo thả thư mục từ Windows Explorer vào cửa sổ ứng dụng để add root folder (kèm visual drop-zone overlay).
3. Fix triệt để lỗi tự động cuộn (scroll jump) lên đầu khi click vào file nhạc, thư mục hoặc danh sách phát.

Working directory: c:/Users/smk28/Desktop/reals lab extension
Integrity mode: development

## Requirements

### R1. OLE Drag & Drop sample ra ngoài REAPER / Windows Explorer
- Kéo thả file audio từ File List trong WebView2 ra track/timeline của REAPER hoặc Explorer phải kích hoạt OLE drag drop (`CF_HDROP` / `DROPEFFECT_COPY`) mượt mà và tin cậy.
- Xử lý đồng bộ sự kiện giữa HTML5 Drag (`dragstart` trong WebView2) và Win32 message dispatch (`WM_REALS_BEGINDRAG` / `DoDragDrop`).

### R2. Kéo thả thư mục từ bên ngoài vào để Add Root Folder
- Hỗ trợ sự kiện `dragover`, `dragleave`, `drop` trên cửa sổ ứng dụng khi người dùng kéo thư mục từ Windows Explorer vào.
- Hiển thị hiệu ứng visual drop-zone (viền sáng / overlay báo hiệu sẵn sàng nhận thư mục).
- Nhận diện đường dẫn thư mục hợp lệ và gọi `fs.addRoot` để lưu cấu hình và cập nhật danh sách Roots ngay lập tức.

### R3. Khắc phục triệt để lỗi Scroll Jump
- Không build lại toàn bộ DOM (`innerHTML = ''`) làm mất `scrollTop` khi click chọn bài hát, phát preview audio, hoặc vẽ lại waveform.
- Lưu trữ và khôi phục chính xác vị trí cuộn (`scrollTop`) của Tree View và File List khi thao tác duyệt thư mục hoặc chọn file.

## Verification Resources
- Build command: `cmake --build --preset windows`
- Check UI assets và event bridge consistency.

## Acceptance Criteria

### Interaction & UX
- [ ] Kéo thả file audio từ Browser ra REAPER track view tạo track và chèn media item thành công.
- [ ] Kéo thả thư mục từ Windows Explorer vào app hiển thị drop-zone overlay và thêm thành công thư mục vào danh sách Roots.
- [ ] Click chọn bài hát, click thư mục hoặc toggle preview không làm giật hay nhảy cuộn (reset scroll) của danh sách file và cây thư mục.
- [ ] CMake build pass không warning, không crash khi chạy.
</original_task>

Working directory: c:/Users/smk28/Desktop/reals lab extension/.agents/victory_auditor_1
Parent conversation ID: 4e136f09-0ea0-4b6e-9c80-4c170c1c7d33

Conduct a 3-phase victory audit (timeline check, cheating/shortcut detection, independent test and code execution).
Verify all acceptance criteria, zero-warning C++20 build, and correctness of R1, R2, R3.
Provide a clear structured verdict (CONFIRMED or REJECTED) with detailed evidence.
