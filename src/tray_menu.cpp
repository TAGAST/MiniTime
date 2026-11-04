#include "tray_menu.h"

TrayMenu::~TrayMenu() = default;

TrayCommand TrayMenu::Show(HWND owner, POINT screenPos, const TrayMenuState& state) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return TrayCommand::None;

    // 状态行：灰色不可点
    const wchar_t* statusText = state.visible ? L"MiniTime · 显示中" : L"MiniTime · 已隐藏";
    AppendMenuW(menu, MF_STRING | MF_GRAYED | MF_DISABLED, 0, statusText);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    const wchar_t* toggleText = state.visible ? L"隐藏        Ctrl+Alt+T" : L"显示        Ctrl+Alt+T";
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::ToggleShow), toggleText);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    UINT autostartFlags = MF_STRING;
    if (state.autostart) autostartFlags |= MF_CHECKED;
    AppendMenuW(menu, autostartFlags, static_cast<UINT>(TrayCommand::Autostart), L"开机自启");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayCommand::Exit), L"退出");

    // 经典坑：不先抢前台，点击菜单外部时菜单不会消失
    SetForegroundWindow(owner);

    const UINT flags = TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY;
    const UINT chosen = static_cast<UINT>(
        TrackPopupMenuEx(menu, flags, screenPos.x, screenPos.y, owner, nullptr));

    DestroyMenu(menu);

    // 同样重要：补一条空消息，确保菜单能正确关闭
    PostMessageW(owner, WM_NULL, 0, 0);

    return static_cast<TrayCommand>(chosen);
}
