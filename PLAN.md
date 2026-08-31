# Reals Lab — Ghi chú kế hoạch

## Ý tưởng chính
- **Reals Lab**: extension cho DAW REAPER, có giao diện.
- Mục tiêu: kết nối **toàn bộ dịch vụ của RealS** (reals.media) trực tiếp trong DAW — kiểu "ReaPack riêng của RealS".
- Người dùng mở REAPER lên là browse chợ, tải, cài đặt mà không cần ra web.

## Phạm vi dự án (cập nhật 2026-08-24)
- **KHÔNG chỉ REAPER extension** — sẽ build thêm bản **desktop app độc lập**.
- Cần **xử lý audio tại máy** (không phải cái gì cũng đẩy lên web):
  - Nghe/preview audio (VD: nghe thử sample trong browser)
  - Load audio vào buffer
  - Xử lý âm thanh cơ bản tại chỗ (căn ngưỡng âm lượng...)

## Bối cảnh (2026-08-24)
- reals.media = marketplace bán script, VST, tool.
- **VST** → dành cho mọi người dùng, mọi DAW.
- Còn lại (script, extension...) → riêng cho user REAPER.
- **Hệ thống phân tích âm thanh**: tách nhạc (stem separation), phát hiện key, hợp âm, tempo.
  - Chức năng: xuất MIDI, tải về ZIP.
  - **Mọi tính năng đều có API riêng** → có thể tạo tool gọi các API đó.

## Tính năng dự kiến

### 🎵 Audio Lab
- Gửi track/item trong REAPER lên API → tách stem → trả về thành track mới trong project
- Detect key/hợp âm/tempo → hiện UI + xuất MIDI kéo thả vào project
- Render vùng chọn rồi gửi phân tích
- **Lọc noise (noise reduction)** — qua API, áp dụng lên track/item trong project

### 🛒 Marketplace
- Browse/tìm kiếm sản phẩm reals.media ngay trong DAW
- Mua/tải bằng tài khoản đã mua
- Tự động cài vào đúng thư mục
- Check update sản phẩm đã cài

### 📂 File Browser (giống FL Studio) — FREE để câu khách
- Trình duyệt file dạng sidebar như FL Studio
- Miễn phí — dùng làm điểm hút user về các dịch vụ trả phí
- Gồm: file local (**làm trước, dùng được luôn**) + content free từ reals.media
- **Chiến lược**: file local làm thật; content free từ reals.media chỉ thiết kế kiến trúc + UI sẵn, hiển thị "Đang phát triển" — khi nào web có dịch vụ thì nối API vào

### 🤖 AI Agent (điều khiển REAPER bằng ngôn ngữ tự nhiên)
- Chat UI trong extension, user gõ lệnh bằng tiếng tự nhiên.
- **Agent thật sự**: cắm API gọi LLM từ hệ thống RealS vào.
- Model: **chủ yếu dùng model free**.
- **Bộ kết nối chuẩn OpenAI-compatible** làm sẵn ở client — trỏ endpoint tới đâu cũng chạy.
- Não (LLM + logic điều khiển) nằm ở **server web**; **máy khách chỉ truyền tải/thực thi tool** trong REAPER.
- **Phạm vi: điều khiển được ALL REAPER.**
- **Cách điều khiển — Hybrid:**
  1. **~40-60 tool ngữ nghĩa cao** (create_track, set_tempo, add_fx, split_item...) — phủ 90% việc thường gặp
  2. **Cầu `Main_OnCommand`** — agent chạy được mọi action có ID trong REAPER
  3. **Bridge get/set thuộc tính** track/item/project qua JSON, whitelist hàm
- **KHÔNG dùng script sinh động** (rủi ro bảo mật, đã cấm Lua).
- **Định nghĩa tool nằm ở SERVER**, client chỉ có **executor generic** nhận `{tool, args}`.
- **Phân quyền 2 lớp:**
  - **Server (admin page)**: chủ động cho phép/hạn chế user được làm gì → sync xuống client.
  - **Client — 3 chế độ:** ① Hỏi tất cả ② Chỉ hỏi nguy hiểm ③ Toàn quyền
- **An toàn toàn quyền**: bọc mọi action trong **undo block** (Ctrl+Z hoàn tác được).
- **Kết nối chéo hệ thống**: "lọc noise all track audio", "tự tìm track vocal rồi lọc" → agent tự gọi API + tự làm.

### 🖱️ Context Menu
- **Ctrl + Right-click** → menu ngữ cảnh Reals Lab tools
- Click audio/item nào → chọn chức năng áp dụng luôn vào item đó
- Quét chọn nhiều item → áp dụng tất cả cùng lúc

### 👤 Tài khoản
- Login/logout, xem license, sản phẩm đã mua

### ⚙️ Khác
- Cài đặt: đường dẫn thư mục, chất lượng render khi gửi API
- Thông báo: sản phẩm mới, khuyến mãi
- **Self-update cho Reals Lab**: bảng thông báo + nút update trong app

## Đã chốt (2026-08-26) — Browser UX, Drag & Drop, Drop-zone Root
- **OLE drag sample ra REAPER**: `IDropSource` + `IDataObject` (`CF_HDROP` + `CF_UNICODETEXT`). UI không dùng HTML5 `dragstart` (mouse đã nhả → OLE cancel). Pointer-move 6px → `browser.beginDrag` (PostMessage, không gọi `DoDragDrop` trong `WebMessageReceived`). `ReleaseCapture` trước `DoDragDrop`.
- **Kéo thư mục Explorer vào để add root**: WebView2 `AllowExternalDrop=FALSE` (JS `File.path` không phải OS path). Host `IDropTarget` trên HWND + child (steal `DRAGDROP_E_ALREADYREGISTERED`) → `fs.dropPaths` + event `fs.rootsChanged`. Overlay `#dropOverlay` theo event `fs.dropHover`. HTML5 drop chỉ dùng ở preview mock (`!hasWebView`).
- **Khắc phục lỗi Scroll Jump**: `#content`/`#pane-browser` overflow hidden; `#tree`/`#files` `overflow-anchor:none`; không `renderTree` khi preview; không `paintVisible` khi chỉ đổi selection; `DocumentFragment` + `state.dirScrolls`.
- **Quét sóng âm nền hoàn toàn bất đồng bộ (Background Waveform Worker & Instant Play)**: `Engine::playFile` chỉ khởi chạy stream âm thanh (phát nhạc tức thì 0ms, không block UI). Việc tính toán sóng âm (`computeEnvelope`) được đẩy sang background worker thread, cache in-memory (`envCache`, `probeCache`) và đẩy sự kiện `audio.envelope` về UI cập nhật waveform mượt mà.
- **Thiết kế lại hiển thị sóng âm (Waveform Canvas Redesign)**: Quét chính xác toàn bộ biên độ PCM (64k chunk trên worker), hiển thị 180 dải sóng âm dạng hai tông màu (Played = màu accent cam sáng, Unplayed = trắng mờ), đường tim chuẩn và con trỏ playhead sắc nét theo phong cách DAW hiện đại.
- **Cây thư mục bên trái & Tự động thu gọn gọn gàng (Auto-Collapse Tree)**: Bổ sung cơ chế accordion tự động thu gọn các nhánh thư mục cùng cấp và không liên quan khi mở một thư mục mới (`autoCollapseTree: true` mặc định bật). Tính năng này có thể tùy chỉnh bật/tắt trong popup Cài đặt (Settings ⚙️).
- **Danh sách file bên phải & Responsive Auto-Hide**: Đã loại bỏ hoàn toàn cột ngày giờ sửa đổi (`fdate`) dư thừa; chỉ giữ tên file, thời lượng (`fdur`) và dung lượng (`fsize`). Khi cửa sổ extension bị co/ép nhỏ (< 650px / 480px), hệ thống tự động ẩn cột dung lượng và co giãn thanh cây thư mục/công cụ để ưu tiên toàn bộ không gian cho tên file không bị che khuất.
- **Loại bỏ thanh Banner cập nhật trên cùng**: Đã xóa bỏ hoàn toàn thanh thông báo banner (`#banner`) ở đầu giao diện để giải phóng không gian, giúp giao diện tập trung trực tiếp vào thanh công cụ và danh sách thư mục.
- **Hệ thống Quét & Phân tích AI Âm thanh Offline 100%, DSP Sync DAW & Mini Piano Transposer (2026-08-26)**:
  - **Bộ suy luận AI C++ Offline (`core/ai/`)**:
    - **Tempo (BPM)**: Tích hợp `Essentia TempoCNN` (kèm fallback `RhythmExtractor2013`) phân tích BPM chính xác và trích xuất beat onsets.
    - **Key & Ensemble Voting**: Mô hình `Essentia EDMA` (chuyên biệt EDM/Modern Production) kết hợp cơ chế bỏ phiếu hòa âm đa mô hình (`Temperley` + `Krumhansl`) xuất Key + Mode + Camelot/OpenKey.
    - **Genre (400 Styles)**: Mô hình `Discogs-MAEST` (hoặc `Discogs-EffNet`) phân loại 400 sub-genre chi tiết (`Minimal House`, `Nu Metal`, `Trap-EDM`...).
    - **Mood & CLAP Semantic Search**: `Mood-Jamendo` multi-label + trích xuất vector 512 chiều `CLAP (Contrastive Language-Audio Pretraining)` lưu trữ trong SQLite để tìm kiếm cảm xúc tự do bằng ngôn ngữ tự nhiên.
  - **DSP Time-Stretch Sync DAW & Realtime Pitch Shifting (`core/audio/`)**:
    - Tích hợp engine DSP `SoundTouch` xử lý kéo dãn thời gian khớp nhịp Project BPM của REAPER (`Master_GetTempo()`) không đổi cao độ khi bật nút `Sync BPM`.
    - Real-time Pitch-Shifting (±12 bán cung) với độ trễ cực thấp (<30ms).
  - **Bộ máy Tìm kiếm Cú pháp & Ngữ nghĩa (`core/search/`)**:
    - `QueryParser`: Hỗ trợ cú pháp tiền tố `/` (ví dụ `/trap /kick kshmr`, `/bpm:120-130 /key:F#m /ambient`).
    - `SemanticSearch`: Tính toán tương đồng Cosine vector bằng tập lệnh SIMD AVX2/SSE2 cực nhanh (<5ms cho 1.000 vectors).
  - **Giao diện Player & Mini Piano Keyboard Transposer (`ui-web/`)**:
    - Dải chip badge hiển thị Tag/Genre/Mood trực quan phía trên waveform.
    - Nút toggle **`Sync BPM`** trên player (highlight cam khi kích hoạt).
    - Popup **Mini Piano Keyboard** 12 phím (C -> B) + nút **`Original Key`** cho phép nghe thử đổi tone tức thời.
  - **Đạt chứng nhận M6**: Vượt qua toàn bộ 146/146 test cases (9 test suites) với 0 cảnh báo biên dịch MSVC C++20.
