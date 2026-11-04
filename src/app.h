#pragma once

#include "clock_presenter.h"
#include "hotkey.h"
#include "overlay_window.h"
#include "renderer_types.h"
#include "timefmt.h"

#include <windows.h>

// 组合根：只负责接线与消息分发，不含绘制/布局/格式化的具体逻辑。
class App {
public:
    App() = default;

    int Run(HINSTANCE instance, int showCommand);

private:
    static LRESULT CALLBACK RouterThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void OnTick();
    void OnToggleVisibility();

    HINSTANCE      instance_  = nullptr;
    OverlayWindow  overlay_   = {};
    ClockPresenter presenter_ = {};
    HotkeyRegistry hotkeys_   = {};
    TimeFormat     format_    = {};
    UINT           timerMs_   = 0;
};