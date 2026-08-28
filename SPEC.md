# Reals Lab — SPEC (Từ 0 đến Full)

> Phiên bản: 1.2 · Ngày: 2026-08-25 · Tham khảo: `PLAN.md` (ý tưởng), `DESIGN.md` (design system)
>
> **v1.2 — Hardening pass**: audit toàn dự án, sửa ~25 bug. `net::HttpClient` có implementation thật (WinHTTP) — LabApi chạy qua nó; lab job workers được track + join; base URL Audio Lab cấu hình được (`labApiBaseUrl`); bridge thêm `fs.addRoot/removeRoot`, `fs.list` nhận sort, `browser.tags` trả full map, `audio.setLoop` hoạt động, alias `lab.tempo/lab.midi`. Version app: **0.2.0**.
>
> **v1.1 — UI PIVOT**: ImGui → **WebView2 (HTML/CSS/JS)**. Lý do + kiến trúc mới xem `PLAN.md` mục Tech stack. Core giữ nguyên; ui/ (ImGui) deprecated, giữ tham chiếu đến khi WebView đạt parity.

## 1. Mục tiêu

Xây dựng **Reals Lab**: hệ sinh thái kết nối toàn bộ dịch vụ reals.media vào DAW và desktop:

| Sản phẩm | Form | Nền tảng |
|---|---|---|
| REAPER Extension | `.dll` / `.dylib` / `.so` | Win → Mac → Linux |
| Desktop App | `.exe` / `.app` / AppImage | Win → Mac → Linux |

**1 codebase — 2 sản phẩm.** UI tiếng Việt + English. Backend: reals.media (Next.js + Prisma + PostgreSQL).

## 2. Tech Stack (chốt)

| Lớp | Công nghệ | Ghi chú |
|---|---|---|
| Ngôn ngữ | C++20 | Chuẩn thống nhất |
| Build | CMake ≥ 3.24 + presets | 1 hệ build cho cả 3 OS |
| **UI** | **WebView2 (HTML/CSS/JS)** | UI từ `mockup.html` đã duyệt; macOS: WKWebView, Linux: WebKitGTK (P6) |
| Bridge | WebMessage JSON (postMessage) | JS ↔ C++: audio, browser, REAPER actions |
| Audio | miniaudio | playback/capture/decode wav-mp3-flac |
| Ghi file | Tự viết WAV writer | miniaudio không encode; tránh phụ thuộc libsndfile |
| Network | `net::HttpClient` (interface) · WinHTTP trên Win | Mọi request đi qua đây; Mac/Linux transport ở P6. libcurl chỉ khi cần resume/proxy phức tạp |
| JSON | nlohmann/json | Header-only |
| Font | Inter / Segoe UI (web) | Hỗ trợ UTF-8 tiếng Việt |

**KHÔNG dùng**: Lua (bảo mật), JUCE (license + rủi ro tích hợp REAPER), Dear ImGui (deprecated sau P1).

## 3. Kiến trúc

```
reals-lab/
├── core/                  ← THƯ VIỆN CHUNG (không phụ thuộc UI/host)
│   ├── include/reals/
│   │   ├── platform/      Path.h, Thread.h, Dll.h — abstraction per-OS
│   │   ├── audio/         Engine.h (miniaudio), Buffer.h, LevelMeter.h
│   │   ├── net/           HttpClient.h, ApiClient.h, ApiModels.h
│   │   ├── i18n/          I18n.h + assets strings_vi.json / strings_en.json
│   │   ├── config/        Config.h (JSON ở %APPDATA%/~/Library/.config)
│   │   ├── browser/       BrowserModel.h — dữ liệu file browser
│   │   └── util/          Log.h, Uuid.h, Time.h
│   └── src/               (.cpp tương ứng)
├── ui-web/                ← UI = HTML/CSS/JS (từ mockup.html đã duyệt)
│   ├── index.html  styles.css  app.js
│   └── (1 codebase UI cho cả 3 OS)
├── bridge/                ← JS ↔ C++ dispatcher (WebMessage JSON)
├── shell/win/             ← Win32 window + WebView2 controller (extension + app dùng chung)
├── extension/             ← SHELL REAPER (reaper_plugin + shell/win)
├── app/                   ← SHELL DESKTOP (.exe)
├── ui/                    ← (DEPRECATED — ImGui, giữ tham chiếu đến parity)
├── libs/                  ← reaper-sdk
├── assets/i18n/           ← strings vi/en (override cho embedded)
├── CMakeLists.txt         + CMakePresets.json
├── PLAN.md  DESIGN.md  SPEC.md  AGENTS.md
```

**Nguyên tắc phụ thuộc (bắt buộc):**
```
app, extension  →  shell/win (WebView2)  →  bridge  →  core
core KHÔNG được biết gì về WebView/REAPER/GLFW. UI (HTML) KHÔNG gọi API hệ thống trực tiếp — mọi thứ qua bridge.
```

