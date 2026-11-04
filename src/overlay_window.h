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

    // 点击穿透开关。关闭穿透后才能接收鼠标消息（编辑模式用）。
    void SetClickThrough(bool enabled);
    bool IsClickThrough() const { return clickThrough_; }

    // 获取当前位置与尺寸
    RECT Bounds() const;

    static const wchar_t* ClassName();

    // 消息转发钩子：编辑模式下由宿主处理鼠标与按键
    using MessageHook = bool (*)(void* context, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void SetMessageHook(MessageHook hook, void* context);

    // 让窗口可获得焦点（编辑模式需要接收按键）
    void SetFocusable(bool focusable);

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_         = nullptr;
    bool visible_      = true;
    bool clickThrough_ = true;
    MessageHook hook_  = nullptr;
    void*       hookContext_ = nullptr;
};