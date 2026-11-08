#pragma once

#include "clock_presenter.h"
#include "config.h"
#include "drag_editor.h"
#include "game_profile.h"
#include "text_input.h"
#include "window_picker.h"
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

    // 游戏位置配置
    void LoadGameProfiles();
    void OnForegroundChanged();
    void ApplyProfileFor(const std::wstring& processName);
    void StartWindowPick();
    void OnWindowPicked(const ForegroundInfo& info);
    void OnWindowPickCancelled();
    static void WindowPickedThunk(void* context, const ForegroundInfo& info);
    static void WindowPickCancelledThunk(void* context);
    // 菜单弹出瞬间捕获的前台信息：点击菜单项时前台可能已改变
    // （托盘菜单、任务栏会成为前台），因此不能重新查询。
    ForegroundInfo menuForeground_ = {};
 
    void SelectProfile(size_t index);
    void RenameProfile(size_t index);
    void DeleteProfile(size_t index);
    void ApplyActiveLayout();
    static void WinEventThunk(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                              LONG idObject, LONG idChild, DWORD thread, DWORD time);

    // 前台钩子是自由函数回调，需要一个可访问的实例指针
    static App* s_instance;
    void SyncRefreshTimer();
    static bool IsHotkeyCommand(MenuCommand command);
    static bool IsFormatCommand(MenuCommand command);
    void HandleMenuCommand(MenuCommand command);

    // 编辑模式
    void EnterEditMode();
    void ExitEditMode(bool commit);
    static bool OverlayMessageThunk(void* context, HWND hwnd, UINT msg,
                                    WPARAM wParam, LPARAM lParam);
    static void OnDragMovedThunk(void* context);
    static void OnEditFinishedThunk(void* context, bool commit);
    void RestoreAfterEdit(bool commit, POINT pos, bool moved);
    POINT ComputeCommittedPosition() const;
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
    WindowPicker   windowPicker_  = {};
    TextInputDialog textInput_     = {};
    POINT          pickSavePos_   = { 0, 0 };   // 开始拾取时的时钟位置
    HICON          appIcon_       = nullptr;
    UINT           trayCallback_  = 0;
    TimeFormat     format_        = {};
    AppConfig      config_        = {};
    std::vector<GameProfile> profiles_;
    std::wstring   activeProfile_ = {};   // 当前生效配置的进程名（空=用全局配置）
    HWINEVENTHOOK  foregroundHook_ = nullptr;
    UINT           timerMs_       = 0;
    bool           shuttingDown_  = false;
};