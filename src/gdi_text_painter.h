#pragma once

#include "renderer_types.h"

// 负责字体与文字绘制。
// 不知道何时重绘，也不知道窗口位置。
class GdiTextPainter {
public:
    GdiTextPainter() = default;
    ~GdiTextPainter();

    GdiTextPainter(const GdiTextPainter&) = delete;
    GdiTextPainter& operator=(const GdiTextPainter&) = delete;

    void SetStyle(const TextStyle& style);

    // 测量文本像素尺寸
    SIZE Measure(HDC dc, const wchar_t* text) const;

    // 在 dc 的指定位置绘制
    void Draw(HDC dc, const wchar_t* text, int x, int y) const;

    // 以较小字号绘制辅助文字（提示、说明），不受主样式字号影响。
    // 只做测量与绘制，不持有业务状态。
    SIZE MeasureHint(HDC dc, const wchar_t* text, int pixelHeight) const;
    void DrawHint(HDC dc, const wchar_t* text, int x, int y,
                  int pixelHeight, COLORREF color) const;

    int PaddingX() const { return style_.paddingX; }
    int PaddingY() const { return style_.paddingY; }

private:
    void EnsureFont();

    TextStyle style_        = {};
    HFONT     font_         = nullptr;
    bool      fontValid_    = false;
};