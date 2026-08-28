#include "reals/i18n/I18n.h"

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
    {"browser.ctx.setRoot", "Đặt làm thư mục gốc", "Set as root"},
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
    {"browser.toast.dropHint", "Kéo thư mục vào đây để thêm gốc", "Drop a folder here to add a root"},
    {"browser.toast.notFolder", "Kéo một thư mục (không phải file) để thêm gốc",
     "Drop a folder (not a file) to add a root"},
    {"browser.toast.renamed", "Đã đổi tên", "Renamed"},
    {"browser.toast.renameFail", "Đổi tên thất bại", "Rename failed"},
    {"browser.toast.deleted", "Đã xóa", "Deleted"},
    {"browser.toast.deleteFail", "Xóa thất bại", "Delete failed"},
    {"browser.toast.labQueued", "Đã gửi tới Audio Lab (mở ở Phase 2)",
     "Sent to Audio Lab (opens in Phase 2)"},
    {"browser.toast.inserted", "Đã chèn vào project", "Inserted into project"},
    {"browser.toast.notMedia", "File này không phải media", "Not a media file"},
    {"toast.copied", "Đã copy đường dẫn", "Path copied"},
    {"toast.inserted", "Đã chèn vào project", "Inserted into project"},
    {"toast.notMedia", "File này không phải media", "Not a media file"},
    {"toast.labQueued", "Đã gửi tới Audio Lab (mở ở Phase 2)", "Sent to Audio Lab (opens in Phase 2)"},
    {"toast.decodeFail", "Không đọc được file audio", "Cannot decode audio file"},
    {"toast.rootAdded", "Đã thêm thư mục gốc", "Root added"},
    {"toast.dropHint", "Kéo thư mục vào đây để thêm gốc", "Drop a folder here to add a root"},
    {"toast.notFolder", "Kéo một thư mục (không phải file) để thêm gốc",
     "Drop a folder (not a file) to add a root"},
    {"common.cancel", "Hủy", "Cancel"},
    {"common.confirm", "Xác nhận", "Confirm"},
    {"common.close", "Đóng", "Close"},
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
    std::string dir(assetsDir);
    if (!dir.empty()) {
#ifdef _WIN32
        constexpr char sep = '\\';
#else
        constexpr char sep = '/';
#endif
        loadTable(dir + sep + "strings_vi.json", g_vi);
        loadTable(dir + sep + "strings_en.json", g_en);
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
