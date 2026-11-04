#pragma once

#include <windows.h>

// 托盘右键菜单的命令 ID
enum class TrayCommand : int {
    None        = 0,
    ToggleShow  = 1001,
    Autostart   = 1002,
    Exit        = 1003,
};

// 菜单构建所需的状态快照（值对象，菜单层不持有业务状态）
struct TrayMenuState {
    bool visible   = true;
    bool autostart = false;
};

// 只负责构建菜单并返回用户选择的命令。
// 不执行任何业务动作，不碰窗口与注册表。
class TrayMenu {
public:
    TrayMenu() = default;
    ~TrayMenu();

    TrayMenu(const TrayMenu&) = delete;
    TrayMenu& operator=(const TrayMenu&) = delete;

    // 在指定屏幕坐标弹出菜单；返回用户选择的命令
    TrayCommand Show(HWND owner, POINT screenPos, const TrayMenuState& state);
};
