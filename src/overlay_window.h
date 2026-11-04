#pragma once

#include <windows.h>

// 负责 HWND 的创建与窗口样式。
// 不知道显示内容，也不知道渲染方式。
class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool Create(HINSTANCE instance);

    HWND Handle() const { return hwnd_; }
    bool IsVisible() const { return visible_; }

    void SetVisible(bool visible);
    void ToggleVisibility() { SetVisible(!visible_); }

    // 维持置顶（前台变化/被遮挡时调用）
    void RaiseTopmost() const;

    // 设置窗口矩形（像素）
    void SetBounds(int x, int y, int width, int height) const;

    static const wchar_t* ClassName();

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_     = nullptr;
    bool visible_  = true;
};