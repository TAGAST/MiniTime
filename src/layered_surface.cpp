#include "layered_surface.h"

LayeredSurface::~LayeredSurface() { Release(); }

void LayeredSurface::Release() {
    if (memDc_) { DeleteDC(memDc_); memDc_ = nullptr; }
    if (bitmap_) { DeleteObject(bitmap_); bitmap_ = nullptr; }
    bits_ = nullptr;
    width_ = height_ = 0;
}

bool LayeredSurface::EnsureSize(int width, int height) {
    if (width < 1)  width = 1;
    if (height < 1) height = 1;
    if (bitmap_ && width_ == width && height_ == height) return true;

    Release();

    BITMAPINFO info = {};
    info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth       = width;
    info.bmiHeader.biHeight      = -height;   // 自上而下
    info.bmiHeader.biPlanes      = 1;
    info.bmiHeader.biBitCount    = 32;
    info.bmiHeader.biCompression = BI_RGB;

    bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits_, nullptr, 0);
    if (!bitmap_ || !bits_) { Release(); return false; }

    memDc_ = CreateCompatibleDC(nullptr);
    if (!memDc_) { Release(); return false; }

    SelectObject(memDc_, bitmap_);
    width_  = width;
    height_ = height;
    return true;
}

void LayeredSurface::Clear() const {
    if (!bits_) return;
    ZeroMemory(bits_, static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4);
}

void LayeredSurface::ApplyAlphaFromLuminance() const {
    if (!bits_) return;

    // 白色文字绘制在黑底上：颜色即覆盖率。白色为预乘结果，直接用作 alpha。
    BYTE* p = static_cast<BYTE*>(bits_);
    const int pixelCount = width_ * height_;
    for (int i = 0; i < pixelCount; ++i, p += 4) {
        const BYTE b = p[0];
        const BYTE g = p[1];
        const BYTE r = p[2];
        BYTE max = b > g ? b : g;
        if (r > max) max = r;
        p[3] = max;
    }
}

bool LayeredSurface::Present(HWND hwnd, int x, int y) const {
    if (!memDc_ || !bitmap_) return false;

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) return false;

    POINT dest    = { x, y };
    POINT src     = { 0, 0 };
    SIZE  size    = { width_, height_ };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    const BOOL ok = UpdateLayeredWindow(hwnd, screenDc, &dest, &size,
                                        memDc_, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDc);
    return ok != FALSE;
}
