#include "tray_menu.h"

#include "hotkey.h"

namespace {

void AppendChecked(HMENU menu, UINT id, const wchar_t* text, bool checked) {
    AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, text);
}

HotkeySpec MakeHK(UINT modifiers, UINT vk) {
    HotkeySpec spec = {};
    spec.modifiers = modifiers;
    spec.virtualKey = vk;
    return spec;
}

// 热键菜单项：被占用的组合置灰，并在文字后标注"已被占用"
void AppendHotkeyItem(HMENU menu, UINT id, const wchar_t* text,
                      bool checked, bool occupied) {
    UINT flags = MF_STRING;
    if (checked) flags |= MF_CHECKED;
    if (occupied) flags |= MF_GRAYED;

    if (occupied) {
        wchar_t label[128] = {};
        swprintf(label, 128, L"%ls  (已被占用)", text);
        AppendMenuW(menu, flags, id, label);
    } else {
        AppendMenuW(menu, flags, id, text);
    }
}

bool SameHotkey(const HotkeySpec& a, const HotkeySpec& b) {
    if (a.IsEmpty() || b.IsEmpty()) return a.IsEmpty() && b.IsEmpty();
    return a.modifiers == b.modifiers && a.virtualKey == b.virtualKey;
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

    // 时间格式
    HMENU timeMenu = CreatePopupMenu();
    AppendChecked(timeMenu, static_cast<UINT>(MenuCommand::Hour24), L"24 小时制",
                  state.config.timeFormat.use24Hour);
    AppendChecked(timeMenu, static_cast<UINT>(MenuCommand::Hour12), L"12 小时制 (AM/PM)",
                  !state.config.timeFormat.use24Hour);
    AppendMenuW(timeMenu, MF_SEPARATOR, 0, nullptr);
    AppendChecked(timeMenu, static_cast<UINT>(MenuCommand::ToggleSeconds), L"显示秒",
                  state.config.timeFormat.showSeconds);
    AppendChecked(timeMenu, static_cast<UINT>(MenuCommand::ToggleDate), L"显示日期",
                  state.config.timeFormat.showDate);
    AppendMenuW(styleMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(timeMenu), L"时间");

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

    // ---- 热键子菜单 ----
    HMENU hotkeyMenu = CreatePopupMenu();

    HMENU toggleMenu = CreatePopupMenu();
    struct HKItem { MenuCommand cmd; const wchar_t* label; HotkeySpec spec; };
    const HKItem toggleKeys[] = {
        { MenuCommand::ToggleHK_CtrlAltT,   L"Ctrl+Alt+T",   MakeHK(MOD_CONTROL | MOD_ALT, 'T') },
        { MenuCommand::ToggleHK_CtrlShiftT, L"Ctrl+Shift+T", MakeHK(MOD_CONTROL | MOD_SHIFT, 'T') },
        { MenuCommand::ToggleHK_AltT,       L"Alt+T",        MakeHK(MOD_ALT, 'T') },
        { MenuCommand::ToggleHK_F9,         L"F9",           MakeHK(0, VK_F9) },
        { MenuCommand::ToggleHK_F10,        L"F10",          MakeHK(0, VK_F10) },
    };
    for (const auto& item : toggleKeys) {
        const bool checked = SameHotkey(state.config.hotkeyToggle, item.spec);
        // 当前生效项由本程序自己注册，探测必然失败，不能判为"被占用"
        const bool occupied = !checked &&
            (HotkeyRegistry::IsAvailable(item.spec) == FALSE) &&
            !state.toggleConflicted;
        AppendHotkeyItem(toggleMenu, static_cast<UINT>(item.cmd), item.label,
                         checked, occupied);
    }
    AppendMenuW(toggleMenu, MF_SEPARATOR, 0, nullptr);
    AppendChecked(toggleMenu, static_cast<UINT>(MenuCommand::ToggleHK_None1), L"禁用",
                  state.config.hotkeyToggle.IsEmpty());

    AppendMenuW(hotkeyMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(toggleMenu), L"显示 / 隐藏");

    HMENU editMenu = CreatePopupMenu();
    const HKItem editKeys[] = {
        { MenuCommand::EditHK_CtrlAltE,   L"Ctrl+Alt+E",   MakeHK(MOD_CONTROL | MOD_ALT, 'E') },
        { MenuCommand::EditHK_CtrlShiftE, L"Ctrl+Shift+E", MakeHK(MOD_CONTROL | MOD_SHIFT, 'E') },
        { MenuCommand::EditHK_AltE,       L"Alt+E",        MakeHK(MOD_ALT, 'E') },
        { MenuCommand::EditHK_F11,        L"F11",          MakeHK(0, VK_F11) },
        { MenuCommand::EditHK_F12,        L"F12",          MakeHK(0, VK_F12) },
    };
    for (const auto& item : editKeys) {
        const bool checked = SameHotkey(state.config.hotkeyEdit, item.spec);
        const bool occupied = !checked &&
            (HotkeyRegistry::IsAvailable(item.spec) == FALSE) &&
            !state.editConflicted;
        AppendHotkeyItem(editMenu, static_cast<UINT>(item.cmd), item.label,
                         checked, occupied);
    }
    AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendChecked(editMenu, static_cast<UINT>(MenuCommand::EditHK_None2), L"禁用",
                  state.config.hotkeyEdit.IsEmpty());

    AppendMenuW(hotkeyMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(editMenu), L"调整位置");

    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hotkeyMenu), L"热键");

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