# AGENTS.md — Quy tắc làm việc dự án Reals Lab

> File này là LUẬT cho bất kỳ AI agent nào làm việc trên repo. Đọc TRƯỚC khi code.

## 0. Ngôn ngữ & phong cách giao tiếp
- Trò chuyện với chủ repo bằng **tiếng Việt**, ngắn gọn, kiểu 2 người bạn.
- Code, comment (khi cần), commit message, tài liệu kỹ thuật: **tiếng Anh**.
- UI text: **KHÔNG BAO GIỜ hardcode** — luôn qua `tr("key")`, string nằm trong `assets/i18n/`.

## 1. Tài liệu nguồn sự thật (đọc trước khi làm bất kỳ việc gì)
| File | Nội dung |
|---|---|
| `PLAN.md` | Ý tưởng + mọi quyết định đã chốt (kèm ngày) |
| `DESIGN.md` | Design system — màu, spacing, typography, 8 nguyên tắc |
| `SPEC.md` | Kiến trúc, API contract, phases, quy ước kỹ thuật |

**Quy tắc vàng: chốt được điều gì → ghi ngay vào `PLAN.md` kèm ngày. Không ghi = không chốt.**

## 2. Quy tắc kiến trúc (VI PHẠM = phải sửa ngay)
- `core/` KHÔNG được include: ImGui, GLFW, reaper_plugin.
- `ui/` KHÔNG được include: GLFW, reaper_plugin — chỉ nhận interface từ shell.
- `app/` và `extension/` là shell mỏng — logic nghiệp vụ phải nằm ở `core/`.
- Network chỉ qua `net::HttpClient`. Path chỉ qua `platform::`. Audio chỉ qua `audio::Engine`.
- Mọi tính năng phải chạy được cả 2 shell (app + extension) nếu dùng core/ui.

## 3. Quy tắc code
- C++20. Zero-warning (`-Wall -Wextra`). Không `using namespace std;` trong header.
- PascalCase class/function · camelCase biến · `m_` member · `k` constant.
- 1 class trách nhiệm rõ ràng; file ≤ ~400 dòng thì tách.
- Smart pointers (`unique_ptr`/`shared_ptr`), không owning raw `new`.
- Thread: mọi call audio API từ audio thread KHÔNG lock/allocate; giao tiếp qua lock-free queue hoặc atomic.
- Không thêm dependency mới nếu chưa bàn + ghi PLAN.md.

## 4. Quy tắc làm việc từng phase
1. Đọc phase hiện tại trong `SPEC.md` mục 6 — làm đúng phase, KHÔNG nhảy cóc.
2. Trước khi code: liệt kê file sẽ sửa + ảnh hưởng (blast radius) cho chủ repo duyệt.
3. Code xong: build zero-warning + tự test luồng chính trước khi báo xong.
4. Xong phase: cập nhật checklist trong SPEC.md, mới sang phase sau.

## 5. Build & kiểm tra
```powershell
cmake --preset windows          # configure
cmake --build --preset windows  # build
ctest --preset windows          # test (khi có)
```
- Windows build PHẢI pass trước khi commit. Mac/Linux kiểm tra bằng review (CI sau).
- Mọi PR/commit: `feat(scope): ...`, `fix(scope): ...` — scope ∈ core/ui/app/extension/browser/lab/market/agent.

## 6. Quản lý tri thức
- Quyết định mới → `PLAN.md`. Thay đổi design → `DESIGN.md`. Thay đổi kiến trúc/API → `SPEC.md` (bump version).
- Bug khó: ghi nguyên nhân gốc + cách sửa vào cuối PLAN.md (mục "Bài học").
- Context phiên làm việc mới: đọc lại 3 file docs trước khi trả lời.

## 7. An toàn
- Không commit secret/key/token. `.env`, `*.key` trong `.gitignore` sẵn.
- Không xóa/sửa file docs (PLAN/DESIGN/SPEC/AGENTS) nếu không được chủ repo đồng ý.
- Agent AI điều khiển REAPER (tính năng trong product): mặc định bọc Undo block, action nguy hiểm phải confirm.

<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **reals-lab-extension** (2498 symbols, 5735 relationships, 171 execution flows). Use the GitNexus MCP tools to understand code, assess impact, and navigate safely.

> Index stale? Run `node .gitnexus/run.cjs analyze` from the project root — it auto-selects an available runner. No `.gitnexus/run.cjs` yet? `npx gitnexus analyze` (npm 11 crash → `npm i -g gitnexus`; #1939).

## Always Do

- **MUST run impact analysis before editing any symbol.** Before modifying a function, class, or method, run `impact({target: "symbolName", direction: "upstream"})` and report the blast radius (direct callers, affected processes, risk level) to the user.
- **MUST run `detect_changes()` before committing** to verify your changes only affect expected symbols and execution flows. For regression review, compare against the default branch: `detect_changes({scope: "compare", base_ref: "main"})`.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding with edits.
- When exploring unfamiliar code, use `query({search_query: "concept"})` to find execution flows instead of grepping. It returns process-grouped results ranked by relevance.
- When you need full context on a specific symbol — callers, callees, which execution flows it participates in — use `context({name: "symbolName"})`.
- For security review, `explain({target: "fileOrSymbol"})` lists taint findings (source→sink flows; needs `analyze --pdg`).

## Never Do

- NEVER edit a function, class, or method without first running `impact` on it.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis.
- NEVER rename symbols with find-and-replace — use `rename` which understands the call graph.
- NEVER commit changes without running `detect_changes()` to check affected scope.

## Resources

| Resource | Use for |
|----------|---------|
| `gitnexus://repo/reals-lab-extension/context` | Codebase overview, check index freshness |
| `gitnexus://repo/reals-lab-extension/clusters` | All functional areas |
| `gitnexus://repo/reals-lab-extension/processes` | All execution flows |
| `gitnexus://repo/reals-lab-extension/process/{name}` | Step-by-step execution trace |

## CLI

| Task | Read this skill file |
|------|---------------------|
| Understand architecture / "How does X work?" | `.claude/skills/gitnexus/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.claude/skills/gitnexus/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.claude/skills/gitnexus/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.claude/skills/gitnexus/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.claude/skills/gitnexus/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.claude/skills/gitnexus/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->
