#include "clock_presenter.h"

void ClockPresenter::SetStyle(const TextStyle& style) {
    style_ = style;
    painter_.SetStyle(style);
}

bool ClockPresenter::Render(HWND hwnd) {
    if (!hwnd) return false;

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    const std::wstring text = timefmt::Format(st, format_);
    if (text.empty()) return false;

    // 用屏幕 DC 测量文字尺寸
    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return false;
    const SIZE textSize = painter_.Measure(screenDc, text.c_str());
    ReleaseDC(nullptr, screenDc);

    const int width  = textSize.cx + painter_.PaddingX() * 2;
    const int height = textSize.cy + painter_.PaddingY() * 2;

    if (!surface_.EnsureSize(width, height)) return false;

    surface_.Clear();
    painter_.Draw(surface_.Dc(), text.c_str(), painter_.PaddingX(), painter_.PaddingY());
    surface_.ApplyAlphaFromLuminance();

    int x = 0;
    int y = 0;
    ComputePosition(width, height, x, y);

    // 关键：先同步窗口几何。若只用 UpdateLayeredWindow 改变尺寸，
    // DWM 不会重绘旧区域，屏幕上会残留旧的文字像素。
    SyncWindowGeometry(hwnd, x, y, width, height);

    surface_.Present(hwnd, x, y);
    return true;
}

void ClockPresenter::SyncWindowGeometry(HWND hwnd, int x, int y, int width, int height) {
    RECT current = {};
    if (!GetWindowRect(hwnd, &current)) return;

    const bool unchanged = hasGeometry_
        && current.left   == x
        && current.top    == y
        && (current.right - current.left) == width
        && (current.bottom - current.top) == height;

    if (unchanged) return;

    // 分层窗口缩小后，DWM 不会擦除原先占据的屏幕区域，旧文字会残留在桌面上。
    // 处理顺序：先把旧区域作废 -> 调整几何 -> 再用透明内容覆盖旧区域。

    if (hasGeometry_) {
        // 用空内容把旧窗口区域刷成完全透明，DWM 才会真正擦除那些像素
        if (surface_.EnsureSize(current.right - current.left, current.bottom - current.top)) {
            surface_.Clear();
            surface_.ApplyAlphaFromLuminance();
            surface_.Present(hwnd, current.left, current.top);
        }
    }

    // SWP_NOACTIVATE：绝不抢游戏焦点
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    lastGeometry_ = RECT{ x, y, x + width, y + height };
    hasGeometry_  = true;
}

void ClockPresenter::ComputePosition(int width, int height, int& x, int& y) const {
    const int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 水平方向
    switch (layout_.anchor) {
    case Anchor::TopLeft:
    case Anchor::MiddleLeft:
    case Anchor::BottomLeft:
        x = layout_.marginX;
        break;

    case Anchor::TopCenter:
    case Anchor::Center:
    case Anchor::BottomCenter:
        x = (screenWidth - width) / 2;
        break;

    default: // 右侧三格
        x = screenWidth - width - layout_.marginX;
        break;
    }

    // 垂直方向
    switch (layout_.anchor) {
    case Anchor::TopLeft:
    case Anchor::TopCenter:
    case Anchor::TopRight:
        y = layout_.marginY;
        break;

    case Anchor::MiddleLeft:
    case Anchor::Center:
    case Anchor::MiddleRight:
        y = (screenHeight - height) / 2;
        break;

    default: // 底部三格
        y = screenHeight - height - layout_.marginY;
        break;
    }

    // 防止跑到屏幕外
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > screenWidth - width)   x = screenWidth - width;
    if (y > screenHeight - height) y = screenHeight - height;
}