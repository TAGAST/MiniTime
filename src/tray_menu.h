#pragma once

#include "config.h"
#include "game_profile.h"
#include "hotkey_key.h"

#include <vector>
#include "menu_commands.h"

#include <windows.h>

// 菜单所需的状态快照（值对象）
struct TrayMenuState {
    bool      visible   = true;
    bool      autostart = false;
    AppConfig config    = {};

    // 当前热键是否因被其它程序占用而注册失败（菜单据此置灰提示）
    bool toggleConflicted = false;
    bool editConflicted   = false;

    // 游戏位置配置
    std::vector<GameProfile> profiles;
    ForegroundInfo foreground;                  // 当前前台
    std::wstring   activeProfileProcess;        // 当前生效的游戏配置（空=无）
};

// 只负责构建菜单并返回用户选择的命令。
// 不执行任何业务动作，不碰窗口、注册表与配置文件。
class TrayMenu {
public:
    TrayMenu() = default;
    ~TrayMenu();

    TrayMenu(const TrayMenu&) = delete;
    TrayMenu& operator=(const TrayMenu&) = delete;

    MenuCommand Show(HWND owner, POINT screenPos, const TrayMenuState& state);
};