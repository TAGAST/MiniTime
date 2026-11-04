#include "tray_menu.h"

namespace {

void AppendChecked(HMENU menu, UINT id, const wchar_t* text, bool checked) {
    AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, text);
}

} // namespace

TrayMenu::~TrayMenu() = default;

MenuCommand TrayMenu::Show(HWND owner, POINT screenPos, const TrayMenuState& state) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return MenuCommand::None;

    const wchar_t* statusText = state.visible ? L"MiniTime · 显示中" : L"MiniTime · 已隐藏";
    AppendMenuW(menu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, statusText);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    const wchar_t* toggleText = state.visible ? L"隐藏        Ctrl+Alt+T" : L"显示        Ctrl+Alt+T";
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(MenuCommand::ToggleShow), toggleText);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // ---- 样式子菜单 ----
    HMENU styleMenu = CreatePopupMenu();

    HMENU scaleMenu = CreatePopupMenu();
    struct ScaleItem { MenuCommand cmd; const wchar_t* label; int value; };
    const ScaleItem scales[] = {
        { MenuCommand::Scale75,  L"75%",  75  },
        { MenuCommand::Scale100, L"100%", 100 },
        { MenuCommand::Scale125, L"125%", 125 },
        { MenuCommand::Scale150, L"150%", 150 },
        { MenuCommand::Scale175, L"175%", 175 },
    };
    for (const auto& item : scales) {
        AppendChecked(scaleMenu, static_cast<UINT>(item.cmd), item.label,
                      state.config.scalePercent == item.value);
    }
    AppendMenuW(styleMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(scaleMenu), L"大小");

    HMENU weightMenu = CreatePopupMenu();
    AppendChecked(weightMenu, static_cast<UINT>(MenuCommand::WeightRegular), L"常规", state.config.fontWeight == 400);
    AppendChecked(weightMenu, static_cast<UINT>(MenuCommand::WeightMedium),  L"中等", state.config.fontWeight == 600);
    AppendChecked(weightMenu, static_cast<UINT>(MenuCommand::WeightBold),    L"粗体", state.config.fontWeight == 700);
    AppendMenuW(styleMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(weightMenu), L"字重");

    HMENU colorMenu = CreatePopupMenu();
    struct ColorItem { MenuCommand cmd; const wchar_t* label; COLORREF value; };
    const ColorItem colors[] = {
        { MenuCommand::ColorWhite,  L"白",   RGB(255, 255, 255) },
        { MenuCommand::ColorCream,  L"米白", RGB(245, 240, 230) },
        { MenuCommand::ColorYellow, L"亮黄", RGB(255, 210,  74) },
        { MenuCommand::ColorCyan,   L"青",   RGB( 79, 216, 235) },
        { MenuCommand::ColorGreen,  L"绿",   RGB(107, 227, 138) },
    };
    for (const auto& item : colors) {
        AppendChecked(colorMenu, static_cast<UINT>(item.cmd), item.label,
                      state.config.textColor == item.value);
    }
    AppendMenuW(styleMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(colorMenu), L"颜色");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(styleMenu), L"样式");

    // ---- 位置子菜单（九宫格）----
    HMENU positionMenu = CreatePopupMenu();
    struct AnchorItem { MenuCommand cmd; const wchar_t* label; Anchor value; };
    const AnchorItem anchors[] = {
        { MenuCommand::AnchorTopLeft,      L"上左",  Anchor::TopLeft },
        { MenuCommand::AnchorTopCenter,    L"上中",  Anchor::TopCenter },
        { MenuCommand::AnchorTopRight,     L"上右",  Anchor::TopRight },
        { MenuCommand::AnchorMiddleLeft,   L"中左",  Anchor::MiddleLeft },
        { MenuCommand::AnchorCenter,       L"居中",  Anchor::Center },
        { MenuCommand::AnchorMiddleRight,  L"中右",  Anchor::MiddleRight },
        { MenuCommand::AnchorBottomLeft,   L"下左",  Anchor::BottomLeft },
        { MenuCommand::AnchorBottomCenter, L"下中",  Anchor::BottomCenter },
        { MenuCommand::AnchorBottomRight,  L"下右",  Anchor::BottomRight },
    };
    for (const auto& item : anchors) {
        AppendChecked(positionMenu, static_cast<UINT>(item.cmd), item.label,
                      state.config.anchor == item.value);
    }
    AppendMenuW(positionMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(positionMenu, MF_STRING,
                static_cast<UINT>(MenuCommand::EnterEditMode), L"调整位置…");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(positionMenu), L"位置");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendChecked(menu, static_cast<UINT>(MenuCommand::Autostart), L"开机自启", state.autostart);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(MenuCommand::Exit), L"退出");

    // 经典坑：不先抢前台，点击菜单外部时菜单不会消失
    SetForegroundWindow(owner);

    const UINT flags = TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY;
    const UINT chosen = static_cast<UINT>(
        TrackPopupMenuEx(menu, flags, screenPos.x, screenPos.y, owner, nullptr));

    DestroyMenu(menu);

    // 同样重要：补一条空消息，确保菜单能正确关闭
    PostMessageW(owner, WM_NULL, 0, 0);

    return static_cast<MenuCommand>(chosen);
}