#include "config.h"

#include <cstdio>

namespace {

constexpr const wchar_t* kSectionStyle    = L"style";
constexpr const wchar_t* kSectionPosition = L"position";

// 基准样式：缩放以此为准
constexpr int kBaseFontSize = 28;
constexpr int kBasePaddingX = 16;
constexpr int kBasePaddingY = 8;

constexpr const wchar_t* kAnchorNames[] = {
    L"top-left",      L"top-center",    L"top-right",
    L"middle-left",   L"center",        L"middle-right",
    L"bottom-left",   L"bottom-center", L"bottom-right",
};

constexpr Anchor kAnchors[] = {
    Anchor::TopLeft,      Anchor::TopCenter,    Anchor::TopRight,
    Anchor::MiddleLeft,   Anchor::Center,       Anchor::MiddleRight,
    Anchor::BottomLeft,   Anchor::BottomCenter, Anchor::BottomRight,
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

int ScaleBy(int base, int percent) {
    if (percent <= 0) percent = 100;
    return (base * percent + 50) / 100;
}

std::wstring AnchorToName(Anchor anchor) {
    for (int i = 0; i < 9; ++i) {
        if (kAnchors[i] == anchor) return kAnchorNames[i];
    }
    return L"top-right";
}

Anchor NameToAnchor(const std::wstring& name) {
    for (int i = 0; i < 9; ++i) {
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

    default:
        return false;
    }
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