### Bridge commands (v0.2)
| Nhóm | Lệnh | Ghi chú |
|---|---|---|
| app | `app.info` | version 0.2.0, platform |
| config | `config.getAll`, `config.set` | key/value JSON |
| fs | `fs.roots`, `fs.addRoot`, `fs.removeRoot`, `fs.dropPaths`, `fs.subdirs`, `fs.list` (sort: 0 tên/1 size/2 ngày), `fs.invalidate`, `fs.watch` | list có cache; `fs.list` trả cả folder (`isDir`); `fs.watch` → event `fs.changed`; drop Explorer → `fs.dropPaths` + event `fs.rootsChanged` |
| browser | `browser.search` (async: `{pending,gen}` + event `browser.searchResult`), `browser.favorites/recents/addRecent/toggleFavorite/clearRecents`, `browser.tag`, `browser.tags` (có path → per-path; không path → full map), `browser.beginDrag` (Cơ chế A: kéo file gốc 0ms lag, queue sync playrate/pitch cho REAPER native take stretch & grid alignment) | search recursive trên worker, hủy được |
| browser | `browser.rename`, `browser.delete` | trả `ok:false` + error khi fail (UI phải catch); UTF-8 path |
| audio | `audio.play/stop/setLoop/setVolume/probe/seek/setPitchShift/setSyncBpm/setOriginalKey/detectBpm` | `audio.play` tự động tính `startFraction` đồng bộ pha DAW playhead (FL Studio Cloud style); probe header-only; seek 0..1; pitchShift ±12st; sync via timeRatio |
| lab | `lab.analyze/keychord/stem/denoise` (+alias `lab.tempo`→analyze, `lab.midi`→keychord) | chạy background thread, đẩy event `lab.progress/result/error`; poll 2s, timeout ~10 phút |
| reaper | `reaper.insert`, `reaper.insertMany`, `reaper.reveal`, `reaper.lab`, `reaper.tempo` | insert mode 1 = new track, bọc Undo; tempo = `Master_GetTempo` |
| window | `window.hide`, `window.minimize` | |

Events push từ C++: `toast`, `audio.state` (30Hz khi playing + 1 frame cuối khi dừng), `lab.progress`, `lab.result`, `lab.error`, `browser.searchResult` `{gen, results}`, `fs.changed` `{path}`, `fs.rootsChanged` `{added:[{name,path}]}`, `fs.dropHover` `{on:bool}`.

## 4. API Contract (reals.media)

Base URL cấu hình được. Mọi response: `{ "ok": true, "data": ... }` hoặc `{ "ok": false, "error": { "code", "message" } }`.

### 4.1 Auth (device flow — chờ web chính làm)
```
POST /api/lab/auth/request     → { deviceCode, userCode, verifyUrl }  (app mở browser)
GET  /api/lab/auth/poll?code=  → { status: "pending" | "ok", token, user }
```
Token lưu trong Config (mã hóa theo OS — DPAPI/Keychain/libsecret).

### 4.2 Marketplace
```
GET  /api/lab/products?query&category&sort&page      → danh sách (map Product, Category)
GET  /api/lab/products/:id                           → chi tiết + ProductFile versions
GET  /api/lab/purchases                              → sản phẩm đã mua (map Purchase)
POST /api/lab/purchases/:id/download-url             → signed URL tải file
GET  /api/lab/notifications                          → Notification list
```

### 4.3 Audio Analysis (API có sẵn)
```
POST /api/lab/audio/analyze   (multipart: file, jobType=stem|key|chord|tempo|denoise)
                              → { jobId }
GET  /api/lab/audio/jobs/:id  → { status: queued|running|done|error, progress, resultUrl }
GET  resultUrl                → ZIP / MIDI / JSON
```
Client **poll mỗi 2s**, timeout 10 phút, hỗ trợ offline queue.

### 4.4 Agent (OpenAI-compatible)
```
POST /api/lab/agent/chat   { messages[], tools[] }  → SSE stream tool_calls
```
Client chỉ là executor: nhận `{tool, args}` → thực thi (REAPER actions / API calls) → trả kết quả vào vòng chat. Phân quyền: server gửi kèm `allowedTools[]`.

## 5. Module Spec

### 5.1 Browser local (MVP ĐẦU TIÊN)
- Tree thư mục (lazy load), list file có cột: tên / kích thước / ngày sửa
- **Preview audio** (miniaudio): click nghe, loop toggle, space = play/stop
- Waveform mini render từ buffer
- Favorites (★) + Recent + Watch folder (file mới tự hiện)
- Tag màu cho file (lưu Config JSON theo path hash)
- Drag & drop file vào timeline REAPER (Cơ chế A: OLE CF_HDROP kéo file gốc 0ms lag; hook `processPendingSyncPlayrates` tự động gán `D_PLAYRATE`, `B_PPITCH = 1`, `D_PITCH` và co giãn `D_LENGTH` khớp chuẩn ô nhịp Grid Bar)
- Quick actions: rename, delete, open folder
- Gửi thẳng Audio Lab từ context menu

