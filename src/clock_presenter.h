#pragma once

#include "gdi_text_painter.h"
#include "layered_surface.h"
#include "renderer_types.h"
#include "timefmt.h"

#include <windows.h>
#include <string>

// 编排层：取时间 -> 格式化 -> 测量 -> 绘制 -> 提交。
// 不直接操作 HWND 样式，不知道窗口如何创建。
class ClockPresenter {
public:
    ClockPresenter() = default;

    void SetFormat(const TimeFormat& format) { format_ = format; }
    void SetStyle(const TextStyle& style);
    void SetLayout(const LayoutConfig& layout) { layout_ = layout; }

    const TimeFormat& Format() const { return format_; }

    // 计算并提交一帧；返回 false 表示失败
    bool Render(HWND hwnd);

private:
    void ComputePosition(int width, int height, int& x, int& y) const;

    // 同步窗口几何：分层窗口仅靠 UpdateLayeredWindow 改变尺寸时，
    // DWM 不会擦除旧区域，必须显式 SetWindowPos 通知几何变化。
    void SyncWindowGeometry(HWND hwnd, int x, int y, int width, int height);

    RECT lastGeometry_ = { 0, 0, 0, 0 };
    bool hasGeometry_  = false;

    TimeFormat      format_  = {};
    TextStyle       style_   = {};
    LayoutConfig    layout_  = {};
    GdiTextPainter  painter_ = {};
    LayeredSurface  surface_ = {};
};