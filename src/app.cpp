#include "app.h"

#include "autostart.h"
#include "resource.h"

namespace {

constexpr UINT_PTR kRefreshTimerId = 1;
constexpr int      kHotkeyMods     = MOD_CONTROL | MOD_ALT;
constexpr UINT     kHotkeyVirtual  = 'T';
constexpr int      kHotkeyEditId   = 2;
constexpr UINT     kHotkeyEditKey  = 'E';
constexpr const wchar_t* kRouterClass = L"MiniTimeRouter";

void ReportHotkeyFailure() {
    OutputDebugStringW(L"MiniTime: hotkey Ctrl+Alt+T unavailable\n");
}

} // namespace

App::~App() { Shutdown(); }

bool App::CreateRouterWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &App::RouterThunk;
    wc.hInstance     = instance_;
    wc.lpszClassName = kRouterClass;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // 必须是普通窗口而非 HWND_MESSAGE：仅消息窗口收不到 TaskbarCreated 广播，
    // 那样 explorer.exe 重启后托盘图标就无法自动恢复。这里用不可见顶层窗口。
    router_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              kRouterClass, L"", WS_POPUP,
                              0, 0, 0, 0, nullptr, nullptr, instance_, this);
    return router_ != nullptr;
}

bool App::CreateTrayIcon() {
    trayCallback_ = RegisterWindowMessageW(L"MiniTimeTrayCallback");

    appIcon_ = LoadTraySizedIcon();
    if (!appIcon_) {
        appIcon_ = LoadIconW(nullptr, IDI_APPLICATION);
    }

    return trayIcon_.Add(router_, trayCallback_, appIcon_, L"MiniTime");
}

HICON App::LoadTraySizedIcon() const {
    // 托盘图标必须按 SM_CXSMICON 加载（通常 16px，150% DPI 下 24px）。
    // 用 LR_DEFAULTSIZE 会按 SM_CXICON 取 48px，导致图标撑满任务栏而无法正常显示。
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    // 不共享句柄：DPI 变化时需要按新尺寸重新加载
    HICON icon = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                   cx, cy, LR_DEFAULTCOLOR));
    if (icon) return icon;

    // 回退：直接加载图标资源
    return static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                   cx, cy, 0));
}

void App::ReloadTrayIcon() {
    HICON fresh = LoadTraySizedIcon();
    if (!fresh) return;

    if (appIcon_ && appIcon_ != LoadIconW(nullptr, IDI_APPLICATION)) {
        DestroyIcon(appIcon_);
    }
    appIcon_ = fresh;

    if (!trayIcon_.ReplaceIcon(appIcon_)) {
        trayIcon_.ReAdd();
    }
}

void App::UpdateTrayTooltip() {
    const wchar_t* text = overlay_.IsVisible() ? L"MiniTime — 显示中" : L"MiniTime — 已隐藏";
    trayIcon_.SetTooltip(text);
}

void App::ApplyConfigToPresenter() {
    presenter_.SetStyle(config_.ToTextStyle());
    presenter_.SetLayout(config_.ToLayoutConfig());
    presenter_.SetFormat(format_);
}

void App::RenderNow() {
    if (overlay_.IsVisible()) {
        presenter_.Render(overlay_.Handle());
    }
}

int App::Run(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    (void)showCommand;

    config_ = config::Load();

    if (!overlay_.Create(instance)) return 1;
    if (!CreateRouterWindow()) return 1;

    ApplyConfigToPresenter();
    presenter_.Render(overlay_.Handle());

    timerMs_ = timefmt::RefreshIntervalMs(format_);
    SetTimer(router_, kRefreshTimerId, timerMs_, nullptr);

    if (!hotkeys_.Register(router_, kHotkeyMods, kHotkeyVirtual)) {
        ReportHotkeyFailure();
    }

    // 编辑模式热键（可选功能，失败不影响主功能）
    RegisterHotKey(router_, kHotkeyEditId, kHotkeyMods | MOD_NOREPEAT, kHotkeyEditKey);

    CreateTrayIcon();
    UpdateTrayTooltip();
    autostart::SyncPathIfEnabled();

    overlay_.SetMessageHook(&App::OverlayMessageThunk, this);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Shutdown();
    return 0;
}

