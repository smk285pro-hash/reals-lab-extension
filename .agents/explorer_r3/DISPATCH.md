## 2026-08-25T13:51:55Z
Bạn là Explorer chuyên trách R3: Web UI Frontend Audit cho dự án Reals Lab.

Working directory của bạn: `.agents/explorer_r3/` (tạo `BRIEFING.md`, `progress.md`, `handoff.md`).

Đọc kỹ tài liệu gốc:
- `c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md`
- `c:\Users\smk28\Desktop\reals lab extension\AGENTS.md`
- `c:\Users\smk28\Desktop\reals lab extension\SPEC.md`
- `c:\Users\smk28\Desktop\reals lab extension\DESIGN.md`
- `c:\Users\smk28\Desktop\reals lab extension\PLAN.md`

QUY TẮC BẮT BUỘC:
- Sử dụng GitNexus MCP tools (`call_mcp_tool` với `ServerName: "gitnexus"` như `query`, `context`, `impact`, `detect_changes`) khi cần thiết.
- Rà soát toàn bộ các file trong:
  + `ui-web/app.js`
  + `ui-web/index.html`
  + `ui-web/app.css`
  + `assets/i18n/strings_en.json`, `assets/i18n/strings_vi.json` (và bất kỳ file i18n nào khác)

Nhiệm vụ cụ thể:
1. JavaScript & i18n Synchronization:
   - Đối chiếu toàn diện giữa dictionary `I18N` trong `app.js` và các file `assets/i18n/*.json`. Tìm key bị thiếu, key thừa, key sai chính tả giữa các ngôn ngữ.
   - Tìm các chuỗi UI text bị hardcode trực tiếp trong `app.js` hoặc `index.html` mà không đi qua hàm `tr()` hoặc hệ thống đa ngôn ngữ (vi phạm nghiêm trọng AGENTS.md mục 0).
   - Rò rỉ tài nguyên: timer intervals (`setInterval`), event listeners gắn trên `window`/`document`/DOM không được cleanup khi đổi tab/pane hoặc reset state.
   - Xử lý bất đồng bộ: unhandled Promise rejections, thiếu `.catch()`, thiếu timeout cho bridge requests, race conditions khi nhận events.
2. HTML & CSS Audit:
   - Xung đột DOM ID hoặc class CSS, duplicate IDs, các element trong `index.html` bị `app.js` query bằng `getElementById` nhưng không tồn tại hoặc sai tên.
   - Container cuộn trang (`.pane-scroll`), layout flex/grid overflow, co giãn cửa sổ khi resize (bị đè, mất nút, clipping).
   - Kiểm tra 4 chế độ vị trí thanh điều hướng: `nav-top`, `nav-bottom`, `nav-left`, `nav-right` trong CSS và DOM xem có hoạt động đúng theo `DESIGN.md` không.
   - Accent colors và theme variables: biến CSS, tương phản màu sắc, switch theme (`dark`/`light`/`system`, `accent-*`).

Đầu ra yêu cầu:
Ghi báo cáo toàn diện vào `.agents/explorer_r3/handoff.md` với định dạng chi tiết từng lỗi:
- File path & Line number
- Severity: 🔴 CRITICAL | 🟠 HIGH | 🟡 MEDIUM | 🔵 LOW / REFACTOR
- Mô tả hành vi lỗi & kịch bản tái hiện (Reproduction Scenario)
- Đề xuất sửa chữa cụ thể kèm mã nguồn (code diff / snippet)
Sau đó dùng `send_message` thông báo cho Orchestrator (ID: `018ee20e-8b90-4c89-8ebe-07e527077cec`).
