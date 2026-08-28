# Original User Request

## 2026-08-28T15:39:51Z

Xây dựng và sửa triệt để hệ thống đồng bộ âm thanh và kéo thả DAW chuyên nghiệp cho Reals Lab (REAPER Extension & C++ Standalone App), đảm bảo 2 tính năng cốt lõi hoạt động chuẩn xác 100% không xung đột DSP / Take Playrate:
1. **Playhead Phase Synchronization (Đồng bộ Pha Nhịp Bar/Beat khi Preview)**.
2. **DAW Drag & Drop Alignment (Kéo thả Khớp chuẩn Grid Bar không bị Double-DSP / Double-Stretch, Zero Lag)**.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

---

## Root Cause Analysis & Architecture Design

### Vấn đề 1: Double Time-Stretch / Double Pitch-Shift khi Kéo thả vào REAPER
- **Hiện tượng**: Khi kéo sample vào REAPER, âm thanh bị méo tiếng, chạy nhanh gấp đôi (double speed), cao độ bị tăng gấp đôi và độ dài item bị co ngắn sai lệch.
- **Nguyên nhân gốc**: 
  - `DragExporter` đã render sẵn file tạm `drag_xxx.wav` đã qua SoundTouch time-stretch & pitch-shift.
  - Khi thả file tạm vào REAPER, hook `processPendingSyncPlayrates()` lại tiếp tục tìm thấy `drag_xxx.wav` và gán thêm `D_PLAYRATE`, `D_PITCH`, `D_LENGTH` một lần nữa trên Take!
  - Kết quả: File bị xử lý time-stretch 2 lần liên tiếp (1.166 * 1.166 = 1.36x), pitch tăng 2 lần (+5st + 5st = +10st).
- **Giải pháp chuẩn**:
  - Chọn 1 trong 2 cơ chế kiến trúc rõ ràng:
    - **Cơ chế A (Khuyên dùng cho REAPER Native Extension)**: Kéo file gốc trực tiếp (`CF_HDROP = p`). Khi thả vào timeline REAPER, `processPendingSyncPlayrates()` gán `D_PLAYRATE`, `B_PPITCH = 1`, `D_PITCH` và co giãn `D_LENGTH` chuẩn số bar. File trong project trỏ về file thật vĩnh viễn (không bị mất file khi dọn Temp), kéo thả mượt 0ms không lag, chất lượng stretch bằng thuật toán native của REAPER (Élastique 3 / RubberBand).
    - **Cơ chế B (Bake WAV Export)**: Nếu kéo file đã render sẵn `drag_xxx.wav` (để dùng cho Sampler plugin ngoài như Serum/Kontakt), thì Take trong REAPER PHẢI giữ `D_PLAYRATE = 1.0` và `D_PITCH = 0.0`, tuyệt đối KHÔNG áp dụng `processPendingSyncPlayrates` lần 2.

### Vấn đề 2: Lệch pha nhịp và giật hạt khi Preview Playhead Phase Sync
- **Hiện tượng**: Khi REAPER đang phát ở Bar 3 Beat 2, bấm preview sample thì sample phát bị lệch pha hoặc giật tiếng lúc khởi động.
- **Nguyên nhân gốc**:
  - `TimeMap2_timeToBeats` và `GetPlayPosition()` lấy thời gian hiện tại của DAW.
  - Khi `playFile` seek decoder đến `startFrame = startFraction * totalFrames`, bộ đệm nội bộ của SoundTouch và ring buffer miniaudio chưa được flush/reset đồng bộ với tỉ lệ stretch mới, dẫn đến vài frame đầu tiên bị trôi lệch nhịp.
- **Giải pháp chuẩn**:
  - Chuẩn hóa công thức pha nhịp:
    rawBeats = (durationSeconds * sampleBpm) / 60.0
    loopBeats = std::max(1.0, std::round(rawBeats))
    beatInLoop = std::fmod(transport.fullBeats, loopBeats)
    startFraction = std::clamp(beatInLoop / loopBeats, 0.0, 0.999)
  - Thiết lập `setTimeRatio` và `setPitchSemitones` trên `SoundTouchProcessor` TRƯỚC KHI mở stream, đồng thời clear toàn bộ pipeline DSP để phát ngay tức thì không độ trễ.

---

## Requirements

### R1. Đồng bộ Pha Nhịp Chuẩn xác 100% khi Preview (DAW Playhead Phase Sync)
- Khi REAPER đang phát (`GetPlayState() & 1`) và bật `Sync BPM`:
  - Lấy tọa độ nhịp `fullBeats` hiện tại từ REAPER SDK.
  - Tự động nhận diện chu kỳ bar của sample (1 bar = 4 beats, 2 bars = 8 beats, 4 bars = 16 beats...).
  - Seek decoder chính xác tới phách tương ứng trong bài nhạc.
  - Khởi động audio stream lập tức (< 15ms latency), âm thanh mượt mà không click/pop/artefact.
- Khi REAPER dừng phát (`GetPlayState() == 0`): phát từ đầu (`startFraction = 0.0`).

### R2. Kéo thả Khớp chuẩn Grid Bar REAPER, Zero Lag & Không Double-DSP
- Khi kéo sample từ Reals Lab vào track REAPER:
  - Thao tác bắt đầu kéo (pointer down & drag) phản hồi tức thì 0ms, không gây khựng hay lag chuột.
  - Item được tạo trên timeline REAPER khớp chuẩn 100% từng ô nhịp (Grid Bar), độ dài vừa vặn với số bar của bài nhạc.
  - Tuyệt đối không xảy ra hiện tượng double time-stretch hoặc double pitch-shift.
  - Đường dẫn file trong REAPER project trỏ đúng file thực tế trên ổ đĩa.

### R3. Kiểm thử Toàn diện & Zero-Warning Build
- Toàn bộ 183+ unit/integration tests trong `reals_tests.exe` phải PASS 100%.
- Biên dịch C++20 MSVC không có warning (`-Wall -Wextra /W4`).
- Tự động triển khai file DLL đã fix vào `%APPDATA%\REAPER\UserPlugins\reaper_realslab.dll`.

---

## Acceptance Criteria

### A1. Playhead Phase Sync Preview
- [ ] REAPER đang phát ở bất kỳ vị trí nào (ví dụ Bar 3 Beat 2), bấm preview loop 4-bar lập tức phát đúng vị trí Bar 3 Beat 2 đồng bộ với tiếng click metronome của REAPER.
- [ ] Khi dừng REAPER, preview phát từ đầu bình thường.

### A2. Drag & Drop Grid Match
- [ ] Kéo loop 120 BPM vào project REAPER 140 BPM (Sync ON): item trên track REAPER co giãn đúng bằng số bar chuẩn của project 140 BPM.
- [ ] Tốc độ phát lại trong REAPER đúng chuẩn 140 BPM (1.166x), cao độ giữ nguyên hoặc đúng semitone đã chọn trên Mini Piano, không bị méo tiếng do double stretch.
- [ ] File tham chiếu trong track REAPER là file gốc của người dùng.

### A3. Hiệu năng & Ổn định
- [ ] Thao tác kéo chuột mượt mà 60fps không bị đứng hình (0ms freeze).
- [ ] 100% automated test suite pass trên Windows.
