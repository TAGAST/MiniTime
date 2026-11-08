#include "config.h"

#include <cstdio>

namespace {

constexpr const wchar_t* kSectionStyle    = L"style";
constexpr const wchar_t* kSectionPosition = L"position";
constexpr const wchar_t* kSectionHotkey   = L"hotkey";
constexpr const wchar_t* kSectionFormat   = L"format";

// 游戏配置段前缀，例如 [profile:GenshinImpact.exe]
constexpr const wchar_t* kProfilePrefix   = L"profile:";
constexpr size_t         kProfilePrefixLen = 8;   // L"profile:" 的长度

unsigned g_profileVersion = 0;

// 基准样式：缩放以此为准
constexpr int kBaseFontSize = 28;
constexpr int kBasePaddingX = 16;
constexpr int kBasePaddingY = 8;

constexpr const wchar_t* kAnchorNames[] = {
    L"top-left",      L"top-center",    L"top-right",
    L"middle-left",   L"center",        L"middle-right",
    L"bottom-left",   L"bottom-center", L"bottom-right",
    L"custom",
};

constexpr Anchor kAnchors[] = {
    Anchor::TopLeft,      Anchor::TopCenter,    Anchor::TopRight,
    Anchor::MiddleLeft,   Anchor::Center,       Anchor::MiddleRight,
    Anchor::BottomLeft,   Anchor::BottomCenter, Anchor::BottomRight,
    Anchor::Custom,
};

std::wstring ExeDirectory() {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    if (length == 0) return std::wstring();

    std::wstring path(buffer, length);
    const size_t slash = path.find_last_of(L"\\");
    if (slash == std::wstring::npos) return std::wstring();
    return path.substr(0, slash);
}

bool IsDirectoryWritable(const std::wstring& dir) {
    if (dir.empty()) return false;

    const std::wstring probe = dir + L"\\.minitime_write_probe";
    HANDLE file = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    CloseHandle(file);
    return true;
}

HotkeySpec MakeHotkey(UINT modifiers, UINT vk) {
    HotkeySpec spec = {};
    spec.modifiers = modifiers;
    spec.virtualKey = vk;
    return spec;
}

int ScaleBy(int base, int percent) {
    if (percent <= 0) percent = 100;
    return (base * percent + 50) / 100;
}

std::wstring AnchorToName(Anchor anchor) {
    for (int i = 0; i < 10; ++i) {
        if (kAnchors[i] == anchor) return kAnchorNames[i];
    }
    return L"top-right";
}

Anchor NameToAnchor(const std::wstring& name) {
    for (int i = 0; i < 10; ++i) {
        if (name == kAnchorNames[i]) return kAnchors[i];
    }
    return Anchor::TopRight;
}

} // namespace

TextStyle AppConfig::ToTextStyle() const {
    TextStyle style = {};
    style.fontSize   = ScaleBy(kBaseFontSize, scalePercent);
    style.fontWeight = fontWeight;
    style.textColor  = textColor;
    style.paddingX   = ScaleBy(kBasePaddingX, scalePercent);
    style.paddingY   = ScaleBy(kBasePaddingY, scalePercent);
    return style;
}

LayoutConfig AppConfig::ToLayoutConfig() const {
    LayoutConfig layout = {};
    layout.anchor  = anchor;
    layout.marginX = marginX;
    layout.marginY = marginY;
    layout.customX = customX;
    layout.customY = customY;
    return layout;
}

namespace config {

std::wstring FilePath() {
    const std::wstring exeDir = ExeDirectory();
    if (IsDirectoryWritable(exeDir)) {
        return exeDir + L"\\config.ini";
    }

    wchar_t appData[MAX_PATH * 2] = {};
    const DWORD length = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH * 2);
    if (length == 0) return std::wstring();

    std::wstring dir(appData, length);
    dir += L"\\MiniTime";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\config.ini";
}

