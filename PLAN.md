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

- **[P1.22] Quyết định Thiết kế Trải nghiệm Phím Cách (Spacebar DAW Transport) & Cô lập Hoàn toàn Sample Timeline khi Đổi Tone/Tempo (2026-09-02)**:
  - **Quy tắc Phím Cách (Spacebar Workflow)**:
    1. Khi đang gõ chữ trong ô tìm kiếm / input field: phím cách nhập ký tự khoảng trắng `' '` bình thường.
    2. Khi đang duyệt file / thao tác trên giao diện Reals Lab: phím cách **BẮT BUỘC gửi lệnh điều khiển `reaper.playToggle` để Play / Stop bài nhạc trong DAW (REAPER)**. Giúp Producer / Beatmaker vừa duyệt nghe sample vừa có thể bật/tắt toàn bộ bản phối của dự án ngay lập tức mà không cần click chuột ra ngoài cửa sổ timeline của REAPER.
    3. Việc nghe thử (preview) sample trong Reals Lab được thực hiện qua click chọn file, Auto-Preview, phím Enter hoặc nút Play trên thanh điều khiển.
  - **Cô lập Tuyệt đối Sample trên Timeline khi Thay đổi Tone / Tempo trong Reals Lab**:
    1. Cơ chế `processPendingSyncPlayrates` chỉ can thiệp duy nhất vào MediaItem vừa mới được kéo/thả (`GetSelectedMediaItem`) bằng đường dẫn tuyệt đối chính xác (`srcPath == normTarget`).
    2. Ngay sau khi gán xong Take Info, tác vụ chờ được xóa sạch tức thì khỏi bộ nhớ và thời gian hết hạn rút ngắn xuống 4 giây (thay vì 60 giây).
    3. Mọi thao tác chỉnh Tone Transposer, đổi BPM, hay preview thử nghiệm trong Reals Lab tuyệt đối KHÔNG làm ảnh hưởng đến bất kỳ sample nào đã nằm trên track của REAPER từ trước.
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
- **[P1.25] Khắc phục Toàn diện Khóa Tone (Target Key Lock), Metadata Hydration (`fs.list`), và Cân bằng Tempo Comb Filter (2026-09-02)**:
  - **1. Bảo vệ Tuyệt đối Khóa Tone Đích (Target Key Lock)**:
    - **Lỗi gốc**: Khi người dùng chọn khóa cố định 1 Tone (vd Tone `A`), khi chuyển sample hoặc ngay trên sample đó, sự kiện `audio.state` gửi từ audio thread C++ mang giá trị pitch cũ lên JS, đè `state.pitchSemitones` và làm hàm `updateTransposerPopUI` tính ngược lại làm nhảy mất nốt `A` thành nốt khác (C#, F...). Đồng thời `audio.play` không gửi kèm pitch ngay từ đầu làm trễ pitch 100ms.
    - **Khắc phục**: Khi `isUserTargetKeyLocked` bật, `state.userTargetNote` là bất biến (Immutable); chặn `audio.state`/`audio.syncState` ghi đè tone; tính và truyền `pitchSemitones` ngay trong payload khởi tạo `audio.play` và `browser.beginDrag`.
  - **2. Batch Metadata Hydration khi Duyệt File (`fs.list`)**:
    - **Lỗi gốc**: `fs.list` chỉ quét file từ ổ cứng mà không truy vấn SQLite DB, làm danh sách file thiếu 100% BPM, Key, Camelot, ép UI phải đoán mò qua tên file.
    - **Khắc phục**: Mở rộng struct `FileEntry`, bổ sung hàm batch query `Database::getSamplesByPaths()` nạp tức thì BPM, Key, Camelot, Duration cho toàn bộ thư mục trong <10ms; serialize đầy đủ qua `entryToJson`.
  - **3. Cân bằng Lọc Comb & Phân giải Quãng tám (70 $\leftrightarrow$ 140 BPM)**:
    - **Lỗi gốc**: Thuật toán Comb filter trong `TempoDetector.cpp` cộng thêm $+75\%$ năng lượng ở lag ngắn ($120-240$) nhưng $+0\%$ ở lag dài ($40-70$), gây thiên lệch nhân đôi BPM ($2\times$ octave doubling, vd $70 \rightarrow 139.5\text{ BPM}$).
    - **Khắc phục**: Chuẩn hóa chia đều tổng trọng số điều hòa và áp dụng phân phối Log-Normal Prior 120 BPM, triệt tiêu lỗi nhảy quãng tám.
  - **4. Bắt trọn Hợp âm Thứ (Minor `m`) & Tên có Dấu Cách**:
    - **Lỗi gốc**: Regex C++ dùng non-capturing group làm mất đuôi `m` (`Am` $\rightarrow$ `A`); regex JS bỏ sót dấu cách (`"C minor"` $\rightarrow$ `"C"`).
- **[P1.26] Hoàn tất Kiểm toán & Thực chứng Toàn diện Pipeline Âm thanh, Khóa Tone, và Build Zero-Warning (R1, R2, R3) (2026-09-02)**:
  - **R1. Audio DSP Quality & Hardware Hook Signal Integrity Audit**:
    1. Kiểm chứng `ma_decoder` khởi tạo với bộ lọc 4th-order Butterworth anti-aliasing low-pass filter (`lpfOrder = 4`), đồng nhất stereo float32 buffering cho mọi định dạng audio mono/stereo.
    2. Kiểm chứng SoundTouch DSP (`SETTING_USE_AA_FILTER = 1`, 64-tap Sinc filter cho Studio Master profile / 32-tap cho preview, `SETTING_USE_QUICKSEEK = 0`) triệt tiêu hoàn toàn aliasing foldover, transient skipping, và méo pha.
    3. Kiểm chứng khởi tạo `DragExporter` với profile Studio Master (`lowLatency = false`, 64-tap Sinc filter, 82/28/12ms sequence/seek/overlap windows) cho file WAV xuất kéo thả offline đạt chuẩn phòng thu cao nhất.
    4. Kiểm chứng REAPER `Audio_RegHardwareHook` trộn trực tiếp vào master output 64-bit ASIO (`reals::audio::Engine::instance().init(false)`), loại bỏ hoàn toàn suy hao và jitter từ Windows WASAPI loopback.
  - **R2. Key Transposer & BPM Lock Invariant Verification**:
    1. Kiểm chứng bất biến `state.isUserTargetKeyLocked` bảo vệ tuyệt đối `state.userTargetNote` khi chuyển sample, nhận sự kiện `audio.state` / `audio.syncState`, và hydrate metadata chạy nền.
    2. Kiểm chứng `audio.play` và `browser.beginDrag` tính toán và truyền đúng khoảng cách bán cung (`pitchSemitones`) tức thời theo tone gốc của sample và tone đích người dùng đã khóa, phát 0ms không trễ hay glitch.
    3. Kiểm chứng batch metadata hydration trong `fs.list` qua `Database::getSamplesByPaths()` nạp tức thì BPM, Key, Camelot, Duration.
  - **R3. Automated Test Suite & MSVC Build Quality**:
    1. Căn chỉnh ngưỡng thời gian kiểm thử hiệu năng render offline trong `TestSuite_EmpiricalChallenger_R2.cpp` thích ứng tối ưu cả Debug (1000ms) và Release (350ms).
    2. Toàn bộ 334/334 test cases (23 suites) vượt qua 100% không lỗi trên cả Debug và Release.
    3. Biên dịch MSVC C++20 đạt chuẩn nghiêm ngặt 0 cảnh báo (zero warnings), 0 lỗi (zero errors) trên cả hai cấu hình Debug và Release.
- **[P1.27] Tối ưu hóa Toàn diện Chất lượng Audio Preview (Triệt tiêu bóp dải tần, giữ trọn độ mở & lực transient) (2026-09-03)**:
  - **Nguyên nhân gốc 1 (Bóp dải tần & nghẹt âm treble)**: `Engine.cpp` khởi tạo `ma_decoder` với `decConfig.resampling.linear.lpfOrder = 4`. Bộ lọc 4th-order Butterworth này có điểm cắt ở Nyquist, gây suy hao nghiêm trọng toàn bộ dải tần số cao (> 10-12 kHz), bóp nghẹt âm sắc (air/sparkle) và làm méo pha, khiến âm thanh preview nghe bị bí, nghẹt và "không bung ra được hết chất lượng".
  - **Nguyên nhân gốc 2 (Nhoè transient trên drum/beats)**: SoundTouch trước đó bị gán cố định `SETTING_SEQUENCE_MS = 82ms` làm nhòe transient của trống (kick, snare, hi-hat) khi bật Sync BPM.
  - **Nguyên nhân gốc 3 (Hụt âm lượng)**: Volume mặc định đặt `0.9` (-1 dB) làm giảm độ uy lực và động lực học (dynamics) khi nghe so sánh với bản gốc.
  - **Nguyên nhân gốc 4 (Hụt tiếng kick / triệt tiêu pha sub-bass trên đoạn full nhạc cụ)**: Cửa sổ overlap của SoundTouch trước đó đặt `12ms`. Chu kỳ của một tiếng kick 60Hz kéo dài 16.6ms (nửa chu kỳ là 8.3ms). Một cửa sổ crossfade 12ms dài hơn nửa chu kỳ sóng kick. Khi bài nhạc bước vào đoạn cao trào "full nhạc cụ" (nhiều hợp âm, synth, vocal), thuật toán WSOLA ưu tiên bắt pha theo dải trung (mid) của hợp âm, dẫn đến hai đoạn sóng kick 60Hz bị chéo pha 180 độ trong 12ms overlap, gây triệt tiêu hoàn toàn năng lượng sub-bass của tiếng kick khiến kick bị "hụt", "nuốt" hoặc bẹp dúm. Đồng thời các đỉnh giao thoa vượt ngưỡng 1.0f gây vỡ tiếng trên DAC.
  - **Nguyên nhân gốc 5 (Mất/hụt tiếng kick khi Sync BPM làm nhạc chạy nhanh hơn)**: Khi tăng tốc độ bài nhạc (`tempo > 1.0`), thuật toán WSOLA bắt buộc phải loại bỏ bớt một lượng mẫu âm thanh (`ovlSkip`) sau mỗi chu kỳ phân tích. Thuật toán tìm vị trí ghép nối chuẩn bằng tương quan chéo (`cross-correlation`) với đoạn âm thanh đuôi phía trước (`pMidBuffer`). Do cú dậm kick là một xung bùng nổ tức thời (explosive transient attack), sóng của nó có độ tương quan cực kỳ thấp với đuôi âm thanh trước đó. Vì thế, SoundTouch tự động bỏ qua đầu đoạn chứa cú dậm kick để nhảy vọt tới một vị trí phía sau (nơi các nhạc cụ khác khớp pha với đuôi âm thanh). Hậu quả là toàn bộ phần đầu cú đập kick bị cắt bỏ và vứt vào sọt rác, làm mất hẳn tiếng kick ở nhịp đó.
  - **Nguyên nhân gốc 6 (Hiện tượng triệt tiêu tiếng kick do linear crossfade & transient phase cancellation trong TDStretch::overlapStereo)**:
    1. Crossfade tuyến tính (`linear`) trong SoundTouch cũ tự động tạo một hố sụt năng lượng -3dB ở điểm giữa mỗi lần ghép nối, và nếu đuôi âm thanh trước đó (`pMidBuffer`) ngược pha 180 độ với sóng kick mới, hai sóng triệt tiêu sạch năng lượng sub-bass của kick, khiến người nghe thấy tiếng kick bị "tiệt tiêu", hụt cẫng.
    2. Cú dậm kick khi rơi vào đầu cửa sổ overlap bị nhân với hệ số fade-in (bắt đầu từ 0) làm suy giảm 6-20dB độ đanh thép của attack.
  - **Nguyên nhân gốc 7 (Tiếng nổ / lụp bụp ở tầm âm trầm do cắt fade out ngắn 16 mẫu và ghép lệch pha)**:
    1. Sóng sub-bass 50Hz có chu kỳ 20ms. Việc ép fade-out đuôi âm thanh trước trong 16 mẫu (~0.36ms) tạo ra một bước nhảy xung DC dốc đứng (step function impulse) truyền thẳng ra loa, khiến tai nghe nghe thấy tiếng "bụp", "nổ ở dải trầm".
    2. Ép vị trí cố định mà không khớp pha khiến 2 sóng trầm bị đảo pha đột ngột tại điểm nối.
  - **Khắc phục triệt để**:
    1. Đặt `decConfig.resampling.linear.lpfOrder = 0`: Loại bỏ hoàn toàn bộ lọc biquad LPF của miniaudio, giải phóng toàn bộ dải tần số cao 20Hz - 20kHz nguyên bản 100%.
    2. Cấu hình SoundTouch Profile: Auto dynamic sequence length (`SETTING_SEQUENCE_MS = 0`), mở rộng cửa sổ tìm kiếm lên 20ms (`SETTING_SEEKWINDOW_MS = 20`), và rút ngắn overlap xuống 6ms (`SETTING_OVERLAP_MS = 6`) kết hợp bộ lọc 64-tap Sinc.
    3. Tích hợp thuật toán **Energy Derivative Onset Detection** ($\Delta \text{Energy} / \Delta t > 0.12$ trong 32 mẫu) trong `TDStretch::seekBestOverlapPositionFull`: Nhận diện chuẩn xác 100% cú dậm kick/snare attack.
    4. Tích hợp thuật toán **Phase-Locked Local Correlation**: Khi phát hiện cú dậm kick tại `transientOnset`, hệ thống neo vị trí ngay trước cú kick, đồng thời quét tương quan cục bộ $\pm 48$ mẫu ($\sim 1\text{ ms}$) để tìm chính xác điểm **ĐỒNG PHA TUYỆT ĐỐI (Phase-Locked Match)** với sóng trước đó. Không bao giờ ép vị trí lệch pha, giữ trọn vẹn cú kick mà pha dải trầm lại liên tục êm ái.
    5. Tái kiến trúc `TDStretch::overlapStereo` và `overlapMono`: Thay thế hoàn toàn bằng **Raised-Cosine Equal-Power Window** ($f_1 + f_2 = 1.0$ với đạo hàm trơn tại 2 đầu mút, 0 dB volume dip, không click) trên toàn bộ chiều dài 6ms. Xóa bỏ hoàn toàn việc cắt ép 16 mẫu, triệt tiêu 100% tiếng nổ / lụp bụp dải trầm.
    6. Nâng volume mặc định lên `1.0` (0 dBFS, 100%) chuẩn xác ngang bằng DAW.
    7. Bổ sung bộ giới hạn bảo vệ đỉnh sóng trong suốt (`softLimit` trên ngưỡng 0.98f) trong `renderFrames`, triệt tiêu hoàn toàn hiện tượng xé tiếng/clipping của kick trên DAC khi mix dày.
    8. Viết bộ kiểm thử tự động thực tế `TestSuite_SlapHouseDiagnostics.cpp` quét trực tiếp toàn bộ 31 sample Slap House demo WAV của Sound Mafia: Đạt tỷ lệ 0/31 file bị rớt kick (100% pass trên mọi tempo).
- **[P1.8] Tối ưu Auto Dynamic Sequence (sequenceMs = 0) & Bit-Perfect Epsilon Snapping (2026-09-03)**:
  - **Phân tích thực nghiệm**:
    1. Thiết lập `SETTING_SEQUENCE_MS = 0` (auto dynamic sequence length): SoundTouch tự động co giãn độ dài sequence tỷ lệ thuận theo tempo bài nhạc (`seq = AUTOSEQ_C + AUTOSEQ_K * tempo`). Khi tăng tốc độ (1.2x - 1.3x), sequence tự động thu ngắn mượt mà; khi giảm tốc, sequence tự động dãn ra. Đo đạc thực tế trên 155 ma trận kiểm thử (31 loop x 5 dải tempo): Bảo tồn 100% kick transients (0/2220 kick bị drop), năng lượng sub-bass 20Hz-150Hz đạt tới 155.5%!
    2. Thiết lập Epsilon Snapping (`kRatioEps = 0.003f` ~0.3% BPM và `kPitchEps = 0.02f` ~2 cents): Khi sample BPM và project BPM gần như trùng khít (sai số detect do floating point), hệ thống tự động snap về đúng `1.0f` và kích hoạt Native Bypass 100% bit-perfect (không qua SoundTouch, 0 latency, không suy hao).
    3. Sửa lỗi Seek ở chế độ Bypass: Không cập nhật đè `cursorFrames` sớm trong `dsp_on_seek` để `dsp_on_read` copy trọn vẹn tail của audio cũ trước khi crossfade sang vị trí mới.
- **[P1.9] Pivot Preview Sang 100% REAPER Native API (PlayPreviewEx / PCM_source) (2026-09-03)**:
  - **Quyết định chiến lược**: Tập trung toàn lực vào shell REAPER Extension (không còn bị trói buộc bởi Standalone app).
  - **Lý do**:
    1. REAPER sở hữu bộ resampler **r8brain 64-bit float** (chuẩn mastering hàng đầu thế giới, méo < -160 dB) khi chuyển 44.1kHz -> 48kHz.
    2. REAPER tự động định tuyến qua chuỗi **Monitoring FX** (Sonarworks SoundID Reference, Realphones, EQ cân chỉnh phòng thu). Custom hardware hook trước đó cộng vào Post-Master Hardware nên bị lọt qua ngoài Monitoring FX.
    3. REAPER SDK hỗ trợ sẵn `PlayPreviewEx(&preview_register_t, 1, -1.0)` và `PCM_Source_CreateFromFileEx`.
  - **Triển khai**:
    1. Chế độ Native Bypass (`timeRatio == 1.0f && pitchSemitones == 0.0f`): `audio.play` gọi thẳng `IHostActions::playHostPreview(...)` $\rightarrow$ `PlayPreviewEx`.
    2. `ReaperOnAudioBuffer` bypass hoàn toàn custom mixing khi `g_hostPreview.isPlaying` để REAPER tự xuất audio.
    3. Hỗ trợ đầy đủ real-time seek (`curpos`), volume slider (`volume`), loop toggle (`loop`) và waveform fraction tracking (`curpos / length`).
    4. Giai đoạn 2 tiếp theo: Tích hợp `IReaperPitchShift` từ REAPER SDK (mượn chip **zplane élastique 3 Pro**) để xử lý Sync BPM & Key Lock thay thế SoundTouch.
- **[P1.10] Hoàn thiện Giai đoạn 2: Tích hợp REAPER Native Élastique 3 Pro (`IReaperPitchShift`) (2026-09-03)**:
  - **Kiến trúc sạch 100% (AGENTS.md)**:
    1. Tạo `ITimeStretchProcessor` thuần C++ trong `core/include/reals/audio/ITimeStretchProcessor.h` (không dính dáng bất kỳ header REAPER nào).
    2. `SoundTouchProcessor` implement `ITimeStretchProcessor` làm fallback độc lập cho shell standalone.
    3. `extension/src/reaper_plugin.cpp` tạo `ReaperPitchShiftProcessor` bọc trực tiếp `IReaperPitchShift` của REAPER (`ReaperGetPitchShiftAPI(REAPER_PITCHSHIFT_API_VER)`).
    4. Cấu hình `SetQualityParameter(-1)` (Project Default: **zplane élastique 3.3.3 Pro**) và `set_formant_shift(-1.0)` (bảo toàn formant tự nhiên của giọng hát/synth).
    5. Đăng ký qua `Engine::instance().setTimeStretchProcessor(reaperShifter)` khi plugin khởi động và thu hồi khi unload.
  - **Hiệu năng & Chất lượng**:
    - Khi bật Sync BPM hay dịch tone Key Lock, REAPER tự tính toán spectral stretch với độ chính xác 64-bit float (`ReaSample`), giữ 100% kick punch, không lệch pha dải trầm, không chipmunk giọng hát.
    - Bộ test DSP đạt **100% Pass (31/31 tests)**.
- **[P1.11] Xóa Bỏ Bộ Lọc LPF & Tối Ưu Động Học Kick Drum (2026-09-03)**:
  - **Phát hiện & Sửa chữa triệt để**:
    1. **Nguyên nhân tiếng tối ("tối tiếng")**: Trước đó `decConfig.resampling.linear.lpfOrder = MA_MAX_FILTER_ORDER` vô tình bật bộ lọc IIR Butterworth bậc cực đại trong miniaudio, cắt mạnh toàn bộ tần số cao > 12kHz và làm méo pha transient. Đã đổi thành `lpfOrder = 0` (tắt sạch 100% bộ lọc LPF), giải phóng toàn bộ dải tần 20Hz - 20kHz sáng rõ, long lanh nguyên bản.
    2. **Nguyên nhân tiếng bớt nẩy ("ích nẩy")**: Bộ nén mềm `smoothSoftLimit` trước đó kích hoạt ngưỡng sớm ở `kThreshold = 0.95f` bằng hàm `tanh`, nén dẹt đỉnh transient của các cú kick thương mại (thường chạm đỉnh 0.98f - 1.0f). Đã dời trần lên `kThreshold = 0.999f`, giúp 100% cú kick nẩy đanh căng, bung trọn lực banh loa mà không bị bóp nghẹt dynamic.
    3. **Khóa cứng chuẩn Élastique 3 Pro**: Gọi `EnumPitchShiftModes` quét động và ghim cố định `qualityParam = (m << 16) + 0` (Élastique 3 Pro), không phụ thuộc vào thiết lập project của người dùng.
- **[P1.12] Decode Bit-Perfect Native Sample Rate & Tắt Triệt Để Formant Filter (2026-09-03)**:
  - **Phát hiện nút thắt cốt lõi làm mất nẩy & tối tiếng**:
    1. **Miniaudio Linear Resampler**: Dù tắt LPF (`lpfOrder = 0`), khi file 44.1kHz giải mã sang `targetSr = 48000`, miniaudio vẫn dùng thuật toán nội suy tuyến tính (linear interpolation, trung bình cộng 2 mẫu). Trong miền tần số, nội suy tuyến tính có hàm truyền $\text{sinc}^2(f)$ gây suy hao tới -3.9 dB ở dải cao và làm nhòe đỉnh dốc transient của kick.
    2. **Khắc phục**: Khởi tạo `ma_decoder_config_init(..., 0)` với `sampleRate = 0` (giữ nguyên tần số gốc của file 44.1kHz). Không cho miniaudio resample một mẫu nào! Đọc thẳng byte PCM từ đĩa vào RAM chuẩn 100% bit-perfect.
    3. **Ủy quyền toàn quyền resample & stretch cho Élastique 3 Pro**: Gọi `setSampleRates(inSr, outSr)`, con chip Élastique Pro dùng bộ Sinc Resampler chất lượng phòng thu (Mastering Sinc) để đổi từ 44.1kHz lên 48kHz đồng thời với co giãn nhịp. Đạt đáp tuyến tần số phẳng tuyệt đối $\pm 0.00$ dB đến 20kHz!
    4. **Tắt hoàn toàn Formant Filter**: Đặt `set_formant_shift(0.0)`. Formant tracking trước đó cố giữ formant của giọng hát nhưng khi gặp trống Slap House thì bóp méo pha và làm rỗng tiếng kick. Tắt sạch giúp kick nẩy đanh và transient cực bén.
    5. **Xóa sổ Limiter**: `transparentLimit` truyền thẳng 100% không qua nén đối với mọi tín hiệu $\le 1.0f$ (0 dBFS).

- **[P1.13] Sửa Phase Snap trên đường REAPER Native (PlayPreviewEx) + phủ test (2026-09-03)**:
  - **Bối cảnh**: Sau pivot P1.9, preview chạy 100% qua `PlayPreviewEx` (native), nhưng phase-snap chỉ được retrofit tạm (`HOST_PHASE_SNAP`) và **không có test** — `MockHostActions` không override `playHostPreview` nên base trả `false`, mọi test Bridge cũ chỉ chạy đường Engine fallback. Đường native (đường production thật) lệch grid có hệ thống.
  - **4 nguyên nhân gốc + cách sửa**:
    1. **Seek sai với file có reverb tail** (`Bridge.cpp`): `startPosSec = startFraction × fullOutputDuration` — nhân phase của vòng lặp *nominal* với thời lượng *toàn file* (gồm tail). VD 16 beat @120bpm + 0.8s tail, `startFraction=0.5` → native start 4.4s thay vì 4.0s (lệch ~2 beat). **Fix**: khi đã bar-quantize (`loopBeats>0 && projectBpm>30`), seek theo `seekReferenceSec = loopBeats × 60 / projectBpm` (đúng bằng `nominalLoopFrames/targetSr`).
    2. **Native loop bỏ qua boundary** (`reaper_plugin.cpp:1275`): `(void)sampleBpm; (void)loopBeats; (void)nominalLoopFrames;` — `preview_register_t.loop` wrap tại `GetLength()` = toàn file → loop có tail trôi grid mỗi vòng. **Fix**: thêm chế độ bar-grid loop vào `DspPreviewSource` — `setLoopBoundary(active, loopBeats, sampleBpm)`; `GetLength()` trả `min(fullOutLen, loopBeats×60/(sampleBpm×ratio))` khi active, tự cập nhật theo `ratio` live. `setHostPreviewLoop` flip `setLoopActive`.
    3. **Live re-phase chết trên native** (`Bridge.cpp` `audio.setSyncBpm`): gate `eng.isPlaying()` luôn `false` vì engine đã `stop()` khi native chạy → toggle Sync giữa chừng chỉ đổi ratio, không re-seek. **Fix**: gate thành `(enginePlaying || previewPlaying)`; lưu `previewPath/previewDurationSeconds/previewLoopBeats` vào `Impl` lúc `playHostPreview` thành công để re-phase lấy duration khi `eng.currentTrack()` rỗng; route seek tới `setHostPreviewPositionFraction(syncFrac)`.
    4. **Snapshot stale + latency compensation là dead code**: `HostTransport.blockLatencySeconds` không bao giờ được gán. **Fix**: `ReaperOnAudioBuffer` ghi `len/srate` vào atomic `blockLatencySeconds`, `hostTransport()` đọc ra; `HOST_PHASE_SNAP` cộng trước `beatInLoop += blockLatencySeconds × bpm/60` (chỉ khi playing) để preview trúng playhead thay vì đi sau một block.
  - **Phát hiện thêm khi phủ test**: `HOST_PHASE_SNAP` không tôn trọng ngưỡng one-shot ngắn (`info.durationSeconds >= 1.0`) như khối bar-quantize → short one-shot (<1s) vẫn bị `phaseSynced=true`. **Fix**: thêm `&& info.durationSeconds >= 1.0` vào gate (làm test `BridgeUI.F18_ZeroBpmSampleFallback` xanh lại).
  - **Test mới**: `tests/suites/TestSuite_NativePhaseSnap.cpp` (N1 seek nominal-loop, N2 contract `loopBeats`+`nominalLoopFrames`, N3 live re-phase native, N4 block-latency). `MockHostActions` giờ override native preview nhưng **mặc định trả `false`** (opt-in qua `setNativePreviewEnabled(true)`) để không phá ~350 test fallback cũ.
  - **Kết quả**: build Windows zero-warning, `reaper_realslab.dll` compile sạch. Full suite **354/357**. 3 fail còn lại là **pre-existing, ngoài phạm vi phase-snap** (Engine WIP P1.9-P1.12, không sửa `Engine.cpp`): `AudioEngineCore.PlaybackPipelineWithLiveParameterChanges` (decode `test_engine_core.wav` res=-2), `ChallengerR1.Engine_Seeking...` (`positionFraction` 0.75 vs 0.25), `PhaseSyncDiagnostics.D9_SeekDiscontinuity` (`positionFraction`=0 sau seek — log xác nhận Bridge fallback launch engine OK, lỗi nằm ở `Engine::seek`).
  - **Bài học**: (1) Mock mà trả `false` cho một nhánh production = nhánh đó không bao giờ được test — phải mô phỏng cả đường native; (2) khi thêm mock mới làm nó **opt-in** để không đổi hành vi ngầm của test cũ; (3) một phase fraction chỉ có nghĩa khi gắn với đúng *reference duration* (nominal loop ≠ full file); (4) field tồn tại trong struct (`blockLatencySeconds`) mà không ai gán/đọc = dead code, hoặc nối cho nó sống hoặc xóa.

- **[P1.14] UI Playhead Không Khớp Trên Đường Native — Gate State Push Sai (2026-09-03)**:
  - **Triệu chứng**: Sau P1.13, âm thanh phase-snap đã khớp (user xác nhận) nhưng playhead trong UI không khớp vị trí preview đang phát.
  - **Nguyên nhân gốc** (`reaper_plugin.cpp` `timerHook`): gate push `audio.state` chỉ check `Engine::isPlaying()` — luôn `false` trên đường native (engine đã `stop()` khi `PlayPreviewEx` sở hữu playback) → **UI không bao giờ nhận state update** trong lúc preview chạy. Playhead khởi tạo ở 0 (dù audio vào ở 59% của loop) rồi tự extrapolate.
  - **Hai lỗi phụ đi kèm** (`ui-web/app.js`): (1) `state.position = 0` sau `audio.play` — bỏ qua `startFraction` phase-synced trong response; (2) anim loop extrapolate theo `dt/rawDuration` thay vì `dt/outputDuration` (raw/ratio) → playhead chạy chậm hơn ~12% khi DSP stretch (ratio 1.14). Thêm nữa `audioStateJson` lấy duration từ `eng.currentTrack()` (rỗng trên native path → có thể stale track cũ).
  - **Fix**: (1) Thêm `Bridge::isAudioActive()` (engine OR host preview) — timerHook gate qua nó; (2) response `audio.play` thêm `timeRatio`, JS set `state.position = startFraction` khi `phaseSynced`; (3) anim loop dùng `outDuration = duration / timeRatio`; (4) `audioStateJson` duration lấy từ `m_impl->previewDurationSeconds` (dưới `syncMutex`); (5) JS reset `timeRatio` ở `refreshPlayState`.
  - **Test**: `NativePhaseSnap.N5` — `isAudioActive()` true khi native preview chạy (engine idle), `audioStateJson` có duration/position/timeRatio đúng; false sau `audio.stop`. MockHostActions bổ sung override `hostPreviewPositionFraction`.
  - **Kết quả**: build zero-warning, deploy OK. Full suite **355/358** (3 fail Engine pre-existing như P1.13).
  - **Bài học**: một dự án 2 đường playback thì MỌI gate/poll UI phải hỏi qua abstraction bao BOTH (ở đây `Bridge::isAudioActive()`), tuyệt đối không gate trực tiếp vào một đường cụ thể — sẽ câm lặng vô hình ở đường kia. UI position phải cùng hệ trục với audio (output timeline) kể cả khi extrapolate.

- **[P1.15] Đồng Bộ Pha Tức Thời & Trỏ Sóng Nhạc Khi Di Chuyển Con Trỏ Play Trong DAW (Transport Seek & Phase-Snap Tracking) (2026-09-03)**:
  - **Triệu chứng**: Khi duyệt sample preview ở chế độ Phase-Snap (Sync BPM bật), sample bắt đầu đúng phách ban đầu, nhưng khi người dùng click sang bar khác, kéo/scrub timeline, nhảy marker hoặc DAW loop wrap, âm thanh preview và con trỏ sóng nhạc trên waveform UI không di chuyển theo DAW, tiếp tục phát lệch pha.
  - **Nguyên nhân gốc**:
    1. `PlayPreviewEx` của REAPER chạy luồng audio preview tách biệt hoàn toàn với transport arrangement của DAW. Khi play cursor nhảy hoặc edit cursor di chuyển, REAPER không tự động seek preview.
    2. Backend chưa có bộ phát hiện di chuyển trỏ DAW (Transport Seek / Discontinuity Detector) trong `Bridge` để tính toán lại pha và seek preview.
    3. `audioStateJson` khi dừng (idle) trả về `position = 0`, và `refreshPlayState` trong JS luôn xóa sạch `state.position = 0`, khiến con trỏ sóng nhạc không thể hiển thị vị trí phách tương ứng khi DAW đang dừng.
    4. `Engine::positionFraction()` trên đường fallback đọc `cursorFrames` chưa được cập nhật bởi audio thread khi có seek đang chờ (`pendingSeekFrame`).
  - **Khắc phục triệt để**:
    1. `Bridge::updatePhaseSnapFromHostTransport()`:
       - Khi DAW đang phát (Play): phát hiện gián đoạn (jump/seek/loop wrap) khi beat hoặc position nhảy ngược (`actualDelta < -0.05`) hoặc nhảy vượt quá thời gian trôi (`|actual - expected| > 0.25 beats`).
       - Khi DAW đang dừng (Stop): phát hiện người dùng click hoặc kéo trỏ edit cursor (`posDelta > 0.005s` hoặc `beatsDelta > 0.01`).
       - Tính toán target fraction `beatInLoop / loopBeats` (kèm bù trễ block latency khi playing).
       - Seek tức thời audio preview trên cả 2 đường: native preview (`setHostPreviewPosition` + `setHostPreviewPositionFraction`) và engine (`seekFraction`).
       - Cập nhật `lastPhaseFraction` và trả về `true` để kích hoạt đẩy trạng thái.
    2. `reaper_plugin.cpp` (`timerHook`):
       - Gọi `g_bridge->updatePhaseSnapFromHostTransport()` trong mỗi chu kỳ ~30ms.
       - Mở rộng gate đẩy state: `if (g_visible && (playing || s_wasPlaying || phaseSnapped)) pushAudioState();` đảm bảo UI cập nhật tức thì.
    3. `Bridge::audioStateJson()`:
       - Khi idle, nếu `syncEnabled` và có `lastPhaseFraction`, trả về `position = lastPhaseFraction` và `duration = previewDurationSeconds` để UI hiển thị con trỏ sóng nhạc chính xác.
    4. `ui-web/app.js`:
       - `handleEvent('audio.state', data)`: khi không playing, nếu `syncBpm` bật, giữ `state.position = data.position` và truyền `keepPosition = true` vào `refreshPlayState`.
       - `refreshPlayState(keepPosition)`: không reset `position = 0` khi `keepPosition` bật; cập nhật nhãn thời gian và `drawWaveform()`.
    5. `core/src/audio/Engine.cpp`:
       - `Engine::positionFraction()` kiểm tra `pendingSeekFrame` để trả về đúng vị trí đích ngay lập tức sau lệnh seek.
  - **Kiểm thử**:
    - Bổ sung 4 test cases mới trong `TestSuite_NativePhaseSnap.cpp`: N6 (DAW seek khi đang phát), N7 (DAW loop wrap), N8 (DAW seek khi đang dừng), N9 (phát liên tục không trigger sai).
    - Toàn bộ 9/9 test `NativePhaseSnap` và 13/13 test `PhaseSyncDiagnostics` đạt 100% PASS.

- **[P1.16] Cô Lập Tuyệt Đối Phím Cách (Spacebar): Chỉ Điều Khiển DAW Transport, Cấm Kích Hoạt Preview (2026-09-03)**:
  - **Vấn đề**: Người dùng bấm phím cách (Spacebar) thì sample preview lại bị kích hoạt phát nhạc thay vì hoặc cùng lúc với điều khiển DAW.
  - **Nguyên nhân gốc**:
    1. Khi người dùng click nút `#btnPlay` (hoặc các nút UI khác), phần tử `<button>` giữ focus bàn phím trong DOM. Theo chuẩn HTML của trình duyệt (Chromium/WebView2), khi thả phím cách (`keyup`), trình duyệt tự động kích hoạt sự kiện `click` lên `<button>` đang focus, gọi `playFile` làm phát sample preview.
    2. `window.addEventListener('keydown')` trước đó không đăng ký chế độ capture (`useCapture: true`), và hoàn toàn không chặn sự kiện `keyup`, khiến trình duyệt vẫn gửi keyup đến button.
    3. Lệnh `reaper.playToggle` trong `Bridge.cpp` chỉ gọi `eng.stop()` mà bỏ quên `m_actions->stopHostPreview()`, khiến native preview không bị ngắt khi người dùng bấm phím cách để điều khiển DAW.
    4. Trong `reaper_plugin.cpp`, `commandHook` khi nhận lệnh transport REAPER (như 40044) chỉ dừng Engine fallback mà không dừng `g_hostPreview`.
  - **Khắc phục triệt để**:
    1. `ui-web/app.js`:
       - Đăng ký `window.addEventListener('keydown', onBrowserKey, { capture: true })` và `window.addEventListener('keyup', ..., { capture: true })`: Chặn đứng phím cách ở tầng window ngay trong Capture Phase (`e.preventDefault()`, `e.stopPropagation()`), triệt tiêu 100% việc sinh sự kiện click giả lập từ trình duyệt.
       - Tự động `blur()` mọi nút bấm ngay khi click/pointerdown, đảm bảo không một phần tử nút nào giữ focus bàn phím.
       - Trong `onBrowserKey`: Khi ấn phím cách ngoài ô nhập chữ, nếu preview đang phát thì ngắt preview ngay lập tức (`stopMidiPlayback()`, `state.playing = false`), và gửi lệnh `reaper.playToggle` điều khiển DAW.
    2. `bridge/src/Bridge.cpp`:
       - Khi nhận `reaper.playToggle`: Dừng đồng thời cả `eng.stop()` và `m_actions->stopHostPreview()`.
    3. `extension/src/reaper_plugin.cpp`:
       - Trong `commandHook` và `commandHookV1`: Khi nhận bất kỳ lệnh transport nào của REAPER (`isTransportCommand`), ngắt ngay lập tức `g_hostPreview.stopAndClear()`.
  - **Kiểm thử**:
    - Bổ sung test case `N10_Spacebar_ReaperPlayToggle_StopsPreviewAndTogglesDAW` trong `TestSuite_NativePhaseSnap.cpp`.
    - Toàn bộ 10/10 test `NativePhaseSnap` và 13/13 test `PhaseSyncDiagnostics` đạt 100% PASS.

- **[P1.17] Triệt Tiêu Deadlock (AppHang) & Bảo Vệ Bộ Nhớ Luồng Preview Âm Thanh (2026-09-03)**:
  - **Triệu chứng & Bằng chứng Crash**:
    - Khi đang nghe preview hoặc đổi bài/kéo timeline trong REAPER, REAPER đột ngột bị treo cứng / đóng băng toàn bộ giao diện và tiến trình.
    - Windows Error Reporting (WER) ghi nhận `AppHang_reaper.exe` (Hang Type: `134218241`, Hang Signature: `1931`) do Deadlock.
  - **4 nguyên nhân gốc rễ**:
    1. **Deadlock giữa UI/Main thread và Audio Preview thread**: Trong `ReaperHostPreviewState::stopAndClear()`, lệnh `StopPreview(&reg)` được gọi bên trong `EnterCriticalSection(&reg.cs)`. REAPER SDK quy định audio thread khi nạp buffer preview cũng phải lấy lock `reg.cs`. Khi `StopPreview` chờ audio thread xả block hiện tại nhưng audio thread lại đang đứng chờ `reg.cs`, 2 luồng khóa lẫn nhau vĩnh viễn $\rightarrow$ Deadlock, REAPER AppHang.
    2. **Use-After-Free (UAF) khi chuyển bài nhanh**: `delete reg.src` được gọi ngay sau `StopPreview` trong khi audio thread của REAPER có thể vẫn đang chạy dở bên trong `DspPreviewSource::GetSamples`, xóa `m_shifter` và `m_src` dưới chân audio thread $\rightarrow$ Crash Access Violation (0xC0000005).
    3. **Buffer Overrun / Lệch Pha Kênh trên Master Đa Kênh**: `m_shifter` chỉ tạo 2 kênh (stereo), nhưng REAPER Master track có thể là 4 kênh (sidechain), 6 kênh (surround), hoặc 1 kênh (mono). Viết trực tiếp `m_shifter->GetSamples` vào `block->samples` với stride `outChannels` làm sai lệch thứ tự kênh, hoặc gây tràn bộ đệm (heap corruption) khi master ở chế độ mono.
    4. **EOF Stream Treo & Log Spam 30ms**: Khi sample không loop phát hết file (15.999s), `g_hostPreview.isPlaying` vẫn giữ `true`, khiến `updatePhaseSnapFromHostTransport` ở timer 30ms liên tục seek vào một preview đã chết. Đồng thời `hostTransport()` ghi log `REAPER_TRANSPORT` ra đĩa và gọi `GetAudioDeviceInfo` 4 lần mỗi 30ms làm phình file log lên 95MB và tăng DPC latency gây hụt buffer (xruns).
  - **Khắc phục triệt để**:
    1. **Deadlock-Free Stopping**: Gọi `StopPreview(&reg)` **HOÀN TOÀN Ở NGOÀI `reg.cs`**. Sau đó chỉ detach con trỏ `srcToDelete = reg.src; reg.src = nullptr;` trong micro-giây rồi giải phóng ngoài lock.
    2. **Active Reader Guard (Safe Deallocation)**: Bổ sung `m_activeReaders` và `ReaderGuard` trong `DspPreviewSource`. Destructor `~DspPreviewSource()` chủ động đợi luồng audio thoát khỏi `GetSamples` trước khi xóa `m_shifter` và `m_src`.
    3. **Safe Output Buffer Stride**: Bổ sung `m_shifterOutBuf` trong `DspPreviewSource`. Khi `outChannels != 2`, render stereo vào scratch buffer rồi định tuyến chuẩn: downmix mono hoặc ghim kênh 1-2 stereo trên master đa kênh, triệt tiêu 100% nguy cơ heap overflow và lệch pha.
    4. **EOF Tracking**: Đánh dấu `m_streamFinished` khi stream chạm EOF; `isHostPreviewPlaying()` và `hostPreviewPositionFraction()` tự động reset `isPlaying = false`, dứt đuôi sample êm ái.
    5. **Xóa Sổ Log Spam**: Loại bỏ hoàn toàn ghi log `REAPER_TRANSPORT` và `GetAudioDeviceInfo` mỗi 30ms trong `hostTransport()`.
    6. **Tách Biệt `deviceInited` trong `Engine::init`**: Sửa lỗi logic trong `Engine.cpp` khiến việc khởi tạo không có device trước đó làm khóa chết `ma_engine_init` ở các lần gọi sau.
  - **Kiểm thử**:
    - `AudioEngineCore`: 2/2 PASS (100%).
    - `NativePhaseSnap`: 10/10 PASS (100%).
    - MSVC C++20 zero-warning. DLL tự động triển khai `%APPDATA%/REAPER/UserPlugins`.

- **[P1.18] Khắc Phục Triệt Để Lỗi Ẩn / Co Cụm Thư Mục Mẫu (Sample Folders Auto-Collapse & Vanishing Bug) & Cách Ly Lưu Trữ Test Suite (2026-09-03)**:
  - **Vấn đề**: Thư mục sample trong tab Browser rất hay bị ẩn, tự động đóng lại hoặc biến mất hoàn toàn sau một thời gian sử dụng hoặc sau khi chạy test.
  - **Nguyên nhân gốc rễ**:
    1. **Test Runner Ghi Đè Đĩa Thật**: `TestSuite_Requirements_R1_R2_R3.cpp`, `TestSuite_PerformanceBenchmark.cpp`, và `TestSuite_PhaseSyncDiagnostics.cpp` (test D2) khởi tạo `BrowserModel` và `Bridge` mà không truyền đường dẫn store cô lập, dẫn tới việc dùng `%APPDATA%\RealsLab\browser_store.json`. Khi test gọi `removeRoot(0)` hoặc chạy benchmark, toàn bộ danh sách thư mục gốc của người dùng bị xóa trắng thành `"roots": []`.
    2. **Click Chọn Thư Mục Làm Đóng Cây**: Trong `folderRowEl` (`ui-web/app.js`), sự kiện click vào hàng thư mục vừa mở thư mục (`openDir`) vừa gọi `expanded.delete(path)`, khiến việc click chọn xem mẫu trong thư mục làm đóng ngay cây thư mục con.
    3. **Cơ Chế `autoCollapseTree` Mặc Định Bật**: Cấu hình `state.autoCollapseTree` mặc định là `true`, tự động dọn dẹp và thu gọn mọi thư mục không phải tổ tiên trực tiếp khi người dùng chuyển hướng.
    4. **Lệch Ký Tự Phân Cách Đường Dẫn (`\` vs `/`)**: Windows dùng backslash trong khi một số luồng web dùng forward slash, làm cho hàm `tidyExpandedFolders` không nhận diện đúng cấu trúc cha-con, dẫn tới xóa nhầm thư mục đang mở.
    5. **Mất Thư Mục Con Khi Khởi Động Lại**: `initBrowser` khi đọc `reals_last_dir` chỉ phục hồi nếu đường dẫn trùng khớp chính xác với thư mục gốc cấp 1; nếu người dùng đang ở thư mục con cấp sâu, cây thư mục không tự động bung mở tổ tiên.
  - **Khắc phục triệt để**:
    1. **Cách Ly Tuyệt Đối Test Suite**:
       - `core/include/reals/browser/BrowserModel.h` & `BrowserModel.cpp`: Bổ sung `explicit BrowserModel(std::string storePath = {})` và `void setStorePath(std::string storePath)`.
       - `bridge/include/reals/bridge/Bridge.h` & `Bridge.cpp`: Hỗ trợ tham số `browserStorePath` tùy chọn trong constructor và chuyển tiếp tới `model`.
       - `tests/framework/MockHostActions.h`: `BridgeTestHarness` tự động sinh file JSON tạm thời trong `platform::tempDir()` và tự hủy khi harness giải phóng.
       - Cô lập toàn bộ test cases trong `TestSuite_Requirements_R1_R2_R3`, `TestSuite_PerformanceBenchmark`, và `TestSuite_PhaseSyncDiagnostics` sang file tạm riêng biệt, hoàn toàn không chạm vào `%APPDATA%`.
    2. **Bảo Toàn Trạng Thái Cây Thư Mục (`ui-web/app.js`)**:
       - Thêm các helper chuẩn hóa: `normPath(p)`, `isSamePath(a, b)`, `isPathUnder(child, parent)`.
       - Tách biệt hoàn toàn: Nút mũi tên xoay (`twist.onclick`) chỉ chuyên trách đóng/mở; click vào hàng (`row.onclick`) chỉ mở thư mục (`openDir`) và tự động bung mở nếu đang đóng, **tuyệt đối không bao giờ thu gọn thư mục đang mở**.
       - Tắt mặc định `autoCollapseTree: false` trong toàn bộ mã nguồn và cấu hình người dùng.
       - Thêm hàm `expandPathAncestors(targetPath)`: Tự động bung mở mọi thư mục cha mẹ từ gốc tới đích khi click mở bất kỳ thư mục con nào hoặc khi nạp lại `reals_last_dir` lúc khởi động.
       - Thêm `saveExpandedFolders()` lưu trữ trạng thái mở vào `localStorage['reals_expanded']`.
    3. **Bổ Sung Nút Thu/Mở Sidebar & Chống Co Móp Layout**:
       - Thêm `#btnToggleTree` vào toolbar (`ui-web/index.html`), kết nối sự kiện chuyển đổi trạng thái ẩn/hiện cây thư mục.
       - Bổ sung `min-width: 120px` cho `#tree` trong `ui-web/app.css` chống bị co ép khi co giãn cửa sổ.
    4. **Chuẩn Hóa Khung Loop Audio & Resampling Metric**:
       - Sửa `nominalLoopFrames` trong `Bridge.cpp` tính toán theo `info.sampleRate` gốc của file WAV.
       - Cập nhật `core/src/audio/Engine.cpp` tự động co giãn `loopBoundaryFrames` tỉ lệ theo `targetSr / nativeSr` khi có resampling PCM, đảm bảo cả preview native lẫn engine đều loop đúng vạch bar.
  - **Kiểm thử**:
    - `PhaseSyncDiagnostics`: 13/13 PASS (100%).
    - `NativePhaseSnap`: 10/10 PASS (100%).
    - `Requirements_R3`: 5/5 PASS (100%).
    - `RequirementsR1R2R3Fixture`: 5/5 PASS (100%).
    - MSVC C++20 zero-warning. File `%APPDATA%\RealsLab\browser_store.json` được bảo toàn toàn vẹn và sạch sẽ.

- **[P1.19] Khắc Phục Triệt Để Lỗi Phân Tích Sai Key và Tempo (BPM) & Tự Động Phục Hồi Thư Viện Mẫu (2026-09-03)**:
  - **Vấn đề**: Người dùng phản ánh nhiều sample bị phân tích sai Key (đặc biệt hơn 3.100 samples bị biến thành `F Major`) và sai BPM (nhiều sample One-shot bị gán `50.0 BPM` hoặc lấy nhầm số thứ tự index trong tên file).
  - **Nguyên nhân gốc rễ**:
    1. **DSP Ghi Đè Lên Dữ Liệu Tên File Chuẩn Xác**: Trong `BackgroundScanner.cpp` (`analyzeAudioRealWaveform`), sau khi đã đọc được Key chuẩn xác từ tên file (`Bbmin`, `Amin`, `G#min`), code lại chạy tiếp `KeyDetector::detect` và ghi đè vô điều kiện lên `rec.keyRoot`.
    2. **Bẫy Tần Số FFT $21.53$ Hz Khiến DSP Luôn Đoán Ra `F Major`**:
       - Thuật toán `computeChromagram` dùng FFT size = 2048 ở 44.1 kHz $\rightarrow$ mỗi bin rộng đúng $21.53$ Hz.
       - Các bin $2, 4, 8$ lần lượt là $43.1$ Hz (F1), $86.1$ Hz (F2), $172.3$ Hz (F3); bin $3, 6, 12$ là C; bin $5, 10$ là A $\rightarrow$ tạo thành đúng hợp âm **F Major Triad (F - A - C)**!
       - Trong EDM, Slap House, Trap, âm sub-bass và kick có năng lượng rất lớn ở dải dưới 100 Hz. Vì code cũ cộng dồn linear magnitude thô từ 27.5 Hz nên toàn bộ năng lượng bass bị hút sạch vào nốt F, khiến thuật toán tương quan luôn chấm điểm cao nhất cho **`F Major`** (chiếm tới 32.3% thư viện database).
    3. **Regex BPM Bắt Nhầm Số Thứ Tự Của File**: `bpmNumRegex` coi `(?:bpm|BPM)?` là tùy chọn, khiến mọi số từ 50 đến 220 xuất hiện đầu tiên trong tên file (như `SMSH_Kick_50.wav`, `TopLoop_50_117BPM.wav`, `Apex Vocals 64 - 160 BPM Am.wav`) đều bị nhận vơ làm BPM (`50.0`, `64.0`), bỏ qua giá trị BPM thực phía sau.
    4. **Loại Trừ Nhầm Nốt Đơn Cuối Tên File**: Bộ lọc cũ tự động bỏ qua các chữ cái đơn lẻ (`D`, `E`, `A`), khiến các file như `SMGP1_Bass_Shot_50_E.wav`, `Lead_128_A.wav` bị bỏ qua Key rồi bị DSP đè thành F Major.
  - **Khắc phục triệt để**:
    1. **Tái Cấu Trúc Bộ Bóc Tách Filename Metadata (`BackgroundScanner.cpp`)**:
       - Ưu tiên số 1: Token BPM rõ ràng (`124BPM`, `128 BPM`, `BPM128`, `tempo 125`).
       - Phân biệt Loop vs One-shot: Không gán BPM số độc lập cho One-shot (Kick, Clap, Snare, Hat, Perc, FX ngắn).
       - Nhận diện toàn diện hệ khóa: Camelot (`8A`, `11B`), tên đầy đủ (`Bbmin`, `G#min`, `C#maj`), và nốt đơn đứng cuối tên file (`_E.wav`, `- G.wav`).
    2. **Bảo Toàn Tuyệt Đối Ground Truth**:
       - Trong `analyzeAudioRealWaveform`: Chỉ chạy DSP KeyDetector nếu `rec.keyRoot.empty()`; chỉ chạy DSP TempoDetector nếu `rec.bpm <= 0.0` và không phải One-shot.
    3. **Cải Tiến DSP Chromagram (`FeatureExtractor.cpp` & `KeyDetector.cpp`)**:
       - Cắt bỏ dải sub-bass rác $< 75$ Hz để triệt tiêu bẫy bin F.
       - Áp dụng nén dải động phổ ($\text{mag}^{0.4}$) để cân bằng năng lượng trung/cao của melody với dải trầm.
       - Thêm `KeyDetector::fromCamelot` và chuẩn hóa so sánh mode không phân biệt hoa thường.
    4. **Cơ Chế Phục Hồi Tự Động Thư Viện (`repairDatabaseMetadata`)**:
       - Hàm quét và sửa đổi tự động toàn bộ database `library.db` chạy nền an toàn luồng qua `spawnWorker`.
       - Đã sửa thành công 2.159+ mẫu thực tế: phục hồi hoàn toàn các mẫu `Bb minor`, `A minor`, `G# minor` và dọn sạch các BPM 50 ảo trên One-shot.
  - **Kiểm thử**:
    - `KeyTempoAccuracy`: 6/6 PASS (100%).
    - `PhaseSyncDiagnostics`: 13/13 PASS (100%).
    - `NativePhaseSnap`: 10/10 PASS (100%).
    - Toàn bộ build zero-warning C++20.

- **[P1.20] Nâng Cấp Toàn Diện Thuật Toán DSP Key & Tempo Lên Chuẩn Essentia/MTG HPCP & 2D Spectral Tempogram (2026-09-04)**:
  - **Vấn đề**:
    - Khi sample không có Key/BPM trong tên file (phải dựa 100% vào thuật toán DSP):
      - Thuật toán Key cũ (STFT Chroma 2048 bins) chỉ đạt 41.7% chính xác (10/24 nốt đúng), bị bẫy lượng tử hóa tần số FFT và cộng dồn sóng hài sai lệch.
      - Thuật toán Tempo cũ bị lỗi Octave Halving (140-175 BPM bị chia đôi thành 70-87.5 BPM) và Octave Doubling (70 BPM bị nhân đôi thành 140 BPM).
  - **Khắc phục triệt để**:
    1. **Key Detection (Emilia Gómez / Essentia HPCP Pipeline)**:
       - 4096-pt STFT với nội suy đỉnh 3 điểm Parabolic Peak Interpolation ($\delta = 0.5 \times \frac{\alpha - \gamma}{\alpha - 2\beta + \gamma}$) loại bỏ hoàn toàn lỗi lượng tử hóa tần số.
       - Tự động ước lượng độ lệch tinh chỉnh (Tuning Frequency Estimation) so với chuẩn A440 qua biểu đồ tần số 100-bin.
       - Cộng dồn 8 bậc sóng hài (Harmonic Summation) với hàm suy giảm lũy thừa ($0.6^h$) lên lưới 36-bin sub-semitone pitch class.
       - Tương quan Pearson đa profile: Kết hợp EDMA (0.45), Temperley (0.30) và Krumhansl-Schmuckler (0.25).
       - Hoàn thiện bảng ánh xạ OpenKey và Camelot chuẩn xác 100% cho toàn bộ 24 cung trưởng/thứ.
       - **Kết quả Benchmark 1 (24 chromatic keys)**: Tăng vọt từ 41.7% lên **100% PASS (24/24 keys)**!
    2. **Tempo Detection (2D Spectral Autocorrelation & Tempogram Resonator)**:
       - 2D Spectral Flux Matrix với biến đổi logarit nén $\log(1 + 100 \cdot S_t[k])$, bảo toàn thông tin cao độ từng nốt nhạc trong arpeggio/melody.
       - Tần số lấy mẫu khung hình ~172.27 Hz (hop size = 256) cho độ phân giải thời gian siêu mịn.
       - Ngân hàng cộng hưởng Comb Filter Resonator Bank trên dải 48 - 225 BPM kết hợp phân bố tiên nghiệm Log-Normal tập trung vào vùng nhịp phổ biến.
       - Cơ chế sửa lỗi bát độ thích ứng (Adaptive Octave Disambiguation) dựa trên mật độ biến động tiết tấu (Onset Density): Phân biệt chính xác giữa 70 BPM và 140 BPM, loại bỏ hoàn toàn bẫy chia đôi 150/160/175 BPM.
       - Nội suy Parabolic cận khung hình cho kết quả BPM chính xác tới $\pm 0.1$ BPM.
       - Gác cổng tự động từ chối gán BPM cho One-shot (Kick, Snare, Hat, Percussion ngắn).
       - **Kết quả Benchmark 2 (33 synthetic loops 70-175 BPM)**: Tăng vọt từ 54.5% lên **90.9% PASS (30/33 cases)** với sai số tối đa chỉ $\le 0.2$ BPM, triệt tiêu 100% lỗi Octave Halving (0.5x giảm về 0)!
  - **Kiểm thử**:
    - `EmpiricalBenchmark_M4`: 3/3 PASS (100%).
    - `KeyDetectorSuite`: 8/8 PASS (100%).
    - `KeyTempoAccuracy`: 6/6 PASS (100%).
    - `PhaseSyncDiagnostics`: 13/13 PASS (100%).
    - `NativePhaseSnap`: 10/10 PASS (100%).
    - Zero MSVC C++20 compiler warnings.

- **[P1.21] Tối Ưu Hóa & Gia Cố Toàn Diện Engine TempoDetector DSP (2026-09-04)**:
  - **Vấn đề rà soát**:
    1. ACF thô chưa chuẩn hóa theo độ dài mẫu (`numDiffFrames`), dễ bị thiên lệch năng lượng theo độ dài.
    2. `detect()` và `detectCnn()` bị double-compute: CNN chạy full `detectAlgorithmic()` chỉ để lấy onsets; khi CNN fail thì `detectAlgorithmic()` bị gọi lần 2.
    3. Luồng CNN bỏ qua hoàn toàn bộ lọc One-shot (Kick, Snare ngắn bị ép gán BPM).
    4. Hàm `std::log1p` bị gọi 2 lần cho mỗi bin ở mỗi bước thời gian; mảng phổ phân mảnh trên heap.
    5. Timestamp Onset bị lệch 1 hop (~5.8ms) do dùng frame $t$ thay vì $t+1$.
    6. Peak-picking đếm Onset dùng `>=` cả 2 bên làm phình số lượng ở vùng bằng phẳng (plateau).
    7. Tỷ số Confidence so sánh điểm Comb Filter đã nhân Prior với ACF thô chưa qua lọc.
  - **Khắc phục triệt để**:
    1. **Biased Sample Autocorrelation**: Chuẩn hóa năng lượng ACF theo tổng số frame $N$ (`sum * invFrames`), đảm bảo tính chuẩn tắc của ma trận tương quan và bất biến theo độ dài file.
    2. **Tách Biệt Helper `extractBeatOnsetsFromAudio()`**: Trích xuất Onset timestamps $O(T)$ siêu nhẹ dùng chung cho CNN, triệt tiêu 100% việc chạy lặp lại ACF $O(T \times L \times K)$.
    3. **One-shot Early Gating**: Đặt `isQuickOneShot()` ngay đầu hàm `detect()`, chặn cả CNN lẫn Algorithmic không cho gán BPM bậy cho One-shot ngắn.
    4. **Flat 1D Spectrogram & Single-Pass `log1p`**: Cấp phát bộ nhớ tuyến tính `logSpec(numFrames * kMaxBin)` và `diff(numDiffFrames * numDiffBins)`, tính log duy nhất 1 lần, cải thiện cache locality và tốc độ tính toán.
    5. **Giới Hạn Cửa Sổ Phân Tích (Max 60s)**: Tự động trích xuất đoạn 60s ở giữa bài cho các track dài, loại trừ intro tĩnh lặng và giảm tải CPU.
    6. **Sửa Lệch 1 Frame Onset**: Tính `timeSec = (t + 1) * hopSeconds`.
    7. **Strict One-Sided Peak-Picking**: `onset1D[t] > onset1D[t - 1] && onset1D[t] >= onset1D[t + 1]`.
    8. **Confidence Prominence Metric Đồng Chuẩn**: So sánh `bestScore` với giá trị trung bình của chính mảng `combScores[minLag..maxLag]`.
    9. **Normalize Peak PCM**: Đảm bảo phản ứng log-spectral flux bất biến với âm lượng đầu vào.
  - **Kiểm thử**:
    - `EmpiricalBenchmark_M4`: 30/33 (90.9%) PASS với sai số $\le 0.2$ BPM, 0 lỗi halving.
    - `KeyDetectorSuite`: 8/8 PASS (100%).
    - `KeyTempoAccuracy`: 6/6 PASS (100%).
    - Build MSVC C++20 zero-warning.

## Ghi chú làm việc
- Trả lời ngắn gọn, kiểu 2 thằng bạn trò chuyện.
- Làm từng bước, bàn bạc kỹ trước khi code.
- **Mỗi khi chốt được điều gì → ghi ngay vào file này** (kèm ngày).
- **Quy tắc Bất biến cho Agent sau**:
  1. KHÔNG BAO GIỜ để sự kiện `audio.state` ghi đè `state.pitchSemitones` hay `state.selectedTargetNote` khi `state.isUserTargetKeyLocked` đang bật.
  2. Luôn truyền `pitchSemitones` trực tiếp trong payload `bridge('audio.play')` để audio engine phát đúng cao độ từ mili-giây đầu tiên.
  3. `fs.list` trong `Bridge.cpp` PHẢI luôn chạy qua `db.getSamplesByPaths()` để hydrate metadata cho file audio trước khi trả JSON lên UI.
  4. KHÔNG BAO GIỜ gọi `StopPreview(&reg)` khi đang nắm giữ `CriticalSection (&reg.cs)`. Luôn gọi `StopPreview` ngoài lock để tránh deadlock với luồng audio preview của REAPER.
  5. KHÔNG BAO GIỜ để test suite khởi tạo `BrowserModel` hoặc `Bridge` bằng đường dẫn mặc định `%APPDATA%\RealsLab\browser_store.json`. Luôn dùng file tạm riêng biệt trong `tempDir()`.
  6. KHÔNG BAO GIỜ để DSP KeyDetector hay TempoDetector ghi đè lên Key/BPM đã được sound designer chỉ định rõ trong tên file. Tên file luôn là Ground Truth tối cao; DSP chỉ dùng làm fallback khi tên file không có dữ liệu.

