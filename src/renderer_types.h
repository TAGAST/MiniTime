#pragma once

#include "menu_commands.h"

#include <windows.h>

// 文字样式：渲染层与配置层之间的值对象
struct TextStyle {
    const wchar_t* fontFace = L"Segoe UI";
    int            fontSize = 28;   // 像素高度
    int            fontWeight = 600;
    COLORREF       textColor = RGB(255, 255, 255);
    int            paddingX = 16;
    int            paddingY = 8;
};

// 叠加层布局：九宫格锚点 + 偏移；anchor == Custom 时使用绝对坐标
struct LayoutConfig {
    Anchor anchor  = Anchor::TopRight;
    int    marginX = 24;
    int    marginY = 20;

    // 仅当 anchor == Custom 时有效
    int    customX = 0;
    int    customY = 0;
};