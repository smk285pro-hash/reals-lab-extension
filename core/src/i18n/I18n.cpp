#include "reals/i18n/I18n.h"

#include "reals/platform/Path.h"

#include <fstream>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace reals::i18n {

namespace {
std::mutex g_mutex;
using Table = std::unordered_map<std::string, std::string>;
Table g_vi;
Table g_en;
std::string g_lang = "vi";

// Embedded fallback tables — the UI works even when no i18n files exist on
// disk (e.g. the extension loads before assets are installed).
struct Entry {
    const char* key;
    const char* vi;
    const char* en;
};

constexpr Entry kEmbedded[] = {
    {"app.title", "REALS LAB", "REALS LAB"},
    {"nav.market", "Market", "Market"},
    {"nav.audioLab", "Audio Lab", "Audio Lab"},
    {"nav.agent", "Agent", "Agent"},
    {"nav.browser", "Browser", "Browser"},
    {"nav.account", "Tài khoản", "Account"},
    {"update.available", "đã sẵn sàng — bổ sung Audio Lab", "is ready — Audio Lab added"},
    {"update.button", "Update", "Update"},
    {"status.connected", "Đã kết nối", "Connected"},
    {"status.disconnected", "Mất kết nối", "Disconnected"},
    {"settings.navPosition", "Vị trí thanh điều hướng", "Navigation position"},
    {"settings.accent", "Màu nhấn", "Accent color"},
    {"settings.noiseOverlay", "Lớp phủ noise", "Noise overlay"},
    {"settings.language", "Ngôn ngữ", "Language"},
    {"settings.browser", "Thư mục & Duyệt", "Folder tree"},
    {"settings.autoCollapse", "Tự động thu gọn thư mục", "Auto-collapse folders"},
    {"settings.displaySize", "Kích thước hiển thị", "Display density"},
    {"size.small", "Nhỏ (gọn)", "Compact"},
    {"size.medium", "Vừa (chuẩn)", "Standard"},
    {"size.large", "To (thoáng)", "Large"},
    {"pos.top", "Trên", "Top"},
    {"pos.bottom", "Dưới", "Bottom"},
    {"pos.left", "Trái", "Left"},
    {"pos.right", "Phải", "Right"},
    {"accent.orange", "Cam Reals", "Reals Orange"},
    {"accent.amber", "Cam hổ phách", "Amber"},
    {"accent.muted", "Cam trầm", "Muted Orange"},
    {"accent.gray", "Xám (monochrome)", "Gray (monochrome)"},
    {"browser.tab.local", "Local", "Local"},
    {"browser.tab.realsFree", "RealS Free", "RealS Free"},
    {"browser.comingSoon", "SẮP RA MẮT", "COMING SOON"},
    {"browser.column.name", "Tên", "Name"},
    {"browser.column.duration", "Thời lượng", "Duration"},
    {"browser.pickRoot", "Chọn thư mục...", "Pick a folder..."},
    {"browser.dropTitle", "Thêm thư mục gốc", "Add Root Folder"},
    {"browser.dropHint", "Thả thư mục từ Windows Explorer vào đây để thêm vào Thư mục gốc", "Drop folder from Windows Explorer here to add as root"},
    {"browser.searchHint", "Tìm trong thư mục...", "Search in folder..."},
    {"browser.sort.name", "Tên", "Name"},
    {"browser.sort.size", "Dung lượng", "Size"},
    {"browser.sort.date", "Ngày sửa", "Date"},
    {"browser.audioOnly", "Chỉ audio", "Audio only"},
    {"browser.autoPreview", "Auto", "Auto"},
    {"browser.favOnly", "Yêu thích", "Favorites"},
    {"browser.tagFilter", "Nhãn", "Tag"},
    {"browser.parent", "Thư mục cha", "Parent folder"},
    {"browser.searching", "Đang tìm...", "Searching..."},
    {"browser.loadingSamples", "Đang tải sample...", "Loading samples..."},
    {"browser.ctx.clearRecents", "Xóa vừa mở", "Clear recents"},
    {"browser.refresh", "Làm mới", "Refresh"},
    {"browser.favorites", "Yêu thích", "Favorites"},
    {"browser.recents", "Vừa mở", "Recent"},
    {"browser.empty", "Trống", "Empty"},
    {"browser.results", "Kết quả", "Results"},
    {"browser.size", "Dung lượng:", "Size:"},
    {"browser.modified", "Sửa:", "Modified:"},
    {"browser.play", "Phát", "Play"},
    {"browser.stop", "Dừng", "Stop"},
    {"browser.loop", "Lặp", "Loop"},
    {"browser.sendLab", "Gửi Audio Lab", "Send to Audio Lab"},
    {"browser.volume", "Âm lượng", "Volume"},
    {"browser.ctx.preview", "Nghe thử", "Preview"},
    {"browser.ctx.insert", "Chèn vào project", "Insert into project"},
    {"browser.ctx.tag", "Nhãn màu", "Color tag"},
    {"browser.tag.none", "Không nhãn", "No tag"},
    {"browser.ctx.copyPath", "Copy đường dẫn", "Copy path"},
    {"browser.ctx.reveal", "Mở vị trí file", "Reveal in Explorer"},
    {"browser.ctx.rename", "Đổi tên", "Rename"},
    {"browser.ctx.delete", "Xóa", "Delete"},
    {"browser.ctx.findSimilar", "🔍 Tìm sample tương tự", "🔍 Find Similar Samples"},
    {"browser.similarTo", "Tương tự như", "Similar to"},
    {"browser.clearSimilar", "Xóa bộ lọc", "Clear filter"},
    {"browser.matchPercent", "khớp", "match"},
    {"browser.ctx.setRoot", "Đặt làm thư mục gốc", "Set as root"},
    {"browser.ctx.scanFolder", "⚡ Quét & Phân tích AI", "⚡ Scan & AI Analyze"},
    {"browser.ctx.scanNew", "⚡ Quét file mới", "⚡ Scan New Files"},
    {"browser.ctx.rescanAll", "🔄 Quét lại toàn bộ AI", "🔄 Rescan All (Re-analyze)"},
    {"browser.ctx.openHere", "Mở ở đây", "Open here"},
    {"browser.ctx.removeRoot", "Xóa thư mục gốc này", "Remove this root"},
    {"browser.lab.stem", "Tách Stem", "Split Stem"},
    {"browser.lab.denoise", "Lọc Noise", "Denoise"},
    {"browser.lab.keychord", "Key & Hợp âm", "Key & Chords"},
    {"browser.lab.tempo", "Detect Tempo", "Detect Tempo"},
    {"browser.lab.midi", "Xuất MIDI", "Export MIDI"},
    {"browser.rename.title", "Đổi tên file:", "Rename file:"},
    {"browser.delete.title", "Xóa file này?", "Delete this file?"},
    {"browser.delete.confirm", "Xóa", "Delete"},
    {"browser.toast.decodeFail", "Không đọc được file audio", "Cannot decode audio file"},
    {"browser.toast.copied", "Đã copy đường dẫn", "Path copied"},
    {"browser.toast.rootAdded", "Đã thêm thư mục gốc", "Root added"},
    {"browser.toast.renamed", "Đã đổi tên", "Renamed"},
    {"browser.toast.renameFail", "Đổi tên thất bại", "Rename failed"},
    {"browser.toast.deleted", "Đã xóa", "Deleted"},
    {"browser.toast.deleteFail", "Xóa thất bại", "Delete failed"},
    {"browser.toast.labQueued", "Đã gửi tới Audio Lab (mở ở Phase 2)", "Sent to Audio Lab (opens in Phase 2)"},
    {"browser.toast.inserted", "Đã chèn vào project", "Inserted into project"},
    {"browser.toast.notMedia", "File này không phải media", "Not a media file"},
    {"toast.copied", "Đã copy đường dẫn", "Path copied"},
    {"toast.inserted", "Đã chèn vào project", "Inserted into project"},
    {"toast.notMedia", "File này không phải media", "Not a media file"},
    {"toast.labQueued", "Đã gửi tới Audio Lab (mở ở Phase 2)", "Sent to Audio Lab (opens in Phase 2)"},
    {"toast.decodeFail", "Không đọc được file audio", "Cannot decode audio file"},
    {"toast.rootAdded", "Đã thêm thư mục gốc", "Root added"},
    {"toast.dropHint", "Kéo thư mục vào đây để thêm gốc", "Drop a folder here to add a root"},
    {"toast.notFolder", "Kéo một thư mục (không phải file) để thêm gốc", "Drop a folder (not a file) to add a root"},
    {"toast.renamed", "Đã đổi tên", "Renamed"},
    {"toast.renameFail", "Đổi tên thất bại", "Rename failed"},
    {"toast.deleted", "Đã xóa", "Deleted"},
    {"toast.deleteFail", "Xóa thất bại", "Delete failed"},
    {"market.search", "Tìm plugin, script, VST...", "Search plugins, scripts, VST..."},
    {"market.trending", "Đang thịnh hành", "Trending"},
    {"market.installed", "Đã cài", "Installed"},
    {"market.download", "Tải về", "Download"},
    {"market.chip.all", "Tất cả", "All"},
    {"market.chip.effects", "Effects", "Effects"},
    {"market.chip.midi", "MIDI", "MIDI"},
    {"market.chip.utility", "Utility", "Utility"},
    {"market.chip.scripts", "Scripts", "Scripts"},
    {"market.chip.free", "Miễn phí", "Free"},
    {"lab.title", "AUDIO LAB", "AUDIO LAB"},
    {"lab.noFile", "Chưa chọn file — chọn trong Browser (click phải → Gửi Audio Lab)", "No file selected — pick in Browser (right-click → Send to Audio Lab)"},
    {"lab.sub.stem", "vocal · drum · bass · other", "vocal · drum · bass · other"},
    {"lab.sub.denoise", "làm sạch audio", "clean up audio"},
    {"lab.sub.keychord", "phát hiện + MIDI", "detect + MIDI"},
    {"lab.sub.tempo", "phát hiện BPM", "detect BPM"},
    {"lab.apiLive", "API thật: analyze · chords · separate · denoise", "Real API: analyze · chords · separate · denoise"},
    {"agent.modes", "CHẾ ĐỘ PHÉP QUYỀN", "PERMISSION MODE"},
    {"agent.mode1", "Hỏi tất cả", "Ask all"},
    {"agent.mode2", "Chỉ hỏi nguy hiểm", "Ask dangerous only"},
    {"agent.mode3", "Toàn quyền", "Full control"},
    {"agent.hint", "Ra lệnh cho agent... (VD: lọc noise all track audio)", "Command the agent... (e.g. denoise all audio tracks)"},
    {"agent.apiStub", "Phase 5 — cần API LLM từ server RealS", "Phase 5 — needs RealS server LLM API"},
    {"account.notLogin", "Chưa đăng nhập", "Not logged in"},
    {"account.loginHint", "Đăng nhập bằng tài khoản reals.media", "Sign in with your reals.media account"},
    {"account.login", "Đăng nhập", "Sign in"},
    {"account.apiStub", "Phase 4 — chờ hệ thống đăng nhập trên web chính", "Phase 4 — waiting for web login system"},
    {"player.syncBpm", "Sync BPM", "Sync BPM"},
    {"player.keyTransposer", "Chuyển Tone", "Key Transposer"},
    {"player.originalKey", "Original Key", "Original Key"},
    {"player.transposer", "Bàn phím chuyển Tone", "Tone Transposer"},
    {"player.semitones", "bán cung", "semitones"},
    {"player.tags", "Nhãn", "Tags"},
    {"scanner.starting", "Bắt đầu quét & phân tích AI...", "Starting scan & AI analysis..."},
    {"scanner.scanning", "Đang quét & phân tích AI...", "Scanning & AI analyzing..."},
    {"scanner.complete", "✓ Quét & Phân tích AI hoàn tất", "✓ Scan & AI analysis complete"},
    {"scanner.cancelled", "Đã dừng quét", "Scan stopped"},
    {"scanner.cancel", "Dừng", "Stop"},
    {"scanner.addedCount", "Đã thêm", "Added"},
    {"scanner.skippedCount", "Bỏ qua", "Skipped"},
    {"scanner.cpuMode", "Hiệu năng quét (CPU)", "Scan Performance (CPU)"},
    {"scanner.cpuMode.low", "Thấp (30% CPU)", "Low (30% CPU)"},
    {"scanner.cpuMode.lowDesc", "Êm ái, không ảnh hưởng DAW hay tác vụ khác", "Quiet, no impact on DAW or background tasks"},
    {"scanner.cpuMode.normal", "Bình thường (50% CPU)", "Normal (50% CPU)"},
    {"scanner.cpuMode.normalDesc", "Cân bằng hiệu năng & độ mượt (Khuyên dùng)", "Balanced performance & smoothness (Recommended)"},
    {"scanner.cpuMode.high", "Cao (85% CPU)", "High (85% CPU)"},
    {"scanner.cpuMode.highDesc", "Quét nhanh đa luồng, dùng tối đa 85% CPU", "Fast multi-threaded scan, utilizes up to 85% CPU"},
    {"scanner.cpuMode.highWarn", "⚠️ CẢNH BÁO: Chế độ Cao (85% CPU) sẽ huy động 85% hiệu năng đa luồng của chip máy tính. Điều này có thể khiến quạt máy quay mạnh hoặc gây giật lag nhẹ nếu bạn đang phát dự án nặng trong DAW. Bạn có chắc muốn tiếp tục?", "⚠️ WARNING: High mode (85% CPU) will utilize 85% of multi-core CPU power. This may cause higher fan speed or minor audio dropouts during heavy DAW project playback. Are you sure you want to continue?"},
    {"market.apiStub", "Đang phát triển — kết nối reals.media khi mở API", "Under development — connects to reals.media when API is live"},
    {"market.installedNote", "Chưa có sản phẩm nào được cài qua Reals Lab.", "No products installed via Reals Lab yet."},
    {"market.tagUpdate", "CẬP NHẬT", "UPDATE"},
    {"settings.dockToReaper", "Dock vào REAPER", "Dock to REAPER"},
    {"settings.effects", "Hiệu ứng", "Effects"},
    {"settings.noise", "Lớp phủ noise", "Noise overlay"},
    {"settings.window", "Cửa sổ & Dock", "Window & Dock"},
    {"window.dock", "📌 Dock", "📌 Dock"},
    {"window.undock", "↗ Tách cửa sổ", "↗ Undock window"},
    {"common.cancel", "Hủy", "Cancel"},
    {"common.confirm", "Xác nhận", "Confirm"},
    {"common.close", "Đóng", "Close"},
    {"window.dockHint", "Dock vào REAPER / Cửa sổ riêng", "Dock into REAPER / Float window"},
    {"window.settingsHint", "Cài đặt", "Settings"},
    {"window.minimize", "Thu nhỏ", "Minimize"},
    {"window.maximize", "Phóng to / Khôi phục", "Maximize / Restore"},
    {"window.close", "Đóng", "Close"},
    {"browser.toggleTree", "Ẩn/Hiện Cây thư mục", "Show/hide folder tree"},
    {"splitter.tree", "Kéo để chỉnh độ rộng Cây thư mục", "Drag to resize the folder tree"},
    {"splitter.preview", "Kéo để chỉnh chiều cao Trình phát", "Drag to resize the player"},
    {"browser.noResults", "Không tìm thấy mẫu tương tự", "No similar samples found"},
    {"toast.labError", "Lỗi Lab", "Lab error"},
    {"toast.scannerError", "Lỗi quét", "Scanner error"},
    {"toast.similarError", "Lỗi tìm mẫu tương tự", "Error finding similar samples"},
    {"sync.noBpm", "Sync: không tìm thấy BPM, thử 120", "Sync: no BPM found, trying 120"},
    {"player.dragTip", "Kéo vào REAPER", "Drag to REAPER"},
    {"browser.addFolder", "Thêm thư mục Sample", "Add Sample Folder"},
    {"settings.title", "Cài đặt Reals Lab", "Reals Lab Settings"},
    {"settings.tab.general", "Chung (General)", "General"},
    {"settings.tab.browser", "Duyệt File (Browser)", "File Browser"},
    {"settings.tab.market", "Marketplace", "Marketplace"},
    {"settings.tab.stem", "Tách Stem (Reals Stem)", "Reals Stem Separation"},
    {"settings.tab.agent", "Agent AI", "Agent AI"},
    {"settings.theme", "Giao diện (Theme)", "Theme"},
    {"theme.darkStudio", "Dark Studio", "Dark Studio"},
    {"theme.pastelPink", "Pastel Pink", "Pastel Pink"},
    {"theme.cyberpunk", "Cyberpunk", "Cyberpunk"},
    {"common.enabled", "Bật", "Enabled"},
    {"common.disabled", "Tắt", "Disabled"},
    {"market.comingSoon", "Marketplace — Sắp ra mắt", "Marketplace — Coming Soon"},
    {"market.placeholderDesc", "Kho thư viện Sample Pack, Preset và Plugin bản quyền từ cộng đồng sáng tạo Reals.", "Library of Sample Packs, Presets, and Plugins from the Reals creative community."},
    {"stem.comingSoon", "Reals Stem Separation — Sắp ra mắt", "Reals Stem Separation — Coming Soon"},
    {"stem.placeholderDesc", "Tách Vocals, Drums, Bass, Instruments chuẩn xác bằng AI tích hợp trực tiếp trong REAPER.", "Precise AI-powered stem separation directly inside REAPER."},
    {"agent.comingSoon", "Agent AI — Sắp ra mắt", "Agent AI — Coming Soon"},
    {"agent.placeholderDesc", "Trợ lý AI tự động hóa chỉnh sửa, định tuyến và sáng tác thông minh trong DAW.", "Intelligent AI assistant for DAW automation, routing, and composition."},
    {"lab.alreadyRunning", "Đang có job chạy — chờ xong đã nhé", "A job is already running — please wait"},
};

void applyEmbedded(Table& vi, Table& en) {
    for (const auto& e : kEmbedded) {
        vi[e.key] = e.vi;
        en[e.key] = e.en;
    }
}

bool loadTable(const std::string& path, Table& out) {
    std::ifstream in(path);
    if (!in)
        return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto data = nlohmann::json::parse(content, nullptr, false);
    if (data.is_discarded() || !data.is_object())
        return false;
    for (auto it = data.begin(); it != data.end(); ++it)
        if (it.value().is_string())
            out[it.key()] = it.value().get<std::string>();
    return true;
}
} // namespace

void init(std::string_view assetsDir) {
    const std::lock_guard lock(g_mutex);
    g_vi.clear();
    g_en.clear();
    applyEmbedded(g_vi, g_en);

    // Disk files override embedded strings when present.
    const std::string dir(assetsDir);
    if (!dir.empty()) {
        loadTable(platform::joinPath(dir, "strings_vi.json"), g_vi); // MIN-08: platform::joinPath
        loadTable(platform::joinPath(dir, "strings_en.json"), g_en);
    }
}

void setLanguage(std::string_view lang) {
    const std::lock_guard lock(g_mutex);
    g_lang = std::string(lang);
}

std::string currentLanguage() {
    const std::lock_guard lock(g_mutex);
    return g_lang;
}

std::string tr(std::string_view key) {
    const std::lock_guard lock(g_mutex);
    const std::string k(key);
    if (g_lang == "en") {
        if (const auto it = g_en.find(k); it != g_en.end())
            return it->second;
    }
    if (const auto it = g_vi.find(k); it != g_vi.end())
        return it->second;
    if (const auto it = g_en.find(k); it != g_en.end())
        return it->second;
    return k;
}

} // namespace reals::i18n