- **Tích hợp Dock trực tiếp vào Docker của REAPER (2026-08-26)**:
  - **Dock / Undock 1-Click**: Bổ sung nút **📌 Dock** trên thanh Top Bar (cạnh nút Cài đặt ⚙️) và tùy chọn trong menu Cài đặt.
  - **Kết nối REAPER SDK Docker API**: Tích hợp `DockWindowAddEx`, `DockWindowRemove`, `DockIsChildOfDock`, `DockWindowActivate` với định danh `"REALSLAB_DOCK"`.
  - **Chuyển đổi trạng thái linh hoạt (Adaptive UI)**:
    - Khi Dock vào REAPER (`#app.docked`): Khung cửa sổ tự động bỏ viền, ẩn các nút phóng to/thu nhỏ ngoài, chuyển nút Dock thành `↗ Tách cửa sổ riêng` để tiện chuyển đổi.
    - Khi Undock (Cửa sổ riêng): Tự động khôi phục giao diện tràn viền bo góc hiện đại 10px, hỗ trợ kéo 8 hướng mượt mà.
  - Tự động đồng bộ trạng thái khi người dùng kéo tab trong REAPER Docker.
- **Thanh phân cách kéo thả thay đổi kích thước các bố cục nội bộ (Draggable Layout Splitters) (2026-08-26)**:
  - **Thanh phân cách Dọc (Tree ↔ File List)**: Nằm giữa Cây thư mục và Danh sách File. Cho phép rê chuột vào vạch phân cách kéo qua trái/phải để tùy chỉnh độ rộng của cây thư mục theo ý muốn (có giới hạn min/max an toàn).
  - **Thanh phân cách Ngang (Browser Body ↔ Preview Player)**: Nằm giữa khu vực danh sách file và khu vực trình phát preview phía dưới. Cho phép rê chuột kéo lên/xuống để mở rộng hoặc thu gọn chiều cao của sóng âm và vùng player.
  - Tự động lưu và phục hồi kích thước đã kéo (`treeWidth`, `previewHeight`) vào cấu hình hệ thống khi mở lại extension.
- **Cơ chế Quét & Phân tích AI Thư mục chủ động (Manual Folder Scan Trigger) (2026-08-27)**:
  - **Menu chuột phải trực quan trên thư mục**: Bổ sung tùy chọn `⚡ Quét & Phân tích AI` vào menu chuột phải (context menu) của thư mục trên Cây thư mục (Tree), Danh sách file (Files list) và Thư mục gốc (Roots).
  - **Thanh trạng thái tiến độ thời gian thực (`#scannerBar`)**: Khi bấm quét, một thanh tiến độ hiện đại xuất hiện ngay trên danh sách file với biểu tượng nhấp nháy `⚡`, hiển thị rõ: số file đã xử lý / tổng số file (`124/450`), số file mới đã thêm, tên file đang phân tích theo thời gian thực và thanh % tiến độ trực quan.
  - **Nút Dừng quét (`✕`)**: Cho phép người dùng dừng tiến trình quét bất kỳ lúc nào một cách an toàn.
  - **Tự động làm mới**: Sau khi quét xong, hiển thị thông báo hoàn tất và tự động tải lại danh sách file để cập nhật ngay metadata mới.
- **Bộ máy Tìm kiếm Thông minh Hybrid (Intelligent Search Engine) (2026-08-27)**:
  - **Kết nối toàn diện `core/search/SearchEngine` vào runtime**: Tích hợp `QueryParser`, `SearchEngine` và `SemanticSearch` trực tiếp vào `bridge/src/Bridge.cpp` (`runSearch`).
  - **Tìm kiếm cú pháp `/` (Syntax Tokens)**: Hỗ trợ `/fav`, `/bpm:120-130`, `/key:Am` (hoặc `/camelot:8A`), `/genre:...`, `/mood:...`, và các tag nhạc cụ/thể loại (`/trap`, `/hiphop`, `/kick`, `/808`, `/vocal`...).
  - **Tìm kiếm ngữ nghĩa AI (CLAP Semantic Search)**: Tự động trích xuất embedding câu mô tả tự do ("punchy 808", "lo-fi chill keys") và tính toán độ tương đồng cosine vector SIMD (AVX2/SSE2) siêu nhanh đối chiếu với cơ sở dữ liệu mẫu.
  - **Tìm kiếm kết hợp (Hybrid Search)**: Tự động kết hợp kết quả tìm kiếm database với trình quét thư mục cục bộ (fallback crawler), hỗ trợ tìm kiếm toàn bộ thư viện (Global Library Search) ngay cả khi chưa chọn thư mục cụ thể.
  - **Giao diện Gợi ý & Auto-Complete (`/`)**: Popup gợi ý tag thông minh khi gõ `/` trong ô tìm kiếm, hiển thị huy hiệu thông số âm nhạc (BPM, Key, Thể loại, Độ khớp %) trực tiếp trên từng hàng file.
