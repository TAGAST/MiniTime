#pragma once

#include "game_profile.h"

#include <windows.h>

// 窗口拾取器：临时创建一个全屏透明窗口，
// 用户点击后把它下面的真实窗口识别出来。
//
// 用它代替"读取菜单弹出瞬间的前台窗口"，是因为后者受时序影响：
// 托盘菜单本身会改变前台窗口，可能记录到错误的进程。
class WindowPicker {
public:
    WindowPicker() = default;
    ~WindowPicker();

    WindowPicker(const WindowPicker&) = delete;
    WindowPicker& operator=(const WindowPicker&) = delete;

    using PickedCallback = void (*)(void* context, const ForegroundInfo& info);
    using CancelCallback = void (*)(void* context);

    // 开始捕获。timeoutMs 到时自动取消。
    bool Begin(HINSTANCE instance, int timeoutMs,
               PickedCallback onPicked, CancelCallback onCancel, void* context);

    void Cancel();
    bool IsActive() const { return hwnd_ != nullptr; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void Finish(const ForegroundInfo* picked);

    HWND  hwnd_    = nullptr;
    HINSTANCE instance_ = nullptr;
    UINT_PTR timerId_ = 0;

    PickedCallback pickedCb_ = nullptr;
    CancelCallback cancelCb_ = nullptr;
    void*          context_  = nullptr;
};