AppConfig Load() {
    AppConfig cfg = {};
    const std::wstring path = FilePath();
    if (path.empty()) return cfg;

    wchar_t buffer[128] = {};

    GetPrivateProfileStringW(kSectionStyle, L"scale", L"100", buffer, 128, path.c_str());
    cfg.scalePercent = _wtoi(buffer);
    if (cfg.scalePercent < 50 || cfg.scalePercent > 300) cfg.scalePercent = 100;

    GetPrivateProfileStringW(kSectionStyle, L"font_weight", L"600", buffer, 128, path.c_str());
    cfg.fontWeight = _wtoi(buffer);
    if (cfg.fontWeight < 100 || cfg.fontWeight > 900) cfg.fontWeight = 600;

    GetPrivateProfileStringW(kSectionStyle, L"text_color", L"#FFFFFF", buffer, 128, path.c_str());
    cfg.textColor = HexToColor(buffer, RGB(255, 255, 255));

    GetPrivateProfileStringW(kSectionPosition, L"anchor", L"top-right", buffer, 128, path.c_str());
    cfg.anchor = NameToAnchor(buffer);

    GetPrivateProfileStringW(kSectionPosition, L"margin_x", L"24", buffer, 128, path.c_str());
    cfg.marginX = _wtoi(buffer);
    if (cfg.marginX < -10000 || cfg.marginX > 10000) cfg.marginX = 24;

    GetPrivateProfileStringW(kSectionPosition, L"margin_y", L"20", buffer, 128, path.c_str());
    cfg.marginY = _wtoi(buffer);
    if (cfg.marginY < -10000 || cfg.marginY > 10000) cfg.marginY = 20;

    GetPrivateProfileStringW(kSectionPosition, L"custom_x", L"0", buffer, 128, path.c_str());
    cfg.customX = _wtoi(buffer);

    GetPrivateProfileStringW(kSectionPosition, L"custom_y", L"0", buffer, 128, path.c_str());
    cfg.customY = _wtoi(buffer);

    // 热键：解析失败时回退默认值，绝不因配置错误导致热键全失效
    wchar_t hotkeyBuffer[64] = {};
    GetPrivateProfileStringW(kSectionHotkey, L"toggle", L"Ctrl+Alt+T",
                             hotkeyBuffer, 64, path.c_str());
    {
        HotkeySpec parsed = hotkey::Parse(hotkeyBuffer);
        if (parsed.IsEmpty() && _wcsicmp(hotkeyBuffer, L"none") != 0) {
            cfg.hotkeyToggle = hotkey::DefaultToggle();
        } else {
            cfg.hotkeyToggle = parsed;
        }
    }

    GetPrivateProfileStringW(kSectionHotkey, L"edit", L"Ctrl+Alt+E",
                             hotkeyBuffer, 64, path.c_str());
    {
        HotkeySpec parsed = hotkey::Parse(hotkeyBuffer);
        if (parsed.IsEmpty() && _wcsicmp(hotkeyBuffer, L"none") != 0) {
            cfg.hotkeyEdit = hotkey::DefaultEdit();
        } else {
            cfg.hotkeyEdit = parsed;
        }
    }

    // 时间格式
    GetPrivateProfileStringW(kSectionFormat, L"hour24", L"1", buffer, 128, path.c_str());
    cfg.timeFormat.use24Hour = _wtoi(buffer) != 0;

    GetPrivateProfileStringW(kSectionFormat, L"show_seconds", L"0", buffer, 128, path.c_str());
    cfg.timeFormat.showSeconds = _wtoi(buffer) != 0;

    GetPrivateProfileStringW(kSectionFormat, L"show_date", L"0", buffer, 128, path.c_str());
    cfg.timeFormat.showDate = _wtoi(buffer) != 0;

    return cfg;
}

