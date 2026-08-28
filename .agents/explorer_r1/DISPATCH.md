## 2026-08-25T13:51:55Z
Bạn là Explorer chuyên trách R1: C++ Core, Audio & Architecture Audit cho dự án Reals Lab.

Working directory của bạn: `.agents/explorer_r1/` (tạo `BRIEFING.md`, `progress.md`, `handoff.md`).

Đọc kỹ tài liệu gốc:
- `c:\Users\smk28\Desktop\reals lab extension\.agents\ORIGINAL_REQUEST.md`
- `c:\Users\smk28\Desktop\reals lab extension\AGENTS.md`
- `c:\Users\smk28\Desktop\reals lab extension\SPEC.md`
- `c:\Users\smk28\Desktop\reals lab extension\PLAN.md`

QUY TẮC BẮT BUỘC:
- Sử dụng GitNexus MCP tools (`call_mcp_tool` với `ServerName: "gitnexus"` như `query`, `context`, `impact`, `detect_changes`) để phân tích mã nguồn, call graph, luồng thực thi và blast radius.
- Rà soát toàn bộ các file trong:
  + `core/src/audio/` và `core/include/reals/audio/`
  + `core/src/model/`, `core/include/reals/model/`
  + `core/src/config/`, `core/include/reals/config/`
  + `core/src/i18n/`, `core/include/reals/i18n/`
  + `core/src/platform/`, `core/include/reals/platform/`
  + `core/include/reals/`

Nhiệm vụ cụ thể:
1. Audio Engine & Thread Safety
2. Memory & Object Lifecycle
3. Windows Unicode / Path Handling
4. Architecture Violations & Compiler Warnings
