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

    // 先复位自身状态，再通知宿主：宿主的回调里会查询 IsActive()，
    // 若此时仍为 true 会提前返回，导致样式无法恢复。
    active_   = false;
    dragging_ = false;

    FinishedCallback cb = finishedCb_;
    void* ctx = finishedCtx_;

    overlay_  = nullptr;
    host_     = nullptr;

    if (cb) cb(ctx, commit);
}

void DragEditor::ClampToScreen(int& x, int& y) const {
    const int screenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > screenWidth - width_)   x = screenWidth - width_;
    if (y > screenHeight - height_) y = screenHeight - height_;
}

void DragEditor::SetMovedCallback(MovedCallback cb, void* context) {
    movedCb_ = cb;
    movedCtx_ = context;
}

void DragEditor::SetFinishedCallback(FinishedCallback cb, void* context) {
    finishedCb_ = cb;
    finishedCtx_ = context;
}

void DragEditor::NotifyMoved() {
    if (movedCb_) movedCb_(movedCtx_);
}

void DragEditor::MoveTo(int x, int y) {
    ClampToScreen(x, y);
    currentX_ = x;
    currentY_ = y;

    SetWindowPos(overlay_, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);

    // 位置变化后立即重绘，否则内容要等下一个 250ms 定时器，
    // 拖动时会看到旧内容停留在新位置上（闪烁/滞后）。
    NotifyMoved();
}

bool DragEditor::OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    if (!active_) return false;

    switch (msg) {
    case WM_LBUTTONDOWN: {
        dragging_ = true;
        SetCapture(hwnd);

        // 必须用屏幕绝对坐标。lParam 是相对窗口客户区的坐标，
        // 而窗口本身正在移动，坐标系随之漂移，会形成滞后与抖动。
        POINT cursor = {};
        GetCursorPos(&cursor);
        dragStartCursor_ = cursor;

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

        POINT cursor = {};
        GetCursorPos(&cursor);
        const int dx = cursor.x - dragStartCursor_.x;
        const int dy = cursor.y - dragStartCursor_.y;

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