LRESULT CALLBACK App::RouterThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->Route(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::Route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // explorer.exe 重启后恢复托盘图标
    if (msg != 0 && msg == trayIcon_.TaskbarCreatedMessage()) {
        trayIcon_.ReAdd();
        UpdateTrayTooltip();
        return 0;
    }

    if (msg == trayCallback_) {
        const UINT event = LOWORD(lParam);
        if (event == WM_LBUTTONUP || event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;
    }

    switch (msg) {
    case WM_TIMER:
        if (wParam == kRefreshTimerId) { OnTick(); return 0; }
        break;

    case WM_HOTKEY:
        if (wParam == kHotkeyEditId) {
            EnterEditMode();
        } else {
            OnToggleVisibility();
        }
        return 0;

    case WM_APP + 1:
        // 第二个实例启动时请求：把叠加层重新显示出来
        EnsureVisibleAndTopmost();
        return 0;

    case WM_DISPLAYCHANGE:
        RenderNow();
        return 0;

    case WM_DPICHANGED:
        // DPI 变化后托盘图标需要按新尺寸重新加载
        ReloadTrayIcon();
        RenderNow();
        return 0;

    case WM_TIMECHANGE:
        RenderNow();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            RenderNow();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void App::ShowTrayMenu() {
    POINT pt = {};
    GetCursorPos(&pt);

    TrayMenuState state = {};
    state.visible   = overlay_.IsVisible();
    state.autostart = autostart::IsEnabled();
    state.config    = config_;

    const MenuCommand command = trayMenu_.Show(router_, pt, state);
    HandleMenuCommand(command);
}

void App::HandleMenuCommand(MenuCommand command) {
    if (command == MenuCommand::None) return;

    switch (command) {
    case MenuCommand::ToggleShow:
        OnToggleVisibility();
        return;

    case MenuCommand::Autostart: {
        const bool next = !autostart::IsEnabled();
        autostart::SetEnabled(next);
        return;
    }

    case MenuCommand::Exit:
        DestroyWindow(router_);
        return;

    case MenuCommand::EnterEditMode:
        EnterEditMode();
        return;

    default:
        break;
    }

    // 样式与位置类命令：修改配置 -> 立即生效 -> 持久化
    if (config::ApplyCommand(config_, command)) {
        ApplyConfigToPresenter();
        RenderNow();
        config::Save(config_);
    }
}

bool App::OverlayMessageThunk(void* context, HWND hwnd, UINT msg,
                              WPARAM wParam, LPARAM lParam) {
    auto* self = static_cast<App*>(context);
    if (!self) return false;
    return self->dragEditor_.OnMessage(hwnd, msg, wParam, lParam);
}

void App::EnterEditMode() {
    if (dragEditor_.IsActive()) {
        ExitEditMode(true);
        return;
    }

    // 1 分钟无操作自动退出，避免忘记退出后游戏无法操作
    constexpr int kEditTimeoutMs = 60 * 1000;
    if (!dragEditor_.Begin(overlay_.Handle(), router_, kEditTimeoutMs)) {
        return;
    }

    // 编辑期间由拖动接管位置，渲染层不得覆写
    presenter_.SetLayoutSuspended(true);

    overlay_.SetClickThrough(false);
    overlay_.SetFocusable(true);
    UpdateTrayTooltip();
}

void App::ExitEditMode(bool commit) {
    if (!dragEditor_.IsActive()) return;

    // 以窗口实际位置为准，避免内部记录与真实几何不一致
    const RECT actual = overlay_.Bounds();
    const POINT pos = { actual.left, actual.top };
    const bool moved = dragEditor_.HasMoved();

    dragEditor_.End(commit);

    // 恢复点击穿透与不可获得焦点的行为
    overlay_.SetClickThrough(true);
    overlay_.SetFocusable(false);
    presenter_.SetLayoutSuspended(false);

    if (commit && moved) {
        config_.anchor  = Anchor::Custom;
        config_.customX = pos.x;
        config_.customY = pos.y;
        config::Save(config_);
    }

    // 位置变化后需让渲染层重新同步窗口几何
    presenter_.InvalidateGeometry();
    ApplyConfigToPresenter();
    RenderNow();
    UpdateTrayTooltip();
}

void App::OnTick() {
    RenderNow();
}

void App::EnsureVisibleAndTopmost() {
    overlay_.SetVisible(true);
    overlay_.RaiseTopmost();
    ApplyConfigToPresenter();
    RenderNow();
    UpdateTrayTooltip();
}

void App::OnToggleVisibility() {
    overlay_.ToggleVisibility();
    if (overlay_.IsVisible()) {
        overlay_.RaiseTopmost();
        RenderNow();
    }
    UpdateTrayTooltip();
}

void App::Shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;

    if (router_) {
        KillTimer(router_, kRefreshTimerId);
    }
    hotkeys_.UnregisterAll();
    trayIcon_.Remove();

    if (router_) {
        DestroyWindow(router_);
        router_ = nullptr;
    }
}