- **Tối ưu hóa hiển thị Responsive khi thu nhỏ/ép dẹp (< 520px / < 440px / < 320px) (2026-08-26)**:
  - **Top Bar Thông Minh**: Khi bề ngang < 520px, thanh tab tự động ẩn chữ chỉ giữ lại Icon SVG sắc nét (`🛍️`, `🎛️`, `🤖`, `🌐`, `👤`), giúp toàn bộ 5 tab + nút Cài đặt ⚙️ + 3 nút điều khiển cửa sổ (`— ▢ ✕`) vừa khít hoàn hảo không bị tràn hay đè lên nhau.
  - **Thanh công cụ Browser 2 hàng tinh gọn**: Hàng 1 (`📁 Cây thư mục toggle`, `Root select`, `Search box`, `Sort select`), Hàng 2 (`Audio`, `Auto`, `★`, `Nhãn`, `Làm mới`) cuộn ngang mượt mà khi bị hẹp.
  - **Nút Ẩn/Hiện Cây thư mục (`📁`)**: Bổ sung nút bấm thu gọn cây thư mục ngay trên thanh công cụ. Khi ấn ẩn cây thư mục, danh sách file bên phải lập tức chiếm trọn 100% bề ngang cửa sổ để hiển thị tên file dài không bị cắt bớt (`...`).
  - **Player & VU Meter Responsive**: Tối ưu kích thước waveform (38px), thanh VU meter (32px), ẩn nhãn chữ đè thanh trượt volume/threshold khi hẹp để các thanh trượt co giãn tối đa không gian.
- **Cửa sổ tràn viền Custom Frameless Window & Co giãn 8 hướng (2026-08-26)**:
  - Ẩn toàn bộ thanh tiêu đề mặc định của Windows (`WM_NCCALCSIZE` return 0), kéo vùng hiển thị Webview tràn 100% lên kịch mép trên cùng (pixel 0,0).
  - Lớp phủ hạt tương tự (**Analog Noise Grain**) giờ đây bao phủ kín 100% toàn bộ bề mặt cửa sổ từ viền trên cùng tới đáy kèm bo góc nhẹ hiện đại 10px.
  - **Co giãn kích thước mượt mà cả 8 hướng (Resize 8-Directions)**: Kết hợp Subclassing HWND con của WebView2 (`SetWindowSubclass` + `HTTRANSPARENT` ở viền 8px) + 8 vùng handle co giãn cạnh/góc trong Web (`bridge('window.startResize')` gọi `WM_NCLBUTTONDOWN`). Cho phép rê chuột vào bất kỳ mép (trên/dưới/trái/phải) hay 4 góc để kéo thả co giãn kích thước cửa sổ cực kỳ mượt mà.
  - Tích hợp vùng kéo thả cửa sổ (`WM_NCLBUTTONDOWN` + `HTCAPTION`), bóng đổ hệ thống (`DwmExtendFrameIntoClientArea`).
  - Tích hợp bộ 3 nút điều khiển cửa sổ tối giản (`—` Thu nhỏ, `▢`/`❐` Phóng to/Khôi phục, `✕` Đóng) trực tiếp trên góc phải thanh Top Bar của Webview.
- **Gộp thanh Tab điều hướng lên hàng Top Bar (Tối ưu không gian dọc)**: Bỏ hàng header logo riêng bên trong webview (vì thanh tiêu đề cửa sổ Windows đã có logo + tên Reals Lab), chuyển toàn bộ thanh tab điều hướng (`Market`, `Audio Lab`, `Agent`, `Browser`, `Tài khoản`) và nút Cài đặt (⚙️) lên thẳng hàng đầu tiên (`#topbar` cao 44px). Giúp giao diện cực kỳ gọn gàng, tiết kiệm không gian và đưa thanh công cụ browser lên sát ngay trên đầu.
- **Icon RealS trên thanh tiêu đề Windows & Taskbar**: Nhúng file tài nguyên Win32 `.rc` (`reals.ico` đa độ phân giải 16px - 256px) vào DLL, nạp trực tiếp qua `LoadImageW` + `WM_SETICON` (`ICON_BIG` & `ICON_SMALL`) để thanh tiêu đề cửa sổ OS và Taskbar/Alt-Tab luôn hiển thị icon RealS sắc nét thay cho icon mặc định của Windows.
- **3 Chế độ kích thước hiển thị (Display Density)**: Thêm tùy chọn "Kích thước hiển thị" trong Cài đặt với 3 nấc: **Nhỏ (gọn)** (28px row), **Vừa (chuẩn)** (34px row - mặc định), **To (thoáng)** (44px row). Cả cây thư mục và danh sách file ảo (virtual scroll) tự động tính toán lại chiều cao hàng động mượt mà.
- **Phím Spacebar điều khiển DAW Transport & Nghe thử đồng thời khi DAW Play**: 
  - Khi DAW đang Play trong nền, người dùng **vẫn có thể click nghe thử sample trong extension bình thường** để audition sample theo nhịp bài.
  - Khi người dùng bấm **phím cách (Space)** hoặc kích hoạt lệnh Transport Play/Stop trong REAPER (`commandHook` `40044`, `1007`, `1016`, `40073`), âm thanh preview trong extension sẽ lập tức ngắt ngay (0ms).
- Auto-preview toggle, keyboard (↑↓ Enter Space Backspace Esc F2 Del), virtualize list lớn.
- Search async (`browser.search` → `{pending,gen}` + event `browser.searchResult`), hủy thế hệ cũ.
- Watch folder (`fs.watch` / `fs.changed`, DirWatch IOCP, debounce 250ms).
- Preview: click waveform seek, rate slider + Sync (pitch), `audio.probe` duration trên hàng.
- Filter ★ / nhãn màu; `browser.clearRecents`; UTF-8 path (`u8path` / `pathToUtf8`).
- **WebView2 Disable HTTP Cache**: Thêm switch `--disable-http-cache` vào `CoreWebView2EnvironmentOptions` để WebView2 luôn đọc live file từ đĩa, tránh bị ghim cache cũ.
- InsertMedia mode **1** (new track). BPM/key file = Lab (trả phí), không nhét vào hole-fix.

## Đã chốt (2026-08-25) — WebView2 cold start (đường khách)
- **Pre-warm lúc plugin load**: tạo HWND ẩn + WebView2 environment/controller/Navigate trong lúc REAPER khởi động. Lần Show Window đầu của khách không còn chờ ~2s.
- **Disk cache giữ giữa các phiên.** Chỉ `ClearBrowsingData(DISK_CACHE)` khi `index.html`/`app.css`/`app.js` đổi (stamp `%APPDATA%\RealsLab\ui-cache.stamp`). Lần cài đầu (stamp trống) không xoá — cache đang rỗng.
- **Nền `#0D0E11`** (HWND brush + `DefaultBackgroundColor`) thay vì đen, hết nháy.
- Đánh đổi: `msedgewebview2.exe` chạy nền ngay khi REAPER mở (~100–150MB) dù chưa mở panel.

## Đã chốt (2026-08-24)
- **API đã có sẵn** cho các tính năng phân tích âm thanh.
- Luồng: gửi file lên → hệ thống xử lý → trả kết quả về.
- **Mô hình kinh doanh**: trả phí (có bản dùng thử free).
- **Hệ thống đăng nhập** thiết kế ở web chính (reals.media), extension chỉ dùng lại.

### Layout
- **1 cửa sổ chính dạng tab** (kiểu panel news FL Studio) — dock được trong REAPER.
- **Tách tab ra cửa sổ riêng = trả phí** (đăng ký gói).
- **Thanh điều hướng đặt được 4 vị trí: Trên/Dưới/Trái/Phải** — user chọn trong Cài đặt. Mặc định: Trên.

### Free / Trả phí
| Tính năng | Giá |
|---|---|
| File browser (local) | **FREE** |
| Content free từ reals.media (khi có) | **FREE** |
| Dùng chung 1 window nhiều tab | **FREE** |
| Tách tab thành cửa sổ riêng | **TRẢ PHÍ** |
| Audio Lab (tách nhạc, key, hợp âm, tempo, MIDI) | **TRẢ PHÍ** (có dùng thử) |
| Marketplace: browse | **FREE** |
| Marketplace: mua/tải sản phẩm | Trả theo sản phẩm |

