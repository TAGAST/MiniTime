#include "overlay_window.h"

namespace {

constexpr const wchar_t* kClassName = L"MiniTimeOverlay";

} // namespace

const wchar_t* OverlayWindow::ClassName() { return kClassName; }

OverlayWindow::~OverlayWindow() { Destroy(); }

void OverlayWindow::Destroy() {
    if (!hwnd_) return;

    // 分层窗口的像素由 DWM 持有。仅隐藏/销毁窗口时，
    // DWM 可能保留最后一次合成的结果，导致退出后屏幕残留文字。
    // 因此先把内容刷成全透明，再隐藏并销毁。
    const int width  = 1;
    const int height = 1;
    HDC screenDc = GetDC(nullptr);
    if (screenDc) {
        BITMAPINFO info = {};
        info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth       = width;
        info.bmiHeader.biHeight      = -height;
        info.bmiHeader.biPlanes      = 1;
        info.bmiHeader.biBitCount    = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bmp && bits) {
            HDC memDc = CreateCompatibleDC(nullptr);
            if (memDc) {
                SelectObject(memDc, bmp);
                ZeroMemory(bits, static_cast<size_t>(width) * height * 4);

                POINT dest = { 0, 0 };
                SIZE  size = { width, height };
                BLENDFUNCTION blend = { AC_SRC_OVER, 0, 0, AC_SRC_ALPHA };
                UpdateLayeredWindow(hwnd_, screenDc, &dest, &size, memDc, &dest, 0, &blend, ULW_ALPHA);
                DeleteDC(memDc);
            }
            DeleteObject(bmp);
        }
        ReleaseDC(nullptr, screenDc);
    }

    ShowWindow(hwnd_, SW_HIDE);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
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
    // 编辑模式下优先交给宿主处理
    if (hook_ && hook_(hookContext_, hwnd, msg, wParam, lParam)) {
        return 0;
    }

    switch (msg) {
    case WM_NCHITTEST:
        // 仅穿过态才返回 HTTRANSPARENT；编辑态需要正常接收鼠标
        if (clickThrough_) return HTTRANSPARENT;
        break;
    case WM_SETCURSOR:
        if (clickThrough_) break;
        SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
        return TRUE;
    case WM_DESTROY:
        hwnd_ = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OverlayWindow::SetMessageHook(MessageHook hook, void* context) {
    hook_ = hook;
    hookContext_ = context;
}

void OverlayWindow::SetFocusable(bool focusable) {
    if (!hwnd_) return;
    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (!focusable) {
        ex |= WS_EX_NOACTIVATE;
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);

    if (focusable) {
        SetForegroundWindow(hwnd_);
        SetFocus(hwnd_);
    }
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

namespace {
// 小于该尺寸的窗口视为系统辅助窗口，不参与遮挡判断
constexpr int kMinOccluderSize = 2;
}   // namespace

bool OverlayWindow::IsTopmost() const {
    if (!hwnd_) return false;

    // 先看自己是否还在置顶层级
    if ((GetWindowLongPtrW(hwnd_, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0) {
        return false;
    }

    // 再看置顶层里有没有窗口真的排在自己之上。
    // 注意：不能只看 IsWindowVisible —— 系统里有些带 TOPMOST 的辅助窗口
    // （例如 explorer 的 ThumbnailDeviceHelperWnd）也报告可见，
    // 但它们没有尺寸、永远挡不住内容，必须按矩形排除掉。
    for (HWND h = GetWindow(hwnd_, GW_HWNDPREV); h; h = GetWindow(h, GW_HWNDPREV)) {
        const LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
        if ((ex & WS_EX_TOPMOST) == 0) break;      // 已走出置顶层
        if (!IsWindowVisible(h)) continue;

        RECT r = {};
        if (!GetWindowRect(h, &r)) continue;

        // 系统里还存在 1x1 的置顶辅助窗口（explorer 缩略图等），
        // 它们同样挡不住内容，一并排除。
        const int w = r.right - r.left;
        const int hgt = r.bottom - r.top;
        if (w < kMinOccluderSize || hgt < kMinOccluderSize) continue;

        return false;
    }

    return true;
}

void OverlayWindow::SetBounds(int x, int y, int width, int height) const {
    if (!hwnd_) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, x, y, width, height,
                 SWP_NOACTIVATE);
}

void OverlayWindow::SetClickThrough(bool enabled) {
    if (!hwnd_ || clickThrough_ == enabled) return;

    clickThrough_ = enabled;

    LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (enabled) {
        ex |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    } else {
        ex &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);

    // 修改扩展样式后必须带 SWP_FRAMECHANGED，否则不生效
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

RECT OverlayWindow::Bounds() const {
    RECT rect = { 0, 0, 0, 0 };
    if (hwnd_) GetWindowRect(hwnd_, &rect);
    return rect;
}