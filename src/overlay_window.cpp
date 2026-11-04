#include "overlay_window.h"

namespace {

constexpr const wchar_t* kClassName = L"MiniTimeOverlay";

} // namespace

const wchar_t* OverlayWindow::ClassName() { return kClassName; }

OverlayWindow::~OverlayWindow() {
    if (hwnd_) DestroyWindow(hwnd_);
}

LRESULT CALLBACK OverlayWindow::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OverlayWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        // 冗余保障：即使样式被外部改动也保持穿透
        return HTTRANSPARENT;
    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool OverlayWindow::Create(HINSTANCE instance) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &OverlayWindow::WndProcThunk;
    wc.hInstance     = instance;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    const DWORD exStyle = WS_EX_TOPMOST      // 置顶
                        | WS_EX_LAYERED      // 逐像素透明
                        | WS_EX_TRANSPARENT  // 点击穿透
                        | WS_EX_TOOLWINDOW   // 不进 Alt+Tab / 任务栏
                        | WS_EX_NOACTIVATE;  // 不抢焦点

    hwnd_ = CreateWindowExW(exStyle, kClassName, L"MiniTime", WS_POPUP,
                            0, 0, 1, 1, nullptr, nullptr, instance, this);
    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

void OverlayWindow::SetVisible(bool visible) {
    visible_ = visible;
    if (!hwnd_) return;
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void OverlayWindow::RaiseTopmost() const {
    if (!hwnd_) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void OverlayWindow::SetBounds(int x, int y, int width, int height) const {
    if (!hwnd_) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE);
}