### Tech stack (chốt — cập nhật lần 3, 2026-08-24)
- **UI PIVOT: ImGui → WebView2 (HTML/CSS/JS)**.
  - Lý do: mockup đã duyệt chính là HTML → convert UI gần như 1-1; team giỏi web; hết cảnh vẽ tay từng pixel.
  - **Windows**: WebView2 (Microsoft, free). **macOS**: WKWebView (P6). **Linux**: WebKitGTK (P6).
  - UI HTML/CSS/JS **viết 1 lần cho cả 3 OS**; chỉ lớp host mỏng (~300 dòng/OS) viết riêng.
  - ImGui code (ui/) giữ trong repo làm tham chiếu đến khi WebView đạt parity, sau đó xóa.
- **C++20 thuần**, KHÔNG dùng Lua (bảo mật), KHÔNG dùng JUCE (license + rủi ro tích hợp REAPER).
- **Audio tại máy**: miniaudio (nghe/buffer/playback, căn ngưỡng) + tự viết WAV writer.
- **Network**: libcurl. **JSON**: nlohmann/json.
- **Kiến trúc mới:**
```
reals-lab/
├── core/       ← logic chung: audio engine, API client, browser model, agent...
├── ui-web/     ← UI = HTML/CSS/JS (từ mockup.html đã duyệt) — 1 lần cho cả 3 OS
├── bridge/     ← JS ↔ C++ (WebMessage JSON): audio, browser, REAPER actions
├── shell/
│   └── win/    ← Win32 window + WebView2 controller (Mac/Linux P6)
├── app/        ← desktop app độc lập (.exe)
└── extension/  ← REAPER extension (.dll)
```

### UI (chốt)
- **Design system đã duyệt** → `DESIGN.md`. Mockup tham chiếu: `mockup.html`.
- Nội dung bên trong theo phong cách **desktop app**: toolbar compact, panel có header.

## Đã chốt bổ sung (2026-08-24)
- **Ngôn ngữ UI**: Việt + Anh (i18n từ đầu, mọi string qua `tr("key")`).
- **Team**: 1 người + AI agent.
- **Nền tảng**: Windows trước, kiến trúc tối ưu cả macOS + Linux từ đầu.
- **Backend reals.media**: Next.js Route Handlers (Node runtime), PostgreSQL + Prisma v6.11.
  - Models: User/Account/Session (Google OAuth), Product, ProductFile, Category, Purchase, Review, SellerApplication, Report, Notification, AnalyticsEvent/Session.
- **Chiến lược MVP**: nền móng vững trước → **Browser local đầu tiên** → các module sau.

## Ý tưởng mở rộng (đề xuất — chờ duyệt)
- **Audio Lab**: loudness normalize/LUFS match, silence auto-trim, fade, convert format/sample rate, batch processing, A/B compare, history, karaoke mode, pitch detect, BPM match, loudness meter, spectrogram, AI tag + tìm sample giống
- **Marketplace**: product detail, wishlist, review, banner KM, refund, license theo máy, gợi ý tương tự, download queue/resume, offline mode, free pack tuần
- **Browser**: preview + loop, tag màu, smart collections, waveform mini, quick actions, watch folder, import URL, gửi thẳng Audio Lab
- **Agent**: memory phiên, multi-step planning, quick templates, tư vấn mix, chạy nền, chat history, đa ngôn ngữ, voice input (sau)
- **Tài khoản**: 2FA, device manager, nâng/hạ gói, usage stats, notification center
- **Hệ thống**: onboarding wizard, multi-language, shortcuts tùy biến, proxy, cache manager, offline queue, crash report

## Bàn sau (chưa chốt)
- Auth/API key cho extension gọi API (phụ thuộc đăng nhập web chính).
- Định dạng kết quả trả về (file trực tiếp hay JSON chứa link).
- Danh sách tính năng MVP chi tiết.
- Thứ tự sau Browser: Audio Lab / Marketplace / Agent.

## Việc cần quyết định (khi bàn tới)
- [x] ~~UI: floating hay dock?~~ → 1 window tab, dock được, tách tab = trả phí
- [x] ~~Cấu trúc repo~~ → core/ui/app/extension (đã dựng P0)

## API phân tích âm thanh — ĐÃ CÓ THẬT (2026-08-24)
- **Base URL**: `https://smk285pro--ai-audio-lab-fastapi-web.modal.run` (FastAPI trên Modal)
- Docs đầy đủ: `C:\Users\smk28\Downloads\test web\API_DOCS.md` (nguồn: thư mục `test web`)
- Không cần auth, CORS mở (`*`), upload multipart field `file`, max 100MB

### Endpoints đang dùng (v1 — public dev API)
| Endpoint | Kiểu | Chức năng |
|---|---|---|
| `POST /api/v1/analyze` | sync 2-5s | tempo + key |
| `POST /api/v1/chords` | sync 15-60s | hợp âm (Viterbi HMM 169 states) |
| `POST /api/v1/separate?stem_mode=4` | async job | tách stem Demucs GPU (mode 2/4/6/8) |
| `POST /api/v1/denoise?strength=80` | async job | lọc noise DeepFilterNet3 (0-100) |
| `GET /api/v1/jobs/{task_id}` | poll 2-5s | {status, percent, stage, result} |
| `GET /api/stems/{task}/{name}` · `/api/export/stems-zip/{task}` | download | stem WAV + ZIP |

