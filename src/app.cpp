#include "app.h"

namespace {

constexpr UINT_PTR kRefreshTimerId = 1;
constexpr int      kHotkeyMods     = MOD_CONTROL | MOD_ALT;
constexpr UINT     kHotkeyVirtual  = 'T';

} // namespace

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

int App::Run(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    (void)showCommand;

    if (!overlay_.Create(instance)) return 1;

    // 叠加窗口由 OverlayWindow 自己处理鼠标穿透；
    // 这里用一个隐藏的消息窗承载定时器与热键，避免把业务逻辑塞进叠加窗口。
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &App::RouterThunk;
    wc.hInstance     = instance;
    wc.lpszClassName = L"MiniTimeRouter";

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return 1;
    }

    HWND router = CreateWindowExW(0, L"MiniTimeRouter", L"", 0,
                                  0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, this);
    if (!router) return 1;

    presenter_.SetFormat(format_);
    presenter_.Render(overlay_.Handle());

    timerMs_ = timefmt::RefreshIntervalMs(format_);
    SetTimer(router, kRefreshTimerId, timerMs_, nullptr);

    if (!hotkeys_.Register(router, kHotkeyMods, kHotkeyVirtual)) {
        // 热键被占用不阻塞其余功能
        OutputDebugStringW(L"MiniTime: hotkey Ctrl+Alt+T unavailable\n");
    }

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(router, kRefreshTimerId);
    hotkeys_.UnregisterAll();
    DestroyWindow(router);
    return 0;
}

LRESULT App::Route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == kRefreshTimerId) { OnTick(); return 0; }
        break;

    case WM_HOTKEY:
        OnToggleVisibility();
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_TIMECHANGE:
        presenter_.Render(overlay_.Handle());
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            presenter_.Render(overlay_.Handle());
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

void App::OnTick() {
    if (overlay_.IsVisible()) presenter_.Render(overlay_.Handle());
}

void App::OnToggleVisibility() {
    overlay_.ToggleVisibility();
    if (overlay_.IsVisible()) {
        overlay_.RaiseTopmost();
        presenter_.Render(overlay_.Handle());
    }
}