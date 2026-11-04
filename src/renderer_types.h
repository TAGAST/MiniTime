#pragma once

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

// 叠加层锚点布局
struct LayoutConfig {
    int marginRight = 24;
    int marginTop   = 20;
};
