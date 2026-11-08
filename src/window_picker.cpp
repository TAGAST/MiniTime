#include "window_picker.h"

#include <windowsx.h>

namespace {

constexpr const wchar_t* kPickClassName = L"MiniTimeWindowPicker";
constexpr UINT_PTR kTimeoutTimerId = 0x9C01;

struct FindContext {
    POINT point = { 0, 0 };
    HWND  result = nullptr;
};

BOOL CALLBACK FindWindowAtProc(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<FindContext*>(param);

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (hwnd == ctx->result) return TRUE;

    // 排除拾取窗口自身与任务栏
    wchar_t cls[256] = {};
    GetClassNameW(hwnd, cls, 256);
    if (wcscmp(cls, kPickClassName) == 0) return TRUE;
    if (wcscmp(cls, L"Shell_TrayWnd") == 0) return TRUE;
    if (wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0) return TRUE;

    RECT rc = {};
    if (!GetWindowRect(hwnd, &rc)) return TRUE;

    if (ctx->point.x < rc.left || ctx->point.x >= rc.right ||
        ctx->point.y < rc.top  || ctx->point.y >= rc.bottom) {
        return TRUE;
    }

    // EnumWindows 自上而下，第一个命中的就是最上层窗口
    ctx->result = hwnd;
    return FALSE;
}

ForegroundInfo ResolveTargetAt(POINT screenPt) {
    ForegroundInfo info;

    FindContext ctx;
    ctx.point = screenPt;
    EnumWindows(&FindWindowAtProc, reinterpret_cast<LPARAM>(&ctx));

    if (!ctx.result) return info;

    DWORD pid = 0;
    GetWindowThreadProcessId(ctx.result, &pid);
    if (pid == 0) return info;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return info;

    wchar_t path[MAX_PATH * 2] = {};
    DWORD size = MAX_PATH * 2;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
        info.processName = path;
    }
    CloseHandle(process);

    if (info.processName.empty()) return info;

    wchar_t title[512] = {};
    GetWindowTextW(ctx.result, title, 512);
    info.windowTitle = title;
    info.valid = true;
    return info;
}

} // namespace

WindowPicker::~WindowPicker() { Cancel(); }

bool WindowPicker::Begin(HINSTANCE instance, int timeoutMs,
                         PickedCallback onPicked,
                         CancelCallback onCancel,
                         void* context) {
    if (hwnd_) return false;
    if (!instance) return false;

    instance_ = instance;
    pickedCb_ = onPicked;
    cancelCb_ = onCancel;
    context_  = context;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &WindowPicker::WndProcThunk;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kPickClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // 覆盖整个虚拟屏幕（含多显示器区域）
    const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 半透明深色遮罩：让用户看清正处于"选择窗口"状态，
    // 同时该窗口会拦截点击，从而让下面那层被识别。
    // WS_EX_LAYERED 是 SetLayeredWindowAttributes 生效的前提，
    // 缺少它会把遮罩变成不透明白屏，完全挡住用户视线。
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
                            kPickClassName, L"",
                            WS_POPUP,
                            x, y, w, h,
                            nullptr, nullptr, instance, this);
    if (!hwnd_) return false;

    // 整体半透明，用户仍能看清下面的窗口
    if (!SetLayeredWindowAttributes(hwnd_, 0, 70, LWA_ALPHA)) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);

    const UINT timeout = timeoutMs > 0 ? static_cast<UINT>(timeoutMs) : 15000u;
    timerId_ = SetTimer(hwnd_, kTimeoutTimerId, timeout, nullptr);

    return true;
}

void WindowPicker::Cancel() { Finish(nullptr); }

void WindowPicker::Finish(const ForegroundInfo* picked) {
    if (!hwnd_) {
        // 仍然清理回调状态
        pickedCb_ = nullptr;
        cancelCb_ = nullptr;
        context_  = nullptr;
        return;
    }

    if (timerId_) {
        KillTimer(hwnd_, kTimeoutTimerId);
        timerId_ = 0;
    }

    DestroyWindow(hwnd_);
    hwnd_ = nullptr;

    // 先取回调再清空，避免回调里再次触发
    PickedCallback onPicked = pickedCb_;
    CancelCallback onCancel = cancelCb_;
    void* ctx = context_;

    pickedCb_ = nullptr;
    cancelCb_ = nullptr;
    context_  = nullptr;

    if (picked && picked->valid) {
        if (onPicked) onPicked(ctx, *picked);
    } else {
        if (onCancel) onCancel(ctx);
    }
}

LRESULT CALLBACK WindowPicker::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowPicker* self = reinterpret_cast<WindowPicker*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<WindowPicker*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self) return self->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT WindowPicker::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &pt);

        const ForegroundInfo info = ResolveTargetAt(pt);
        Finish(info.valid ? &info : nullptr);
        return 0;
    }

    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        Cancel();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            Cancel();
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == kTimeoutTimerId) {
            Cancel();
            return 0;
        }
        break;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}