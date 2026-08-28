## 2026-08-26T15:17:08Z
# Original User Request

## Initial Request — 2026-08-26T14:40:17Z

Xây dựng hệ thống quét, phân tích và tìm kiếm mẫu âm thanh thông minh hoàn toàn Local (Offline 100%) chuẩn công nghiệp cho Reals Lab (REAPER Extension & Standalone C++ App), tích hợp bộ mô hình AI chuyên sâu (TempoCNN, EDMA Key Voting, Discogs-MAEST 400-subgenre, Mood-Jamendo, CLAP Semantic Embedding), bộ xử lý DSP Time-Stretch/Pitch-Shift đồng bộ DAW, và giao diện Mini Piano Keyboard tương tác chuyển Tone trực tiếp.

Working directory: c:\Users\smk28\Desktop\reals lab extension
Integrity mode: development

---

## Requirements

### R1. Bộ suy luận AI C++ Offline (Local ONNX Runtime & Essentia Models)
- **Kiến trúc Engine**: Nhúng ONNX Runtime C++ (`core/ai/`) để chạy trực tiếp trên CPU/GPU local, không phụ thuộc vào internet hay external cloud service. Tự động kiểm tra / tải model weights về `%APPDATA%\RealsLab\models\`.
- **1. Tempo (BPM)**: Tích hợp mô hình **Essentia TempoCNN** (kèm fallback thuật toán nhịp nhanh **RhythmExtractor2013**). Xuất BPM chính xác, độ tin cậy (confidence) và vị trí beat onsets.
- **2. Key Detection & Ensemble Voting**: Tích hợp mô hình **Essentia EDMA** chuyên dụng cho EDM/Modern Production; hỗ trợ chế độ Multi-model Voting (kết hợp **Temperley** / **Krumhansl**) để tăng độ chuẩn xác cho các thể loại Acoustic/Rock/Jazz. Xuất Key + Mode (Major/Minor) + Camelo/OpenKey notation.
- **3. Genre Classification (400 Styles)**: Tích hợp **Discogs-MAEST** (hoặc Discogs-EffNet) phân loại 400 sub-genre chi tiết (ví dụ: `Minimal House`, `Nu Metal`, `Trap-EDM`, `Future Bass`).
- **4. Mood & Semantic CLAP Embedding**:
  - Multi-label **Mood-Jamendo** (happy, aggressive, relaxed, dark, party, sad...).
  - Trích xuất vector embedding **CLAP (Contrastive Language-Audio Pretraining)** (512-dim) lưu vào SQLite / Vector DB local để hỗ trợ tìm kiếm cảm xúc tự do.

### R2. Trình quét thư mục đa luồng & Tìm kiếm Cú pháp / Ngữ nghĩa (`/tag`)
- **Background Scanner**: Bộ quét thư mục bất đồng bộ (multi-threaded pool) quét hàng nghìn file sample, trích xuất metadata và lưu cache DB với hash checksum để tránh quét lại.
- **Tìm kiếm cú pháp `/`**: Trình phân tích cú pháp tìm kiếm hỗ trợ prefix `/` (ví dụ: `/trap /kick kshmr`, `/bpm:120-130 /key:F#m /ambient`).
- **Tìm kiếm Ngữ nghĩa (CLAP Semantic Search)**: Nhập từ khóa mô tả tự do ("âm thanh tối ấm áp", "lo-fi dusty vinyl", "punchy aggressive 808") và tìm kiếm theo độ tương đồng cosine vector.

### R3. Bộ xử lý DSP Time-Stretch Sync DAW & Pitch-Shift Real-time
- **Tích hợp SoundTouch / RubberBand DSP Engine** vào `core/audio/Engine`:
  - **Sync BPM với DAW**: Khi bật nút `Sync BPM`, engine tự động đọc Project BPM từ REAPER API (`Master_GetTempo()`) và time-stretch sample theo tỷ lệ tempo thực tế mà không làm thay đổi cao độ (pitch-neutral).
  - **Pitch-Shift Real-time**: Cho phép dịch chuyển cao độ (±12 semitones) trong thời gian thực khi nghe preview mà không làm đổi tốc độ phát.

### R4. Giao diện Web UI Hiện đại, Responsive & Tương tác Mini Piano Keyboard
- **Tags & Badges hiển thị trên Player**: Thanh hiển thị Tag/Mood/Genre dạng chip badge (`Soul`, `Trap`, `Vocals`, `Choir`, `Male`, `Reverb`, `Ensemble`...) ngay phía trên waveform preview.
- **Nút bấm Sync BPM**: Nút toggle `Sync BPM` trên thanh điều khiển Player với trạng thái bật/tắt (highlight accent cam).
- **Mini Piano Keyboard Popup (Key Transposer)**:
  - Khi click vào nhãn Key trên thanh điều khiển Player (ví dụ: `F# KEY`), mở popup bàn phím Piano trực quan (gồm các phím C, C#, D, D#, E, F, F#, G, G#, A, A#, B).
  - Nút **`Original Key`** màu cam để khôi phục về tone gốc ban đầu.
  - Nhấp vào bất kỳ phím piano nào sẽ ngay lập tức pitch-shift âm thanh preview sang tone đã chọn trong thời gian thực.
- **Tương thích toàn diện**: Hoạt động hoàn hảo ở cả chế độ Cửa sổ nổi tràn viền và chế độ Dock vào REAPER.

---

## Acceptance Criteria

### A1. Phân tích AI & Độ chính xác
- [ ] Phân tích chính xác BPM và Key cho các tệp WAV/MP3/FLAC tiêu chuẩn, xuất kết quả có cấu trúc JSON.
- [ ] Trích xuất được top-5 genre tags từ Discogs-MAEST và mood labels từ Mood-Jamendo.
- [ ] Vector CLAP embedding (512 float) được sinh ra và lưu thành công vào SQLite local database.

### A2. Tính năng Tìm kiếm & Quét thư mục
- [ ] Tìm kiếm cú pháp `/trap /kick` lọc chính xác các sample có tag tương ứng.
- [ ] Tìm kiếm semantic tự do ("lo-fi chill acoustic") trả về các kết quả có độ tương đồng vector cao nhất.
- [ ] Background scanner chạy mượt mà không gây đơ/lag UI chính hoặc giao diện REAPER.

### A3. DSP Sync DAW & Realtime Pitch Shifting
- [ ] Khi bật `Sync BPM`, sample 120 BPM phát trong project REAPER 140 BPM tự động khớp nhịp hoàn hảo (time-stretched 1.166x).
- [ ] Bấm phím trên Mini Piano Keyboard chuyển tone sample ngay tức thì với độ trễ < 30ms, âm thanh rõ nét không bị méo tiếng/artefact nghiêm trọng.
- [ ] Nút `Original Key` đưa tone về đúng cao độ gốc của sample.

### A4. Build & Quy tắc Kiến trúc
- [ ] Build thành công zero-warning (`cmake --build --preset windows`).
- [ ] Tuân thủ triệt để cấu trúc thư mục quy định (`core/` không dính UI, `ui-web/` tương tác qua bridge JSON-RPC).
- [ ] Chạy ổn định trong REAPER 7.x (x64) trên Windows 11/10.
