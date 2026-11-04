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

    // 在文字下层合成一层半透明背景（正确的 alpha 合成）。
    // 用于编辑模式：既提供视觉提示，也让整个窗口可被点击（分层窗口的
    // 命中测试基于 alpha，alpha 为 0 的像素收不到鼠标消息）。
    void CompositeOverBackground(COLORREF background, BYTE backgroundAlpha) const;

    // 画一个 1px 边框（编辑模式的视觉边界）
    void DrawBorder(COLORREF color, BYTE alpha) const;

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