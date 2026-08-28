# Reals Lab — Design System (ĐÃ DUYỆT 2026-08-24)

## 1. Visual Direction
- Dark premium / desktop SaaS / audio-production software.
- Nền đen than (không đen tuyệt đối), card xám đen rất nhẹ, border mảnh low-contrast.
- Cam Reals chỉ cho action/active. Typography trắng/xám, hierarchy rõ.
- Ít gradient, không shadow nặng, radius 10–14px.
- Cảm giác **native desktop app**, không giống website marketing.
- Density hơi cao nhưng vẫn thoáng.
- Icon/plugin được đa màu, UI chrome chỉ monochrome + orange.

## 2. Color Tokens
```css
/* Background */
--bg-root:#090A0C;  --bg-app:#0D0E11;  --bg-sidebar:#101114;
--bg-panel:#121316; --bg-card:#15171A; --bg-card-hover:#191B1F;
--bg-input:#0D0F12; --bg-elevated:#1A1C20;
/* Border */
--border-subtle:#24262B; --border-default:#2C2F35; --border-strong:#363941;
/* Text */
--text-primary:#F2F3F5; --text-secondary:#A3A6AD;
--text-tertiary:#737780; --text-disabled:#4E5158;
/* Reals Orange */
--accent:#FF6B2C; --accent-hover:#FF7A3D; --accent-active:#E9571D;
--accent-soft:rgba(255,107,44,.12); --accent-border:rgba(255,107,44,.35);
/* Badges */
FREE: rgba(34,197,94,.12) / #35D07F
PRO: rgba(255,107,44,.12) / #FF7A3D
Update: rgba(59,130,246,.12) / #55A5FF
```

## 3. App Shell
- Outer window: Custom Frameless Window tràn viền, bo góc nhẹ hiện đại 10px (`border-radius: 10px`, `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND`), viền tinh tế `#24262B`, lớp Analog Noise Grain phủ 100% toàn bộ bề mặt cửa sổ.
- Khi phóng to toàn màn hình (Maximized): tự động chuyển sang chế độ phẳng vuông góc không viền (`border-radius: 0`).
- Layout: Top Bar (Tabs + Drag Area + Window Controls) → Body Row (Sidebar | Content) → Status Bar.

## 4. Top Bar (Header & Navigation & Window Controls)
- Height 44px (khi nav-top) hoặc 36px (khi nav-left/right/bottom).
- Khu vực bên trái: Thanh Tab điều hướng (Market, Audio Lab, Agent, Browser, Tài khoản).
- Khu vực giữa: Vùng kéo thả cửa sổ (`window-drag-region`, bấm giữ để di chuyển).
- Khu vực bên phải: Nút Cài đặt (⚙️) và Bộ 3 nút điều khiển cửa sổ (`—` Thu nhỏ, `▢`/`❐` Phóng to/Khôi phục, `✕` Đóng - hover đỏ).

## 7. Main Content
- Padding 24px, bg #121316.
- Search: h48, radius 10, bg #0D0F12, border #292C31.
  - Focus: border rgba(255,107,44,.55) + ring 3px rgba(255,107,44,.08).
  - Shortcut "Ctrl+K" bên phải, màu tertiary.

## 8. Filter Chips
- Inactive: bg #15171A, border #272A30, color #8F939B.
- Active: bg #FF6B2C, text trắng. Height 34, radius 17, gap 8.

## 9. Section Header
- 12px/600, ls 1.5px, #737780, uppercase. Margin: top 28, bottom 12.

## 10. Plugin Card (component quan trọng nhất)
- Height 72–82, padding 14–18, bg #15171A, border #24272C, radius 11.
- Hover: bg #191B1F, border #30333A, translateY(-1px), transition 120–160ms.
- **Không hover glow cam.**

## 11. Plugin Icon
- 56×56, radius 8, font 18–20/700. Màu riêng từng plugin (UI xung quanh không đổi).

## 12. Typography Plugin
- Tên: 15–16/600, #F1F2F4. Meta: 12, #777B84. Badge height 20.

