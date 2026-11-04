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

    surface_.Present(hwnd, x, y);
    return true;
}

void ClockPresenter::ComputePosition(int width, int height, int& x, int& y) const {
    const int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    x = screenWidth - width - layout_.marginRight;
    y = layout_.marginTop;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (y > screenHeight - height) y = screenHeight - height;
}
