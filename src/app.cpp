#include "app.h"

#include "autostart.h"
#include "resource.h"

namespace {

constexpr UINT_PTR kRefreshTimerId = 1;
constexpr int      kHotkeyMods     = MOD_CONTROL | MOD_ALT;
constexpr UINT     kHotkeyVirtual  = 'T';
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

    appIcon_ = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                   0, 0, LR_DEFAULTSIZE | LR_SHARED));

    if (!appIcon_) {
        appIcon_ = LoadIconW(nullptr, IDI_APPLICATION);
    }

    return trayIcon_.Add(router_, trayCallback_, appIcon_, L"MiniTime");
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

    CreateTrayIcon();
    UpdateTrayTooltip();
    autostart::SyncPathIfEnabled();

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
        OnToggleVisibility();
        return 0;

    case WM_DISPLAYCHANGE:
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

void App::OnTick() {
    RenderNow();
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