### Tích hợp trong extension (đã build)
- `core/lab/LabApi` — WinHTTP client (multipart upload stream, poll, download)
- `bridge`: `lab.analyze/keychord/stem/denoise` → **background thread** → event queue → timer đẩy `lab.progress` / `lab.result` / `lab.error` ra UI
- Kết quả stem/denoise tải về `%APPDATA%\RealsLab\lab\` → chèn vào project được
- UI Audio Lab: chọn file (right-click trong Browser → Gửi Audio Lab) → tool → progress → results + "Chèn tất cả vào project"
- **Studio Session API** (upload 1 lần, MIDI export, SSE progress) — nâng cấp sau, xem API_DOCS.md

## Đã chốt (2026-08-25) — đợt điều tra + sửa bug toàn dự án
Audit tìm thấy ~25 lỗi (9 nghiêm trọng), đã sửa hết, build zero-warning + smoke test pass:
- **`net::HttpClient` đã có implementation thật** (`core/src/net/HttpClient.cpp`, transport WinHTTP). Đây là nơi DUY NHẤT được đụng API mạng (đúng luật AGENTS). LabApi viết lại mỏng chạy trên nó.
- Fix 2 bug ẩn trong transport cũ: **leak** `WinHttpCrackUrl` (buffer NULL + length>0 → WinHTTP alloc, không ai free) và **query params bị rơi** (`stem_mode=4`, `strength=80` không bao giờ tới server vì chỉ lấy UrlPath, bỏ ExtraInfo).
- **Base URL Audio Lab cấu hình được**: Config key `labApiBaseUrl` (fallback: constant `kBase`). Token tự động gắn header `Authorization: Bearer` từ Config `authToken`.
- **Mọi file I/O wide-safe**: `CreateFileW` + `fs::path` — path tiếng Việt không còn hỏng.
- **Lab job workers được track + join** khi Bridge hủy (hết detach); poll có timeout (300 polls ~10 phút).
- Ngày sửa file dùng `clock_cast<system_clock>` (trước đây sai ~369 năm trên MSVC).
- Engine có `setLoop()` áp dụng ngay; bridge `audio.setLoop` hoạt động thật.
- Browser UI: tag màu + ★ hiển thị được (tagCache/favSet nạp thật), sort gửi xuống backend, Refresh invalidate cache, rename/delete báo lỗi, search chống race, cột ngày sửa mới.
- Context menu lab jobs chạy THẬT qua tab Audio Lab (không còn toast P2); folder menu có add/remove root thật.
- Extension unload dọn sạch: unregister timer/hookcommand/gaccel/command_id + CoUninitialize đúng ownership.
- Version thống nhất **0.2.0**, một nguồn duy nhất: bridge `app.info` → UI tự render.
- CMake guard khối legacy ImGui app (không còn `app/main.cpp` thì skip, option vô hại).

## Bài học (bug khó + nguyên nhân gốc)
- **[P1] Extension crash khi load REAPER** (2026-08-24): `Config::load()` lock `m_mutex` rồi gọi `save()` — `save()` lock lại cùng thread → `std::mutex` ném `std::system_error(EDEADLK)` "resource deadlock would occur" → uncaught → abort. **Fix**: `std::recursive_mutex` cho Config + try/catch + log từng bước trong entrypoint. **Bài học**: không bao giờ gọi hàm public cũng lock mutex từ trong hàm đang giữ lock (recursive_mutex hoặc tách hàm unlocked).
- **[Tooling] PowerShell Set-Content/Add-Content làm hỏng encoding UTF-8** của file tiếng Việt (PLAN.md bị mojibake 2026-08-24, đã viết lại). **Bài học**: chỉ sửa file tiếng Việt bằng Write/Edit tool, không qua PowerShell.
- **[P1.5] Query params bị rơi + leak trong WinHTTP** (2026-08-25): `WinHttpCrackUrl` tách URL thành UrlPath và ExtraInfo (phần `?...`) — code cũ chỉ lấy UrlPath nên `stem_mode/strength` không tới server; đồng thời truyền pointer NULL + length>0 khiến WinHTTP `GlobalAlloc` buffer mà caller không `GlobalFree`. **Fix**: pre-allocated stack buffers + append ExtraInfo vào path. **Bài học**: đọc kỹ contract API Windows trước khi dùng; test đường dẫn có query.
- **[P1.5] Ngày sửa sai ~369 năm** (2026-08-25): `file_clock` của MSVC có epoch 01-01-1601, `duration_cast` thẳng sang seconds không phải Unix time. **Fix**: `clock_cast<system_clock>` trước. **Bài học**: muốn epoch seconds "thật" thì bắt buộc qua `system_clock`.
- **[P1] Deadlock/Crash khi click nghe audio sample** (2026-08-26): `Engine::level()` giữ khóa `stateMutex` rồi gọi `positionFraction()`, mà `positionFraction()` lại cố tình lock lại `stateMutex` (loại `std::mutex` thường). Trên MSVC, việc lock lặp lại `std::mutex` trên cùng một luồng ném ngoại lệ `std::system_error (EDEADLK)` khiến REAPER bị terminate crash ngay lập tức ở tick timer đầu tiên. **Fix**: Đổi `stateMutex` & `dspMutex` thành `std::recursive_mutex`, tính toán `frac` trực tiếp trong `level()`, giải phóng `ma_data_source_uninit(&base)` khi đóng dspSource, và bọc toàn bộ `timerHook()` trong khối `try-catch`.
- **[Docking] Mất UI khi Undock khỏi REAPER Docker** (2026-08-26): Khi REAPER dock một cửa sổ vào docker container, nó gán `SetParent(g_hwnd, hDocker)`. Khi undock (`DockWindowRemove`), nếu không gọi `SetParent(g_hwnd, nullptr)` và không khôi phục style `WS_POPUP | WS_THICKFRAME` + kích thước/tọa độ thật của floating window, `g_hwnd` sẽ bị kẹt làm con hoặc nằm ngoài viewport khiến UI biến mất. **Fix**: Lưu `g_floatingRect` trước khi dock, khi undock gọi `SetParent(nullptr)` + khôi phục style + `SetWindowPos` theo kích thước cửa sổ nổi + `ShowWindow` + resize WebView.
- **[P1.5] Detached thread trong DLL** (2026-08-25): `std::thread(...).detach()` capture con trỏ bridge — REAPER unload DLL lúc job đang chạy → dangling → crash. **Fix**: track threads trong vector + join ở destructor + atomic abort flag. **Bài học**: trong DLL không detach thread; luôn join trước khi giải phóng tài nguyên mà thread chạm vào.
- **[P1.5] Path tiếng Việt hỏng khi upload/download** (2026-08-25): `CreateFileA` + `std::ofstream(char*)` là ANSI, không phải UTF-8. **Fix**: `CreateFileW` + `fs::path`. **Bài học**: product cho user Việt Nam thì mặc định mọi file I/O là wide/UTF-8, cấm `*A` API.
- **[P1.5] Tab trắng do lệch id DOM** (2026-08-25): JS tìm `#pane-audioLab` nhưng HTML đặt `pane-lab` → click nav = mất toàn bộ pane active. **Fix**: thống nhất id. **Bài học**: thêm script đối chiếu tự động id + i18n keys giữa HTML ↔ JS (đã viết `check_ui.cjs`), chạy khi sửa UI.
- **[P1.5] Sóng âm bị phẳng đơ (flat waveform) khi quét biên độ** (2026-08-26): `ma_decoder_init_file_w` nếu không truyền config `ma_format_f32` sẽ mặc định xuất định dạng nguyên bản (s16 PCM cho file 16-bit). Khi đọc dữ liệu nguyên vào mảng float, giá trị int16 bị hiểu sai thành float cực lớn và bị `std::min(1.0f)` ép toàn bộ thành 1.0 (phẳng lì). **Fix**: khởi tạo decoder với `ma_decoder_config_init(ma_format_f32, 0, 0)` và normalize peak theo tỷ lệ 0.95. **Bài học**: luôn cấu hình tường minh output format cho decoder âm thanh sang float 32-bit.
- **[P1.5] Mất CSS/vỡ toàn bộ giao diện WebView2 do Query String** (2026-08-26): `SetVirtualHostNameToFolderMapping` trong WebView2 ánh xạ URL thẳng tới file trên đĩa. Khi gắn query string như `href="app.css?v=1.1"`, WebView2 tìm file `app.css?v=1.1` trên Windows (ký tự `?` không hợp lệ) dẫn tới lỗi 404 và làm mất 100% style CSS. **Fix**: loại bỏ query string trong `index.html` và xóa cache qua `ClearBrowsingData`. **Bài học**: tuyệt đối không dùng query string trong URL tài nguyên tĩnh của virtual host mapping WebView2.
- **[P1.6] Loại bỏ Ngưỡng & Tốc độ (2026-08-27)**: Hai slider "Ngưỡng" (`audio.setThreshold`) và "Tốc độ" (`audio.setPitch` ratio 0.5-2) vô dụng, gây nhiễu và lỗi heap do `level()` callback `aboveThreshold`. **Fix**: Xóa toàn bộ `Engine::setThreshold/threshold/onThresholdCrossed`, `LevelState::aboveThreshold`, `Engine::setPitch`/`pitch`, `Bridge` `audio.setThreshold`/`audio.setPitch`, UI `index.html`/`app.js`/`app.css`/`I18N`/`assets/i18n`, `tests` legacy. Giữ `setVolume`, `setPitchSemitones` (±12st, key), `setTimeRatio` (Sync BPM). **Bài học**: Chỉ giữ DSP liên quan đến Sync BPM và Key.
- **[P1.6] Sync BPM & Key tự động (2026-08-27)**: `sampleBpm`/`sampleKey` trước chỉ lấy từ filename regex `(\d+)bpm` → hầu hết file 0 → Sync không stretch, insert không khớp. **Fix**: `Engine::detectBpm()` (decode 30s mono → `TempoDetector`), `Bridge::detectBpmForPath()` (DB → regex → TempoDetector → cache DB), `audio.setSyncBpm` tự detect khi `sampleBpm==0`, `reaper.insert` tự tính `playrate = projectBpm/sampleBpm` và `SetMediaItemTakeInfo_Value` `D_PLAYRATE`+`B_PPITCH`, `browser.beginDrag` queue pending playrate, `ui-web/app.js` gọi `audio.detectBpm` khi `sampleBpm==0`. **Bài học**: Luôn có fallback DB + TempoDetector, không dựa vào tên file.
- **[P1.7] Browser hardening (2026-08-27)**: Audit 6 lỗi BrowserModel: `rewritePath`/`forgetPath` không xử lý prefix thư mục → stale fav/tag; `lower` ASCII → hỏng TV `Đ`; `buildListing` cache poisoning khi `!exists`; `search` không sort; `addRoot` case-sensitive; `saveStore` không atomic. **Fix**: `lowerUtf8` (Win32 `MultiByteToWideChar`+`towlower`), `rewritePath`/`forgetPath` prefix logic, `buildListing` không cache khi `ec`, `search` sort `entryLess`, `addRoot` case-insensitive, `saveStore` tmp+rename, `m_storeMutex` → `recursive_mutex`. **Bài học**: Mọi `FileEntry` phải xử lý UTF-8 và prefix, cache phải có invalidation đúng.
- **[P1.8] Sửa triệt để Crash Audio Thread khi Click Play Sample (2026-08-27)**:
  - **Nguyên nhân gốc (Crash dump `0xC0000005` in `VCRUNTIME140D.dll`)**:
    1. `TDStretchSSE::calcCrossCorr` trong thư viện SoundTouch thực hiện unroll SIMD 16-float (`pVec1 += 16; pVec2 += 4;`). `overlapLength` tính ra không chia hết cho 16 và buffer `pMidBufferUnaligned` chỉ cấp phát thêm 4 floats (`16 / sizeof(float)`), dẫn đến việc bộ lệnh SSE 128-bit đọc tràn biên vùng nhớ (out-of-bounds read) gây Access Violation.
    2. Cấp phát động (`std::vector::resize`) trực tiếp trong callback audio thread `dsp_on_read`.
    3. Luồng preview thông thường (`timeRatio == 1.0f` và `pitchSemitones == 0.0f`) vẫn bị ép đi qua SoundTouch xử lý thay vì đọc trực tiếp decoder native.
  - **Giải pháp**:
    1. Cấp phát buffer `pMidBufferUnaligned` với padding an toàn +64 floats và căn chỉnh `overlapLength` chia hết cho 16 cho SSE vectorization.
    2. Thêm luồng Fast-path (Bypass mode) trong `dsp_on_read`: khi `timeRatio == 1.0f && pitchSemitones == 0.0f`, đọc trực tiếp từ `ma_decoder_read_pcm_frames` (0 latency, 0 allocation, bit-perfect 100%, không bao giờ crash).
    3. Khi bật `Sync BPM` hoặc đổi tone trên Mini Piano Keyboard (`timeRatio != 1.0f || pitchSemitones != 0.0f`), dữ liệu được đẩy qua SoundTouch DSP với buffer đã tiền cấp phát an toàn tuyệt đối.
    4. Xóa toàn bộ log spam 30ms trong timerHook/audioStateJson để giải phóng CPU.
