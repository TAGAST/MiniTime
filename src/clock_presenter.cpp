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

    // 编辑模式留出更大边距，便于抓住
    const int editPad = editAppearance_ ? 10 : 0;

    int width  = textSize.cx + painter_.PaddingX() * 2 + editPad * 2;
    int height = textSize.cy + painter_.PaddingY() * 2 + editPad * 2;

    // 提示行尺寸（编辑模式才显示）
    constexpr int kHintPixelSize = 13;   // 请求字号
    constexpr int kHintGap       = 4;
    int hintWidth  = 0;
    int hintHeight = 0;
    bool showHint  = false;

    if (editAppearance_ && editHint_ && editHint_[0]) {
        // 必须在与绘制表面同类的内存 DC 上测量：
        // 屏幕 DC 与 DIB 内存 DC 在非 100% DPI 下取到的字体度量不一致，
        // 用屏幕 DC 测量会导致提示文字超出底板。
        HDC measureDc = CreateCompatibleDC(nullptr);
        if (measureDc) {
            const SIZE hintSize = painter_.MeasureHint(measureDc, editHint_, kHintPixelSize);
            hintWidth  = hintSize.cx;
            hintHeight = hintSize.cy > 0 ? hintSize.cy : kHintPixelSize;
            DeleteDC(measureDc);
        }
        if (hintWidth > 0 && hintHeight > 0) {
            showHint = true;
            height += kHintGap + hintHeight;
        }
    }

    // 底板必须同时容纳主文字与提示行，两侧各留 editPad 内边距。
    // 用取两者较大值的方式，避免任一被裁切。
    if (editAppearance_) {
        const int need = (showHint ? hintWidth : 0) + editPad * 2;
        if (need > width) width = need;
    }

    if (!surface_.EnsureSize(width, height)) return false;

    // 第二遍校准：在真实表面 DC 上复测提示宽度，防止 DPI/字体回退造成的偏差
    if (showHint) {
        const SIZE actual = painter_.MeasureHint(surface_.Dc(), editHint_, kHintPixelSize);
        if (actual.cx > hintWidth) hintWidth = actual.cx;
        if (actual.cy > hintHeight) {
            height += actual.cy - hintHeight;
            hintHeight = actual.cy;
        }

        const int need = hintWidth + editPad * 2;
        if (need > width) width = need;

        if (!surface_.EnsureSize(width, height)) return false;
    }

    lastWidth_       = width;
    lastHeight_      = height;
    mainTextWidth_   = textSize.cx;
    mainTextHeight_  = textSize.cy;

    surface_.Clear();

    // 主文字：编辑模式下水平居中，避免与提示行错位
    const int mainX = editAppearance_
        ? (width - textSize.cx) / 2
        : painter_.PaddingX();
    const int mainY = painter_.PaddingY() + editPad;

    painter_.Draw(surface_.Dc(), text.c_str(), mainX, mainY);

    // 提示行同样先以白色绘制在黑底上
    if (showHint) {
        // 居中；若空间不足则从内边距处左对齐，宁可贴边也不溢出底板
        const int avail = width - editPad * 2;
        const int hintX = (hintWidth <= avail)
            ? (width - hintWidth) / 2
            : editPad;
        const int hintY = height - editPad - hintHeight;
        painter_.DrawHint(surface_.Dc(), editHint_, hintX, hintY,
                          kHintPixelSize, RGB(255, 255, 255));
    }

    // 此时所有文字都以"白色 = 覆盖率"的形式存在，统一推一次 alpha。
    // 必须放在合成底板之前，否则会把底板与边框的 alpha 覆盖掉。
    surface_.ApplyAlphaFromLuminance();

    if (editAppearance_) {
        // 半透明底板：让整块区域都能接收鼠标（分层窗口按 alpha 做命中测试）
        surface_.CompositeOverBackground(RGB(24, 28, 36), 170);

        // 1px 边框，明确拖动区域
        surface_.DrawBorder(RGB(120, 200, 255), 220);
    }

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

POINT ClockPresenter::MainTextOffset() const {
    // 与 Render 中的绘制坐标保持一致
    const int editPad = editAppearance_ ? 10 : 0;

    POINT offset = { 0, 0 };

    if (editAppearance_) {
        // 编辑模式主文字水平居中
        offset.x = (lastWidth_ - mainTextWidth_) / 2;
        offset.y = painter_.PaddingY() + editPad;
    } else {
        offset.x = painter_.PaddingX();
        offset.y = painter_.PaddingY();
    }

    if (offset.x < 0) offset.x = 0;
    if (offset.y < 0) offset.y = 0;
    return offset;
}

SIZE ClockPresenter::MeasureNormalSize() const {
    SIZE result = { lastWidth_, lastHeight_ };

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    const std::wstring text = timefmt::Format(st, format_);
    if (text.empty()) return result;

    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return result;

    const SIZE textSize = painter_.Measure(dc, text.c_str());
    DeleteDC(dc);

    if (textSize.cx > 0 && textSize.cy > 0) {
        result.cx = textSize.cx + painter_.PaddingX() * 2;
        result.cy = textSize.cy + painter_.PaddingY() * 2;
    }

    return result;
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

    // 分层窗口缩小后，DWM 不会擦除原先占据的屏幕区域，旧内容会残留在桌面上。
    // 注意：这里不能复用 surface_ —— 调用方刚在其中绘制好新内容，
    // 改动它的尺寸或清空会把这一帧毁掉。改用一次性的临时表面擦除旧区域。
    //
    // 必须无条件擦除：调用方可能刚调用过 InvalidateGeometry()（例如退出编辑模式），
    // 此时 hasGeometry_ 已是 false，若以此为条件会跳过擦除，
    // 导致编辑模式的底板在窗口缩小后残留在屏幕上。

    EraseRegion(hwnd, current.left, current.top,
                current.right - current.left, current.bottom - current.top);

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