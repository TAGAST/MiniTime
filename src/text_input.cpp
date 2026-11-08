#include "text_input.h"

#include <commctrl.h>

#include <algorithm>

namespace {

constexpr const wchar_t* kClassName = L"MiniTimeTextInput";
constexpr int kClientWidth  = 380;
constexpr int kClientHeight = 108;
constexpr int kMargin       = 14;
constexpr int kEditHeight   = 26;
constexpr int kButtonWidth  = 84;
constexpr int kButtonHeight = 28;
constexpr int kButtonGap    = 10;

// 去掉首尾空白
std::wstring Trim(const std::wstring& s) {
    const wchar_t* ws = L" \t\r\n";
    const size_t begin = s.find_first_not_of(ws);
    if (begin == std::wstring::npos) return std::wstring();
    const size_t end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

} // namespace

TextInputDialog::~TextInputDialog() {
    if (font_) DeleteObject(font_);
}

bool TextInputDialog::Show(HINSTANCE instance, HWND owner,
                           const std::wstring& title,
                           const std::wstring& initialText,
                           std::wstring& outText) {
    if (hwnd_) return false;
    if (!instance) return false;

    title_ = title;
    text_  = Trim(initialText);
    accepted_ = false;
    done_ = false;

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &TextInputDialog::WndProcThunk;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT ownerRect = { 0, 0, 0, 0 };
    if (owner) GetWindowRect(owner, &ownerRect);

    // 以屏幕中心为基准（owner 通常是隐藏的消息窗口，其坐标为 0 无意义）
    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);

    RECT desired = { 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRectEx(&desired, WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);

    const int w = desired.right - desired.left;
    const int h = desired.bottom - desired.top;
    const int x = (screenW - w) / 2;
    const int y = (screenH - h) / 2;

    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        kClassName, title_.c_str(),
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x, y, w, h,
        nullptr, nullptr, instance, this);

    if (!hwnd_) return false;

    CreateControls(instance);
    LayoutControls();

    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(edit_);

    // 全选，便于直接覆盖输入
    SendMessageW(edit_, EM_SETSEL, 0, -1);

    // 模态循环：只取本窗口相关消息，避免吞掉主程序的热键与托盘消息
    MSG msg = {};
    while (!done_ && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.hwnd == hwnd_ || IsChild(hwnd_, msg.hwnd)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else {
            // 其它窗口的消息交回给主消息循环
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (font_) {
        DeleteObject(font_);
        font_ = nullptr;
    }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    edit_ = okBtn_ = cancelBtn_ = nullptr;

    if (accepted_ && !text_.empty()) {
        outText = text_;
        return true;
    }
    return false;
}

// EDIT 控件会吞掉回车，需要子类化把 VK_RETURN / VK_ESCAPE 转发给父窗口
static LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam, UINT_PTR idSubclass,
                                         DWORD_PTR refData) {
    (void)idSubclass;
    (void)refData;

    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) {
            SendMessageW(GetParent(hwnd), msg, wParam, lParam);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void TextInputDialog::CreateControls(HINSTANCE instance) {
    font_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                        L"Segoe UI");

    edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text_.c_str(),
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            0, 0, 0, 0, hwnd_, nullptr,
                            instance, nullptr);

    okBtn_ = CreateWindowExW(0, L"BUTTON", L"确定",
                             WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                             0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(1),
                             instance, nullptr);

    cancelBtn_ = CreateWindowExW(0, L"BUTTON", L"取消",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(2),
                                 instance, nullptr);

    SetWindowSubclass(edit_, &EditSubclassProc, 1, 0);

    if (font_) {
        SendMessageW(edit_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(okBtn_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        SendMessageW(cancelBtn_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    }
}

void TextInputDialog::LayoutControls() {
    if (!hwnd_) return;

    RECT rc = {};
    GetClientRect(hwnd_, &rc);
    const int cw = rc.right - rc.left;

    MoveWindow(edit_, kMargin, kMargin,
               cw - kMargin * 2, kEditHeight, TRUE);

    const int btnY = kMargin + kEditHeight + kMargin;
    const int rightX = cw - kMargin - kButtonWidth;

    MoveWindow(cancelBtn_, rightX, btnY, kButtonWidth, kButtonHeight, TRUE);
    MoveWindow(okBtn_, rightX - kButtonGap - kButtonWidth, btnY,
               kButtonWidth, kButtonHeight, TRUE);
}

void TextInputDialog::Confirm() {
    wchar_t buf[256] = {};
    GetWindowTextW(edit_, buf, 256);

    const std::wstring trimmed = Trim(buf);
    if (trimmed.empty()) {
        // 不允许空名字：留在对话框内
        MessageBeep(MB_ICONWARNING);
        SetFocus(edit_);
        return;
    }

    text_ = trimmed;
    accepted_ = true;
    done_ = true;
}

void TextInputDialog::CancelDialog() {
    accepted_ = false;
    done_ = true;
}

LRESULT CALLBACK TextInputDialog::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TextInputDialog* self = reinterpret_cast<TextInputDialog*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<TextInputDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self) return self->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT TextInputDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == 1) { Confirm(); return 0; }
        if (id == 2) { CancelDialog(); return 0; }
        break;
    }

    case WM_CLOSE:
        CancelDialog();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN) { Confirm(); return 0; }
        if (wParam == VK_ESCAPE) { CancelDialog(); return 0; }
        break;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}