#pragma once

#include "clock_presenter.h"
#include "config.h"
#include "drag_editor.h"
#include "hotkey.h"
#include "menu_commands.h"
#include "overlay_window.h"
#include "renderer_types.h"
#include "timefmt.h"
#include "tray_icon.h"
#include "tray_menu.h"

#include <windows.h>

// 组合根：只负责接线与消息分发，不含绘制/布局/格式化的具体逻辑。
class App {
public:
    App() = default;
    ~App();

    int Run(HINSTANCE instance, int showCommand);

private:
    static LRESULT CALLBACK RouterThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT Route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool CreateRouterWindow();
    bool CreateTrayIcon();
    HICON LoadTraySizedIcon() const;
    void ReloadTrayIcon();
    void UpdateTrayTooltip();

    void ShowTrayMenu();
    void RegisterHotkeysFromConfig();
    static bool IsHotkeyCommand(MenuCommand command);
    void HandleMenuCommand(MenuCommand command);

    // 编辑模式
    void EnterEditMode();
    void ExitEditMode(bool commit);
    static bool OverlayMessageThunk(void* context, HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
    static void OnDragMovedThunk(void* context);
    void ApplyConfigToPresenter();
    void RenderNow();

    void OnTick();
    void EnsureVisibleAndTopmost();
    void OnToggleVisibility();
    void Shutdown();

    HINSTANCE      instance_      = nullptr;
    HWND           router_        = nullptr;
    OverlayWindow  overlay_       = {};
    ClockPresenter presenter_     = {};
    HotkeyRegistry hotkeys_       = {};
    TrayIcon       trayIcon_      = {};
    TrayMenu       trayMenu_      = {};
    DragEditor     dragEditor_    = {};
    HICON          appIcon_       = nullptr;
    UINT           trayCallback_  = 0;
    TimeFormat     format_        = {};
    AppConfig      config_        = {};
    UINT           timerMs_       = 0;
    bool           shuttingDown_  = false;
};