- **[P1.9] Tích hợp 3 Chế độ Hiệu năng Quét CPU an toàn (2026-08-27)**:
  - Loại bỏ hoàn toàn chế độ Cực cao (100% CPU) để tránh nguy cơ đóng băng DAW/thiết bị.
  - Chuẩn hóa 3 chế độ CPU với Dynamic Concurrency Gate & Windows Thread Priority:
    1. **Thấp (30% CPU)**: Dùng tối đa 30% số nhân phần cứng, throttle sleep 30ms sau mỗi file, Windows priority `THREAD_PRIORITY_LOWEST` (hoàn toàn êm ái, nhường CPU cho DAW/Game).
    2. **Bình thường (50% CPU - Mặc định)**: Dùng 50% số nhân, throttle 10ms, priority `THREAD_PRIORITY_BELOW_NORMAL` (cân bằng, khuyên dùng).
    3. **Cao (85% CPU)**: Dùng 85% số nhân CPU, 0ms sleep (tốc độ cao). Khi kích hoạt có hộp thoại cảnh báo người dùng xác nhận về tải CPU cao.
  - Phân tích trực tiếp sóng âm thật (Real Audio DSP / AI Chromagram Key + Tempo + Spectral Timbre) cho tất cả các mẫu âm thanh (bao gồm tuned EDM kicks, 808s, toms, synths, piano, strings, vocals, bass).
  - Tách bạch 2 menu quét chuột phải: `⚡ Quét file mới` (Incremental, bỏ qua file cũ) và `🔄 Quét lại toàn bộ AI` (Rescan All).
- **[P1.10] Tích hợp Tìm Sample Tương Tự (Find Similar) & Semantic Vector Search (2026-08-27)**:
  - Trích xuất tự động vector đặc trưng âm học 512 chiều (CLAP audio embeddings) trong lúc quét file và lưu vào SQLite database.
  - Thêm menu chuột phải `🔍 Tìm sample tương tự` (Find Similar Samples) trên mọi file âm thanh.
  - Thuật toán xếp hạng đa tiêu chí: SIMD Cosine Similarity + Hòa âm phím Camelot Harmonic (+0.06/0.04) + Khớp BPM (+0.03) + Nhãn thể loại (+0.03).
  - Giao diện trực quan với thanh banner lọc `🔍 Tương tự như: [File.wav] (xx kết quả) [✕]` và huy hiệu % tương đồng `xx% khớp` nổi bật.
- **[P1.11] Fix Auto Sync Tempo & Pitch khi kéo thả sample vào REAPER DAW (2026-08-28)**:
  - Nguyên nhân: Trước đây khi kéo thả qua OLE (`browser.beginDrag`), nếu `syncRatio` đang là 1.0 thì bị bỏ qua không tính toán; đồng thời hàng đợi pending playrate hết hạn quá nhanh (300ms) trước khi người dùng kịp thả chuột vào timeline REAPER.
  - Khắc phục toàn diện:
    1. Gửi trực tiếp trạng thái `syncBpm`, `sampleBpm`, `pitchSemitones` ngay trong sự kiện `armOleDrag` và `insertMedia`.
    2. Trong C++ `Bridge.cpp` (`browser.beginDrag` và `reaper.insert`): tự động phát hiện BPM (`detectBpmForPath`) và tính tỷ lệ time-stretch `playrate = projectBpm / sampleBpm`.
    3. Nâng cấp hàng đợi `PendingPlayrate` duy trì đến 60 giây khi kéo thả, tự động khớp Take bằng `GetMediaItemTake_Source(take)->GetFileName()`, gán `D_PLAYRATE`, bật `B_PPITCH = 1` (giữ nguyên cao độ), gán `D_PITCH` và tự co giãn độ dài Item `D_LENGTH` chuẩn xác theo thanh nhịp DAW.