bool Save(const AppConfig& cfg) {
    const std::wstring path = FilePath();
    if (path.empty()) return false;

    wchar_t buffer[64] = {};

    swprintf(buffer, 64, L"%d", cfg.scalePercent);
    WritePrivateProfileStringW(kSectionStyle, L"scale", buffer, path.c_str());

    swprintf(buffer, 64, L"%d", cfg.fontWeight);
    WritePrivateProfileStringW(kSectionStyle, L"font_weight", buffer, path.c_str());
    WritePrivateProfileStringW(kSectionStyle, L"text_color",
                               ColorToHex(cfg.textColor).c_str(), path.c_str());

    WritePrivateProfileStringW(kSectionPosition, L"anchor",
                               AnchorToName(cfg.anchor).c_str(), path.c_str());

    swprintf(buffer, 64, L"%d", cfg.marginX);
    WritePrivateProfileStringW(kSectionPosition, L"margin_x", buffer, path.c_str());

    swprintf(buffer, 64, L"%d", cfg.marginY);
    WritePrivateProfileStringW(kSectionPosition, L"margin_y", buffer, path.c_str());

    swprintf(buffer, 64, L"%d", cfg.customX);
    WritePrivateProfileStringW(kSectionPosition, L"custom_x", buffer, path.c_str());

    swprintf(buffer, 64, L"%d", cfg.customY);
    WritePrivateProfileStringW(kSectionPosition, L"custom_y", buffer, path.c_str());

    WritePrivateProfileStringW(kSectionHotkey, L"toggle",
                               cfg.hotkeyToggle.ToString().c_str(), path.c_str());
    WritePrivateProfileStringW(kSectionHotkey, L"edit",
                               cfg.hotkeyEdit.ToString().c_str(), path.c_str());

    wchar_t fmtBuf[8] = {};
    swprintf(fmtBuf, 8, L"%d", cfg.timeFormat.use24Hour ? 1 : 0);
    WritePrivateProfileStringW(kSectionFormat, L"hour24", fmtBuf, path.c_str());

    swprintf(fmtBuf, 8, L"%d", cfg.timeFormat.showSeconds ? 1 : 0);
    WritePrivateProfileStringW(kSectionFormat, L"show_seconds", fmtBuf, path.c_str());

    swprintf(fmtBuf, 8, L"%d", cfg.timeFormat.showDate ? 1 : 0);
    WritePrivateProfileStringW(kSectionFormat, L"show_date", fmtBuf, path.c_str());

    return true;
}

