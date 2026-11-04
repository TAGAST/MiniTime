#include "drag_editor.h"

#include <windowsx.h>

namespace {

constexpr UINT_PTR kTimeoutTimerId = 0xDA11;

} // namespace

bool DragEditor::Begin(HWND overlay, HWND host, int timeoutMs) {
    if (!overlay || !host || active_) return false;

    overlay_ = overlay;
    host_    = host;

    RECT rect = {};
    if (!GetWindowRect(overlay_, &rect)) return false;

    origX_ = startX_ = currentX_ = rect.left;
    origY_ = startY_ = currentY_ = rect.top;
    width_  = rect.right - rect.left;
    height_ = rect.bottom - rect.top;
    moved_  = false;
    dragging_ = false;

    timeoutMs_ = timeoutMs > 0 ? static_cast<UINT>(timeoutMs) : 60000u;

    active_ = true;
    timerId_ = SetTimer(host_, kTimeoutTimerId, timeoutMs_, nullptr);

    return true;
}

void DragEditor::End(bool commit) {
    if (!active_) return;

    if (timerId_) {
        KillTimer(host_, kTimeoutTimerId);
        timerId_ = 0;
    }

    if (!commit) {
        MoveTo(origX_, origY_);
    }

    active_   = false;
    dragging_ = false;
    overlay_  = nullptr;
    host_     = nullptr;
}

void DragEditor::ClampToScreen(int& x, int& y) const {
    const int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > screenWidth - width_)   x = screenWidth - width_;
    if (y > screenHeight - height_) y = screenHeight - height_;
}

void DragEditor::MoveTo(int x, int y) {
    ClampToScreen(x, y);
    currentX_ = x;
    currentY_ = y;

    SetWindowPos(overlay_, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
}

bool DragEditor::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!active_) return false;

    switch (msg) {
    case WM_LBUTTONDOWN: {
        dragging_ = true;
        SetCapture(hwnd);

        // 用消息自带的光标坐标而非 GetCursorPos：后者在部分会话/跨 DPI 场景下不可靠
        dragStartCursor_.x = GET_X_LPARAM(lParam);
        dragStartCursor_.y = GET_Y_LPARAM(lParam);

        // 同时记录真实几何，拖拽期间用窗口原点累加相对位移
        RECT rect = {};
        GetWindowRect(overlay_, &rect);
        startX_ = rect.left;
        startY_ = rect.top;
        currentX_ = rect.left;
        currentY_ = rect.top;
        return true;
    }

    case WM_MOUSEMOVE: {
        if (!dragging_) return true;
        const int dx = GET_X_LPARAM(lParam) - dragStartCursor_.x;
        const int dy = GET_Y_LPARAM(lParam) - dragStartCursor_.y;
        MoveTo(startX_ + dx, startY_ + dy);
        moved_ = true;
        return true;
    }

    case WM_LBUTTONUP: {
        if (dragging_) {
            ReleaseCapture();
            dragging_ = false;
            moved_ = true;
        }
        return true;
    }

    case WM_KEYDOWN: {
        // 方向键微调：1px，Shift 加速到 10px
        const int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
        int x = currentX_;
        int y = currentY_;

        switch (wParam) {
        case VK_LEFT:  x -= step; break;
        case VK_RIGHT: x += step; break;
        case VK_UP:    y -= step; break;
        case VK_DOWN:  y += step; break;
        case VK_RETURN: End(true);  return true;
        case VK_ESCAPE: End(false); return true;
        default: return true;
        }

        MoveTo(x, y);
        moved_ = true;
        return true;
    }

    case WM_TIMER:
        if (wParam == kTimeoutTimerId) {
            // 超时自动提交并退出，避免用户忘记退出后游戏无法操作
            End(true);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}