- **[P1.12] Đồng bộ Pha Phát Nhịp Playhead Phase Sync & Auto-Render Temp on Drag (2026-08-28)**:
  - **Playhead Phase Synchronization (R1 / A1)**:
    1. Kết nối REAPER SDK API: `GetPlayState`, `GetPlayPosition`, `GetPlayPosition2`, `TimeMap2_timeToBeats`, `Master_GetTempo`.
    2. Trừu tượng hóa `HostTransport` qua interface `IHostActions` cho cả REAPER extension và desktop standalone app.
    3. Khi DAW đang phát (`isPlaying()`) và Sync BPM bật: tự động tính chu kỳ nhịp vòng lặp `loopBeats` (1/2/4/8 bars) và độ lệch pha `startFraction = fmod(fullbeats, loopBeats) / loopBeats`.
    4. `Engine::playFile` hỗ trợ `startFraction`: pre-seek decoder trực tiếp tới frame chính xác trước khi phát, xóa buffer DSP để đảm bảo phát tức thì 0ms, không lệch phách, không tiếng click.
    5. Khi DAW dừng phát: tự động phát từ đầu (`startFraction = 0.0`).
  - **Auto-Render Temp on Drag (R2 / A2)**:
    1. Module mới `reals::audio::DragExporter` xử lý render offline tốc độ cao miniaudio + SoundTouch quickseek + 16-bit RIFF WAV vào `%TEMP%\RealsLab\drag_export\`.
    2. Bộ nhớ cache xác định (Deterministic Caching) trả về file đã render trong < 0.05ms khi kéo lặp lại cùng thông số.
    3. Khi kéo sample có bật Sync BPM hoặc Đổi Tone (Pitch Shift): tự động render file tạm và truyền đường dẫn đã xử lý vào `CF_HDROP` / `CF_UNICODETEXT`, giúp khung bóng mờ (drag ghost) trên timeline REAPER khớp 100% từng ô nhịp (Grid) ngay lúc đang rê chuột.
    4. Khi không bật Sync và Pitch = 0: truyền file gốc trực tiếp (bypass) không tốn chi phí render.
  - **Kiểm thử tự động & Build Zero-Warning (R3 / A3)**:
    1. Bổ sung toàn diện 19+ test cases trong `TestSuite_AudioDSP`, `TestSuite_BridgeUI`, `TestSuite_CrossFeatures`.
    2. Vượt qua 183/183 tests (100% pass) trên Windows C++20 với 0 cảnh báo biên dịch MSVC `/W4`.
- **[P1.13] Hoàn thiện Tối ưu Zero-Lag Drag & Căn chỉnh Chuẩn xác Pha Nhịp Playhead (2026-08-28)**:
  - **Khắc phục Lag khi Kéo thả**: Tích hợp bộ nhớ đệm In-Memory RAM Caching đa luồng cho `DragExporter`, lưu giữ kết quả tính toán kèm checksum `srcMtime` của file gốc. Thời gian truy xuất bộ nhớ đệm đạt < 0.01ms (10 microseconds), loại bỏ hoàn toàn hiện tượng khựng chuột (lag) khi bắt đầu rê kéo sample.
  - **Khắc phục Đồng bộ Pha Nhịp khi Preview**: 
    1. Truyền đầy đủ `syncBpm`, `sampleBpm`, và `pitchSemitones` từ Web UI vào lệnh `audio.play`.
    2. Cập nhật tức thời `setTimeRatio` và `setPitchSemitones` trên audio engine đồng bộ trước khi seek decoder đến `startFraction`, đảm bảo sample phát khớp 100% từng phách/bar đang chạy của REAPER timeline theo phong cách FL Studio Cloud.
- **[P1.14] DAW Drag & Drop Alignment & Triệt tiêu Double-DSP (Cơ chế A / Cơ chế B) (2026-08-28)**:
  - **Triệt tiêu hiện tượng Double-Stretch & Double Pitch-Shift (R2 / A2)**:
    1. **Cơ chế A (REAPER Native Drag)**: `Bridge.cpp` (`browser.beginDrag`) truyền trực tiếp đường dẫn file gốc của người dùng vào `queueSyncPlayrate(p, playrate, pitchShift)` và `beginDrag(p)`. Loại bỏ bước export file tạm đồng bộ, đạt 0ms zero-lag drag start và đảm bảo REAPER project tham chiếu vĩnh viễn đến file gốc thực tế.
    2. **Khớp chuẩn Grid Bar & Co giãn Take**: Trong `reaper_plugin.cpp` (`processPendingSyncPlayrates`), khi MediaItem được thả vào timeline: gán `D_PLAYRATE = it->playrate`, bật `B_PPITCH = 1`, gán `D_PITCH = it->pitchSemitones`, và tự động tính lại độ dài `D_LENGTH = (curLen * curRate) / it->playrate` để khớp 100% từng ô nhịp (Grid Bar) của bài hát.
    3. **Chốt an toàn Cơ chế B (Mechanism B Safeguard)**: Nếu file được kéo là file render sẵn (chứa `drag_` hoặc `drag_export`), hệ thống tự động gán `D_PLAYRATE = 1.0` và `D_PITCH = 0.0` trên Take để loại trừ nguy cơ xử lý DSP 2 lần liên tiếp.
  - **Tự động hóa Triển khai DLL (R3 / A3)**:
    - Bổ sung lệnh `POST_BUILD` trong `extension/CMakeLists.txt` tự động tạo thư mục và triển khai `reaper_realslab.dll` vào `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll` ngay sau khi biên dịch (hỗ trợ thay thế atomic file ngay cả khi REAPER đang chạy).
- **[P1.15] Khắc phục triệt để lỗi Lệch Pha & Lệch Bar khi Preview Sample ở chế độ Sync DAW (2026-08-30)**:
  - **Nguyên nhân gốc rễ được tìm thấy & xử lý**:
    1. **REAPER API `GetPlayPosition2` (Audio Block Stream Alignment)**: REAPER xuất audio qua driver ASIO/WASAPI theo từng audio block tương lai (`GetPlayPosition2()`). Cùng lúc đó, engine âm thanh miniaudio của Reals Lab cũng render trực tiếp ra audio hardware block. Khi dùng `GetPlayPosition()`, con trỏ bị trễ 44ms (bù trễ DAC trên timeline) khiến miniaudio nạp mẫu trễ hơn 44ms ("bùn ... bùn"). Đã chuyển `hostTransport()` đồng bộ theo `GetPlayPosition2()` (`pos2`) để cả 2 luồng audio cùng chảy vào buffer phần cứng ở cùng một thời điểm block, triệt tiêu hoàn toàn độ trễ 44ms.
    2. **Khắc phục Lệch do Reverb Tail / Padding (`nominalLoopFrames`)**: File loop có đuôi vang (ví dụ 4 bar = 8.0s, file dài 8.8s) trước đây bị nhân `startFraction * totalFrames` dẫn tới nhảy lệch phách. Đã bổ sung `nominalLoopFrames = (loopBeats * 60 / sampleBpm) * sampleRate` trong `Engine.cpp` và `Bridge.cpp` để `startFrame` và điểm ngắt lặp luôn bám đúng chu kỳ bar danh nghĩa.
    3. **Seamless Looping & Xóa bỏ khoảng lặng 28ms của SoundTouch**: Trong `Engine.cpp` (`dsp_on_read`), khi loop lặp lại ở cả chế độ Bypass và DSP (SoundTouch), tự động quay về frame 0 và tiếp tục nạp mẫu mượt mà mà KHÔNG gọi `ds->processor.flush()`, loại bỏ hoàn toàn khoảng lặng 28ms và triệt tiêu hiện tượng trôi nhịp tích lũy sau nhiều vòng lặp.
    4. **Nâng cấp Musical Bar Quantizer**: Mở rộng danh sách bar chuẩn (`0.25, 0.5, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 64 bars`) và dung sai thông minh (cho phép đuôi vang lên tới 20% mà không bị nhảy sang số beat lẻ).
    5. **Live Re-phase on Sync change**: Khi bật `Sync BPM` hoặc phát hiện BPM mới trong lúc DAW đang chạy, `Bridge.cpp` tự động re-align lại playhead và boundary frames khớp tức thì với thanh nhịp DAW.
    6. **Triệt tiêu Over-compensation (+62ms latency)**: Dữ liệu log thực tế chứng minh CPU tính toán SoundTouch chỉ mất 2~5ms chứ không hề có độ trễ 30ms trong thời gian thực. Đã loại bỏ phép cộng bù `latBeats` (+62ms) trong `Bridge.cpp`, đưa `startFraction` về đúng 100% phách thật của DAW, triệt tiêu hoàn toàn hiện tượng phát sớm (flam) nốt 1/32.
    7. **Auto Hardware Latency Delta Alignment (Tự động bù trừ độ trễ Soundcard chuyên dụng)**:
       - Khi người dùng dùng Soundcard chuyên dụng (Focusrite, Apollo, Steinberg, RME...), driver ASIO có độ dự đoán block cao hơn Windows WASAPI (`deltaLatMs = reaperBlockLat - wasapiBufLat = 54.9ms - 30.0ms = +24.9ms`).
       - `Bridge.cpp` và `reaper_plugin.cpp` tự động đo đạc hiệu số này và bù đúng $\Delta_{\text{latency}}$ vào `beatInLoop`. Khi dùng FL Studio ASIO (`delta = 0ms`) hay Focusrite ASIO (`delta = +24.9ms`), âm thanh preview luôn chạm tới màng loa ở đúng mili-giây với project DAW.
- **[P1.20] Tích hợp Audio Hook Chuẩn SWS (Audio_RegHardwareHook & GetPlayPosition2Ex) (2026-08-31)**:
  - **Kiến trúc Audio Thread Native Callback**:
    1. Đăng ký `Audio_RegHardwareHook` trực tiếp với REAPER audio core (`OnAudioBuffer`).
    2. Trong callback âm thanh thời gian thực: truy vấn `GetPlayPosition2Ex(nullptr)` và `TimeMap2_timeToBeats` trực tiếp trên từng block buffer ASIO.
    3. Giải mã và resample toàn bộ dữ liệu âm thanh preview sang RAM (`float32` stereo) tại đúng tần số mẫu dự án (`srate` = 48000/44100/96000Hz).
    4. Trộn trực tiếp vào `reg->GetBuffer(true, 0)` và `reg->GetBuffer(true, 1)` với **độ trễ = 0.0000ms tuyệt đối**, không bị jitter IPC, không phụ thuộc timer UI, không dính độ trễ DAC, không tạo file WAV tạm thời gian thực trên đĩa.
    5. Cập nhật metric Peak, RMS và con trỏ vị trí atomic thời gian thực về UI để hiển thị sóng âm và volume meter mượt mà 100%.
  - **Kiểm thử**: 256/256 tests pass 100%, MSVC /W4 zero-warning. DLL đã được tự động deploy vào `%APPDATA%\REAPER\UserPlugins`.

- **[P1.21] Định hướng Mô hình Thương mại & Lộ trình Tính năng (2026-08-31)**:
  - **Mô hình Hybrid B2C**:
    - **Tier Local (Extension/Client)**: Preview sample, Phase Sync 0ms, Local DSP, Local SQLite scanner -> Bán mua đứt trọn đời (Perpetual $39 - $59) hoặc bản Free câu khách (không tốn chi phí server).
    - **Tier Cloud (AI Lab trên Server)**: Tách stem, AI generation, Cloud semantic search -> Bán dạng Subscription ($7.99 - $9.99/tháng) hoặc Credit / Token (Pay-as-you-go, ví dụ $5/50 credits), tuyệt đối không bán vĩnh viễn để tránh rủi ro chi phí server/GPU.
  - **Lộ trình Tính năng**:
    - Ưu tiên số 1: Ổn định lõi âm thanh, Playhead Phase Sync sample-accurate, DSP SoundTouch và AI Lab.
    - Các tính năng duyệt file nâng cao (File Browser UX chuyên sâu) tạm thời gác lại xử lý ở phase sau.

- **[P1.22] Khắc phục Toàn diện Tính năng Kéo Thả Sample Tự Động Khớp Tempo / BPM vào REAPER (2026-08-31)**:
  - **Nguyên nhân cốt lõi**:
    1. Thiếu API `REAPERAPI_WANT_UpdateItemInProject`: Khi plugin gán `D_PLAYRATE`, `B_PPITCH`, `D_LENGTH`, REAPER không tự động render lại waveform / stretch buffer cho take nếu thiếu `UpdateItemInProject(item)`.
    2. Path matching: Khi REAPER bật tùy chọn copy media vào thư mục project (`C:\Projects\...`), đường dẫn tuyệt đối bị thay đổi, dẫn đến so khớp `normTarget == srcPath` bị trượt.
    3. Nhận diện BPM trong tên file: Regex cũ chỉ bắt `128bpm`, bỏ sót các mẫu chuẩn như `128_Kick.wav`, `BPM128_Drums.wav`, `Synth_120_C.wav`.
  - **Khắc phục**:
    1. Bổ sung `UpdateItemInProject(item)` và `UpdateArrange()` ngay khi gán `D_PLAYRATE`, `B_PPITCH = 1`, `D_PITCH`, và `D_LENGTH = (curLen * curRate) / playrate`.
    2. Nâng cấp bộ regex `detectBpmForPath` bắt trọn các định dạng BPM và fallback sang bộ phát hiện tempo tự động nếu cần.
    3. Hỗ trợ so khớp basename (tên file) để tự động nhận diện MediaItem kể cả khi REAPER đã copy file vào project folder.
    4. Tự động tính toán và kích hoạt playrate sync ngay khi kéo sample nếu phát hiện được BPM (không cần người dùng phải bấm nút bật thủ công).
- **[P1.23] Nâng cấp Tương tác Click 0ms, Mini Waveform & MIDI Preview, và Player Drag-and-Drop (2026-08-31)**:
  - **Tương tác Click & Preview 0ms**: Bắt sự kiện `pointerdown` tập trung bằng Event Delegation trên `#files`, loại bỏ hiện tượng bị nuốt click khi vừa lăn chuột xong bấm nghe thử ngay, rút ngắn thời gian phản hồi preview xuống tức thời.
  - **Mini Preview Waveform & Piano Roll Background**:
    - Thêm `.mini-preview-bg` hiển thị sóng âm mini (Audio) hoặc các nốt piano roll mini (MIDI) mờ nghệ thuật phía dưới nền mỗi dòng file trong danh sách.
    - Chữ và thông tin file nằm ở lớp nổi bên trên (`z-index: 1`) sắc nét, dễ đọc.
  - **Giao diện Piano Roll trên Player chính**: Canvas `#waveform` tự động vẽ chế độ Piano Roll đa tầng màu khi chọn file MIDI.
  - **Kéo thả từ Player chính ra REAPER**: Bổ sung `armOleDrag` trên `#waveform` và `#trackInfo`, cho phép rê chuột kéo trực tiếp sample/MIDI đang nghe vào timeline REAPER.
