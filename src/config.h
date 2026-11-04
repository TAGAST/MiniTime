#pragma once

#include "menu_commands.h"
#include "renderer_types.h"

#include <windows.h>
#include <string>

// 应用配置（值对象）
struct AppConfig {
    // 样式
    int      scalePercent = 100;      // 整体缩放：75/100/125/150/175
    int      fontWeight   = 600;      // 400/600/700
    COLORREF textColor    = RGB(255, 255, 255);

    // 位置
    Anchor anchor  = Anchor::TopRight;
    int    marginX = 24;
    int    marginY = 20;

    // 转换为渲染参数（应用整体缩放）
    TextStyle    ToTextStyle() const;
    LayoutConfig ToLayoutConfig() const;
};

// 配置持久化与应用
namespace config {

// 配置文件路径（便携优先，回退到 %APPDATA%）
std::wstring FilePath();

// 读取；文件不存在或损坏时返回默认配置
AppConfig Load();

// 保存；返回是否成功
bool Save(const AppConfig& config);

// 把菜单命令应用到配置；返回 true 表示配置已被修改
bool ApplyCommand(AppConfig& config, MenuCommand command);

// 颜色与字符串互转（供配置文件使用）
std::wstring ColorToHex(COLORREF color);
COLORREF     HexToColor(const std::wstring& text, COLORREF fallback);

} // namespace config
