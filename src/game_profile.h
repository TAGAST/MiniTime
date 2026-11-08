#pragma once

#include "menu_commands.h"

#include <windows.h>
#include <string>
#include <vector>

// 单个游戏的位置配置。
// 以进程名识别游戏，命中时套用该位置。
struct GameProfile {
    std::wstring processName;   // 例如 L"GenshinImpact.exe"，比较时不区分大小写
    std::wstring displayName;   // 菜单显示用（默认取进程名去掉扩展名）

    Anchor anchor  = Anchor::Custom;
    int    customX = 0;
    int    customY = 0;

    int    marginX = 24;
    int    marginY = 20;
};

// 前台窗口信息：识别游戏所需的最小信息集。
struct ForegroundInfo {
    std::wstring processName;
    std::wstring windowTitle;
    bool valid = false;
};

namespace gameprofile {

// 读取当前前台窗口所属进程（用于"保存当前游戏位置"）
ForegroundInfo QueryForeground();

// 进程名规范化：取文件名、去掉扩展名作为显示名，并统一小写用于比较
std::wstring NormalizeProcessName(const std::wstring& raw);
std::wstring ToDisplayName(const std::wstring& processName);

// 不区分大小写比较
bool SameProcess(const std::wstring& a, const std::wstring& b);

} // namespace gameprofile
