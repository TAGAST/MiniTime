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

    // 编辑模式期间窗口位置由拖动逻辑接管：
    // UpdateLayeredWindow 会把窗口移动到传入的坐标，因此必须使用真实位置，
    // 否则每一帧都会把窗口拉回配置里记录的旧坐标。
    int presentX = x;
    int presentY = y;
    if (suspendLayout_) {
        RECT actual = {};
        if (GetWindowRect(hwnd, &actual)) {
            presentX = actual.left;
            presentY = actual.top;
        }
    }

    surface_.Present(hwnd, presentX, presentY);
    return true;
}

void ClockPresenter::EraseRegion(HWND hwnd, int x, int y, int width, int height) {
    if (width <= 0 || height <= 0) return;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return;

    BITMAPINFO info = {};
    info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth       = width;
    info.bmiHeader.biHeight      = -height;
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap || !bits) {
        ReleaseDC(nullptr, screenDc);
        return;
    }

    HDC memDc = CreateCompatibleDC(nullptr);
    if (memDc) {
        HGDIOBJ old = SelectObject(memDc, bitmap);
        ZeroMemory(bits, static_cast<size_t>(width) * static_cast<size_t>(height) * 4);

        POINT dest = { x, y };
        POINT src  = { 0, 0 };
        SIZE  size = { width, height };
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 0, AC_SRC_ALPHA };
        UpdateLayeredWindow(hwnd, screenDc, &dest, &size, memDc, &src, 0, &blend, ULW_ALPHA);

        SelectObject(memDc, old);
        DeleteDC(memDc);
    }

    DeleteObject(bitmap);
    ReleaseDC(nullptr, screenDc);
}

void ClockPresenter::InvalidateGeometry() {
    hasGeometry_ = false;
}

void ClockPresenter::SyncWindowGeometry(HWND hwnd, int x, int y, int width, int height) {
    // 编辑模式期间由 DragEditor 接管窗口位置，渲染层不得覆写
    if (suspendLayout_) return;

    RECT current = {};
    if (!GetWindowRect(hwnd, &current)) return;

    const bool unchanged = hasGeometry_
        && current.left   == x
        && current.top    == y
        && (current.right - current.left) == width
        && (current.bottom - current.top) == height;

    if (unchanged) return;

    // 分层窗口缩小后，DWM 不会擦除原先占据的屏幕区域，旧文字会残留在桌面上。
    // 注意：这里不能复用 surface_ —— 调用方刚在其中绘制好新内容，
    // 改动它的尺寸或清空会把这一帧毁掉。改用一次性的临时表面擦除旧区域。

    if (hasGeometry_) {
        EraseRegion(hwnd, current.left, current.top,
                    current.right - current.left, current.bottom - current.top);
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

    // 自由拖动产生的绝对坐标
    if (layout_.anchor == Anchor::Custom) {
        x = layout_.customX;
        y = layout_.customY;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > screenWidth - width)   x = screenWidth - width;
        if (y > screenHeight - height) y = screenHeight - height;
        return;
    }

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