bool ApplyCommand(AppConfig& cfg, MenuCommand command) {
    switch (command) {
    case MenuCommand::Scale75:       cfg.scalePercent = 75;  return true;
    case MenuCommand::Scale100:      cfg.scalePercent = 100; return true;
    case MenuCommand::Scale125:      cfg.scalePercent = 125; return true;
    case MenuCommand::Scale150:      cfg.scalePercent = 150; return true;
    case MenuCommand::Scale175:      cfg.scalePercent = 175; return true;

    case MenuCommand::WeightRegular: cfg.fontWeight = 400; return true;
    case MenuCommand::WeightMedium:  cfg.fontWeight = 600; return true;
    case MenuCommand::WeightBold:    cfg.fontWeight = 700; return true;

    case MenuCommand::ColorWhite:    cfg.textColor = RGB(255, 255, 255); return true;
    case MenuCommand::ColorCream:    cfg.textColor = RGB(245, 240, 230); return true;
    case MenuCommand::ColorYellow:   cfg.textColor = RGB(255, 210,  74); return true;
    case MenuCommand::ColorCyan:     cfg.textColor = RGB( 79, 216, 235); return true;
    case MenuCommand::ColorGreen:    cfg.textColor = RGB(107, 227, 138); return true;

    case MenuCommand::AnchorTopLeft:      cfg.anchor = Anchor::TopLeft;      return true;
    case MenuCommand::AnchorTopCenter:    cfg.anchor = Anchor::TopCenter;    return true;
    case MenuCommand::AnchorTopRight:     cfg.anchor = Anchor::TopRight;     return true;
    case MenuCommand::AnchorMiddleLeft:   cfg.anchor = Anchor::MiddleLeft;   return true;
    case MenuCommand::AnchorCenter:       cfg.anchor = Anchor::Center;       return true;
    case MenuCommand::AnchorMiddleRight:  cfg.anchor = Anchor::MiddleRight;  return true;
    case MenuCommand::AnchorBottomLeft:   cfg.anchor = Anchor::BottomLeft;   return true;
    case MenuCommand::AnchorBottomCenter: cfg.anchor = Anchor::BottomCenter; return true;
    case MenuCommand::AnchorBottomRight:  cfg.anchor = Anchor::BottomRight;  return true;

    // 时间格式
    case MenuCommand::Hour24:
        cfg.timeFormat.use24Hour = true; return true;
    case MenuCommand::Hour12:
        cfg.timeFormat.use24Hour = false; return true;
    case MenuCommand::ToggleSeconds:
        cfg.timeFormat.showSeconds = !cfg.timeFormat.showSeconds; return true;
    case MenuCommand::ToggleDate:
        cfg.timeFormat.showDate = !cfg.timeFormat.showDate; return true;

    // 热键：显示/隐藏
    case MenuCommand::ToggleHK_CtrlAltT:
        cfg.hotkeyToggle = MakeHotkey(MOD_CONTROL | MOD_ALT, 'T'); return true;
    case MenuCommand::ToggleHK_CtrlShiftT:
        cfg.hotkeyToggle = MakeHotkey(MOD_CONTROL | MOD_SHIFT, 'T'); return true;
    case MenuCommand::ToggleHK_AltT:
        cfg.hotkeyToggle = MakeHotkey(MOD_ALT, 'T'); return true;
    case MenuCommand::ToggleHK_F9:
        cfg.hotkeyToggle = MakeHotkey(0, VK_F9); return true;
    case MenuCommand::ToggleHK_F10:
        cfg.hotkeyToggle = MakeHotkey(0, VK_F10); return true;
    case MenuCommand::ToggleHK_None1:
        cfg.hotkeyToggle = HotkeySpec{}; return true;

    // 热键：调整位置
    case MenuCommand::EditHK_CtrlAltE:
        cfg.hotkeyEdit = MakeHotkey(MOD_CONTROL | MOD_ALT, 'E'); return true;
    case MenuCommand::EditHK_CtrlShiftE:
        cfg.hotkeyEdit = MakeHotkey(MOD_CONTROL | MOD_SHIFT, 'E'); return true;
    case MenuCommand::EditHK_AltE:
        cfg.hotkeyEdit = MakeHotkey(MOD_ALT, 'E'); return true;
    case MenuCommand::EditHK_F11:
        cfg.hotkeyEdit = MakeHotkey(0, VK_F11); return true;
    case MenuCommand::EditHK_F12:
        cfg.hotkeyEdit = MakeHotkey(0, VK_F12); return true;
    case MenuCommand::EditHK_None2:
        cfg.hotkeyEdit = HotkeySpec{}; return true;

    default:
        return false;
    }
}