### 5.2 Audio Lab
- Chọn item/track → render vùng chọn (extension) hoặc chọn file (app)
- Jobs: stem, denoise, key+chord, tempo (gọi 4.3)
- Kết quả: stem → tạo track mới; MIDI → item mới; ZIP → lưu + mở folder
- **Local**: preview A/B trước/sau, loudness meter (peak/RMS/LUFS ngắn), silence trim (local, không cần API)
- Batch queue nhiều file

### 5.3 Marketplace
- Search + chips lọc + sort (map 4.2)
- Card: thumb, tên, tag FREE/PRO, giá, nút Tải về/Mua
- Đã cài: check version → banner update
- Install tự động theo loại: JSFX→Effects, Script→Scripts, Theme→ColorThemes, VST→đường dẫn user cấu hình
- Download manager: queue, resume (curl resume), retry

### 5.4 Agent
- Chat UI + 3 chế độ phân quyền (client) + allowedTools (server)
- Tool registry: JSON định nghĩa ở server, client executor generic
- Tools REAPER: ~50 tool ngữ nghĩa cao + bridge `Main_OnCommand` + get/set props
- Mọi action bọc Undo block; action nguy hiểm → confirm theo chế độ
- Memory phiên + history lưu Config

### 5.5 Account & Hệ thống
- Device flow login, hiển thị gói + license + usage
- Settings: ngôn ngữ, paths, nav position (4 hướng), accent (4 biến thể), noise overlay, proxy, cache
- Self-update: GET /api/lab/version → banner → tải + thay file (app); extension: mở trang tải
- Onboarding lần đầu: chọn thư mục → login → tour

## 6. Lộ trình (Phases)

| Phase | Nội dung | Done khi |
|---|---|---|
| **P0 — Nền móng** ✅ | Repo + CMake build Win + core skeleton (Log/Config/I18n/Platform) + **cả 2 shell: app (GLFW+GL) & extension (REAPER+DX11)** + AGENTS.md | ✅ `reals_app.exe` + `reaper_realslab.dll` build pass zero-warning |
| **P1 — Browser MVP** ✅ (ImGui, deprecated) | Browser đầy đủ bằng ImGui — **bị thay thế bởi P1.5** | ✅ Đã chạy trong extension |
| **P1.5 — WebView2 pivot** ✅ | shell/win (Win32 + WebView2 controller + virtual host mapping) + bridge JSON đầy đủ (fs/audio/browser/config/reaper/window) + `ui-web/` full UI 5 tab (vi/en, mockup design) | ✅ Build pass; extension mở window HTML, JS ↔ C++ bridge hoạt động |
| **P1.6 — Browser trên WebView** | Chuyển toàn bộ tính năng browser (tree/preview/favorites/tags/search/insert) qua bridge | Browser HTML dùng được như bản ImGui |
| **P2 — Audio Lab** | 5.2 đầy đủ (API + local tools) | Tách stem từ REAPER ra track mới |
| **P3 — Marketplace** | 5.3 | Mua/tải/cài trong app |
| **P4 — Account** | Device flow + settings đầy đủ | Login sync với web |
| **P5 — Agent** | 5.4 | Chat điều khiển REAPER được |
| **P6 — Full** | Update system, onboarding, **Mac (WKWebView) + Linux (WebKitGTK)**, polish | 3 OS release |

Mỗi phase có checklist riêng, xong phase mới sang phase sau. Không nhảy cóc.

## 7. Rủi ro & Phòng ngừa

| Rủi ro | Khả năng | Phòng ngừa |
|---|---|---|
| ImGui host vào REAPER window khác nhau mỗi OS | Cao | P0 làm Win trước + abstraction `IHostWindow`; Mac/Linux ở P6 |
| Serverless Vercel giới hạn upload size | Trung | Xác nhận với web team: audio API chạy Node/Docker, không phải serverless |
| miniaudio thiếu format encode | Thấp | Chỉ cần ghi WAV (tự viết); encode mp3/flac không có trong MVP |
| Agent action phá project user | Trung | Undo block + confirm mode + allowedTools server |
| API auth chưa sẵn khi đến P4 | Cao | Mock ApiClient với JSON local — UI không bị chặn |
| 1 người + AI bỏ dở giữa chừng | Trung | Mỗi phase độc lập, ship được riêng; PLAN.md/AGENTS.md giữ context |

## 8. Quy ước kỹ thuật

- C++20, warning `-Wall -Wextra`, zero-warning policy
- Naming: `PascalCase` class/function, `camelCase` biến, `m_` prefix member, `k` prefix constant, `s_` static
- Mọi string UI qua `tr("key")` — cấm hardcode text
- Mọi path qua `platform::` — cấm `\` cứng, cấm `%APPDATA%` trực tiếp
- Mọi network qua `net::HttpClient` — cấm gọi curl trực tiếp nơi khác
- Format: clang-format (`.clang-format` ở root), chạy trước mọi commit
- Commit: `feat(browser): ...` / `fix(core): ...` — conventional commits