## 13. Buttons
- Primary: bg #FF6B2C (hover #FF7A3D, active #E9571D), radius 9, h36, padding 0 16, /600.
- Secondary: bg #1A1C20, border #30333A, color #D7D9DD.
- **Price button KHÔNG cam** — dùng secondary (để Download/Update nổi bật hơn).

## 14. Spacing Tokens
4 / 8 / 12 / 16 / 20 / 24 / 32 / 40 / 48
- icon↔text: 10–12 · card padding: 16 · card↔card: 10–12 · section↔section: 28–36 · main edge: 24

## 15. Typography System
- Font: **Inter** (hoặc Geist).
- App title 16–18/700 · Page title 20–24/650 · Plugin name 15–16/600 · Body 13–14/400 · Metadata 12/400 · Label 11–12/600.
- Không lạm dụng 700/800 — hierarchy nhẹ.

## 16. Border Philosophy
- Normal → subtle (#24262B). Hover → visible. Active/Focus → orange.
- Không border #555 khắp nơi.

## 17. Shadow
- Card: none. Modal/dropdown: 0 12px 40px rgba(0,0,0,.35).
- Không orange glow / neon / shadow to.

## 18. Animation
- hover/button 120ms, dropdown 160ms, modal 180ms. Ease.
- Chỉ: opacity, transform, background, border-color.
- Không: bounce, elastic, scale to, neon.

## 20. Vị trí thanh điều hướng (bổ sung 2026-08-24)
- Nav đặt được **4 vị trí: Trên / Dưới / Trái / Phải** — user tự chọn trong Cài đặt (⚙).
- **Mặc định: Trên** (ngang, nằm dưới update banner).
- Active indicator: vạch cam 2px theo cạnh tương ứng (trên → vạch dưới item, v.v.).
- Khi ngang: item padding 0 16, icon + text inline.

## 21. Nội dung bên trong — Desktop App Feel (bổ sung 2026-08-24)
- Tool dùng **toolbar compact** (nút nhỏ icon + text ngang), không grid nút to kiểu web.
- Panel kết quả có **header với border-bottom** phân tách body.
- List dùng row gọn, có toolbar (path bar + view toggle).
- Radius control nhỏ: 8–9px. Padding chặt hơn nhưng vẫn thoáng.

## 22. Đồng bộ Âm thanh & Kéo thả Timeline (bổ sung 2026-08-28)
- **Playhead Phase Sync Preview**: Khi nút `Sync BPM` sáng cam và DAW đang phát, sóng âm và con trỏ phát tự động nhảy vào đúng vị trí Bar/Beat tương ứng với Timeline DAW, mang lại trải nghiệm nghe thử nhịp điệu liền mạch.
- **DAW Drag & Drop Alignment (Cơ chế A — REAPER Native Drag)**: Khi kéo thả mẫu âm thanh vào track REAPER, file gốc của người dùng được kéo thả tức thì (0ms lag, không tốn thời gian render file tạm). Khi thả vào timeline, Take tự động co giãn `D_LENGTH`, gán `D_PLAYRATE`, bật `B_PPITCH = 1` và `D_PITCH` để khớp chính xác 100% từng ô nhịp (Grid Bar) với chất lượng time-stretch native của REAPER.
- **Triệt tiêu Double-DSP (Cơ chế B — Pre-baked Safeguard)**: Nếu file kéo vào là file đã render sẵn (`drag_` / `drag_export`), hệ thống tự động khóa `D_PLAYRATE = 1.0` và `D_PITCH = 0.0` để bảo vệ âm thanh không bị méo tiếng do xử lý trùng lặp.

## 19. 8 Nguyên Tắc Vàng
1. Dark first
2. Orange is action, not decoration
3. Gray establishes hierarchy
4. Borders are subtle
5. Cards are flat, not floating
6. Typography carries hierarchy
7. Motion is fast and restrained
8. Audio-software feeling > generic web-dashboard feeling