std::vector<GameProfile> LoadProfiles() {
    std::vector<GameProfile> result;

    const std::wstring path = FilePath();
    if (path.empty()) return result;

    // 枚举所有段名，挑出 profile: 前缀的
    wchar_t sections[8192] = {};
    const DWORD written = GetPrivateProfileSectionNamesW(sections, 8192, path.c_str());
    if (written == 0) return result;

    for (const wchar_t* p = sections; *p; p += lstrlenW(p) + 1) {
        const std::wstring section(p);
        if (section.size() <= kProfilePrefixLen) continue;
        if (section.compare(0, kProfilePrefixLen, kProfilePrefix) != 0) continue;

        GameProfile profile;
        profile.processName = section.substr(kProfilePrefixLen);
        profile.displayName = gameprofile::ToDisplayName(profile.processName);

        wchar_t buffer[256] = {};

        // 默认值必须用独立缓冲：直接传 displayName.c_str() 会让 API
        // 向同一块内存写入，取到空值。
        const std::wstring fallbackName = profile.displayName;
        GetPrivateProfileStringW(section.c_str(), L"name", fallbackName.c_str(),
                                 buffer, 256, path.c_str());
        if (buffer[0] != L'\0') {
            profile.displayName = buffer;
        }

        GetPrivateProfileStringW(section.c_str(), L"anchor", L"custom", buffer, 256, path.c_str());
        profile.anchor = NameToAnchor(buffer);

        GetPrivateProfileStringW(section.c_str(), L"custom_x", L"0", buffer, 256, path.c_str());
        profile.customX = _wtoi(buffer);

        GetPrivateProfileStringW(section.c_str(), L"custom_y", L"0", buffer, 256, path.c_str());
        profile.customY = _wtoi(buffer);

        GetPrivateProfileStringW(section.c_str(), L"margin_x", L"24", buffer, 256, path.c_str());
        profile.marginX = _wtoi(buffer);

        GetPrivateProfileStringW(section.c_str(), L"margin_y", L"20", buffer, 256, path.c_str());
        profile.marginY = _wtoi(buffer);

        if (!profile.processName.empty()) {
            result.push_back(profile);
        }
    }

    return result;
}

bool SaveProfiles(const std::vector<GameProfile>& profiles) {
    const std::wstring path = FilePath();
    if (path.empty()) return false;

    ++g_profileVersion;

    // 先删除旧的 profile 段，避免已删除的游戏残留
    wchar_t sections[8192] = {};
    const DWORD written = GetPrivateProfileSectionNamesW(sections, 8192, path.c_str());
    if (written > 0) {
        std::vector<std::wstring> toRemove;
        for (const wchar_t* p = sections; *p; p += lstrlenW(p) + 1) {
            const std::wstring section(p);
            if (section.size() > kProfilePrefixLen &&
                section.compare(0, kProfilePrefixLen, kProfilePrefix) == 0) {
                toRemove.push_back(section);
            }
        }
        for (const auto& section : toRemove) {
            WritePrivateProfileStringW(section.c_str(), nullptr, nullptr, path.c_str());
        }
    }

    for (const auto& profile : profiles) {
        if (profile.processName.empty()) continue;

        const std::wstring section = std::wstring(kProfilePrefix) + profile.processName;

        WritePrivateProfileStringW(section.c_str(), L"name",
                                   profile.displayName.c_str(), path.c_str());

        WritePrivateProfileStringW(section.c_str(), L"anchor",
                                   AnchorToName(profile.anchor).c_str(), path.c_str());

        wchar_t buffer[64] = {};
        swprintf(buffer, 64, L"%d", profile.customX);
        WritePrivateProfileStringW(section.c_str(), L"custom_x", buffer, path.c_str());

        swprintf(buffer, 64, L"%d", profile.customY);
        WritePrivateProfileStringW(section.c_str(), L"custom_y", buffer, path.c_str());

        swprintf(buffer, 64, L"%d", profile.marginX);
        WritePrivateProfileStringW(section.c_str(), L"margin_x", buffer, path.c_str());

        swprintf(buffer, 64, L"%d", profile.marginY);
        WritePrivateProfileStringW(section.c_str(), L"margin_y", buffer, path.c_str());
    }

    return true;
}

unsigned ProfileVersion() {
    return g_profileVersion;
}

std::wstring ColorToHex(COLORREF color) {
    wchar_t buffer[16] = {};
    swprintf(buffer, 16, L"#%02X%02X%02X",
             GetRValue(color), GetGValue(color), GetBValue(color));
    return std::wstring(buffer);
}

COLORREF HexToColor(const std::wstring& text, COLORREF fallback) {
    if (text.size() != 7 || text[0] != L'#') return fallback;

    unsigned int r = 0, g = 0, b = 0;
    if (swscanf_s(text.c_str() + 1, L"%2x%2x%2x", &r, &g, &b) != 3) return fallback;
    if (r > 255 || g > 255 || b > 255) return fallback;

    return RGB(r, g, b);
}

} // namespace config