#pragma once

#include <windows.h>

// 负责"像素缓冲 + 提交到分层窗口"。
// 不知道文字内容，也不知道窗口样式。
class LayeredSurface {
public:
    LayeredSurface() = default;
    ~LayeredSurface();

    LayeredSurface(const LayeredSurface&) = delete;
    LayeredSurface& operator=(const LayeredSurface&) = delete;

    // 确保缓冲为指定尺寸；尺寸变化时才重建
    bool EnsureSize(int width, int height);

    HDC   Dc() const { return memDc_; }
    void* Bits() const { return bits_; }
    int   Width() const { return width_; }
    int   Height() const { return height_; }

    // 清空为全透明
    void Clear() const;

    // GDI 绘制后 alpha 恒为 0；用亮度推导覆盖率
    void ApplyAlphaFromLuminance() const;

    // 提交到屏幕指定位置
    bool Present(HWND hwnd, int x, int y) const;

private:
    void Release();

    HBITMAP bitmap_ = nullptr;
    HDC     memDc_  = nullptr;
    void*   bits_   = nullptr;
    int     width_  = 0;
    int     height_ = 0;
};