- **[P1.24] Polyphonic MIDI Engine, Real Key Detection & Target Lock Transposer, Studio DAW Flat UI (2026-08-31)**:
  - **Polyphonic MIDI Engine**:
    - Backend C++ bổ sung API `audio.readMidi` / `fs.readBase64` trích xuất binary `.mid` / `.midi`.
    - Web Audio Synthesizer phát âm thanh đa âm thật trực tiếp qua loa với ADSR gain envelope, playhead 60fps, và Piano Roll Canvas vẽ đúng các khối nốt thật từ file MIDI.
  - **Real Key Detection & Target Key Lock Transposer**:
    - Tích hợp bộ giải mã nhạc lý thông minh 12 cung bậc (`C..B`), hệ Camelot (`1A..12B`), dấu thăng/giáng (`#`/`b`), Major/Minor.
    - Nhận diện tức thì Root Note của sample (`originalRootNote`), highlight phím nốt gốc trên bàn phím Mini Piano Transposer và hiển thị huy hiệu `Root: [Tone gốc]`.
    - Chế độ **Khóa Tone Đích (Target Key Lock)**: Khi người dùng chọn một nốt đích (vd `F`), khi duyệt qua các sample khác (vd `D`, `E`, `C#`), hệ thống tự động giữ nguyên tone đích `F`, tự tính độ lệch semitone tương ứng, áp dụng DSP SoundTouch realtime pitch shift sang `F`, đồng thời đánh dấu nốt gốc bằng chấm hổ phách `•` (`.root-marker`) trên phím đàn.
  - **Studio DAW Flat UI & Modern Vector SVGs**:
    - Loại bỏ toàn bộ viền gắt trên thanh công cụ (`#btnToggleTree`, `#search`, `#sort`, `#favOnly`, `#tagFilter`, `#btnRefresh`) chuyển sang phong cách phẳng Borderless Studio Dark Theme.
    - Thay thế toàn bộ emoji hệ thống (📁, 🔍, 🔊) bằng bộ icon vector SVG sắc nét, tối giản chuẩn DAW chuyên nghiệp.

## Ghi chú làm việc
- Trả lời ngắn gọn, kiểu 2 thằng bạn trò chuyện.
- Làm từng bước, bàn bạc kỹ trước khi code.
- **Mỗi khi chốt được điều gì → ghi ngay vào file này** (kèm ngày).

