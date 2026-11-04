#include "tray_icon.h"

namespace {

constexpr UINT kIconId = 1;
constexpr const wchar_t* kTaskbarCreatedName = L"TaskbarCreated";

} // namespace

TrayIcon::~TrayIcon() {
    Remove();
    if (ownsIcon_ && icon_) {
        DestroyIcon(icon_);
        icon_ = nullptr;
    }
}

bool TrayIcon::Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip) {
    owner_       = owner;
    callbackMsg_ = callbackMessage;

    if (icon_ && ownsIcon_) DestroyIcon(icon_);
    icon_     = icon;
    ownsIcon_ = false;

    if (tooltip) {
        wcsncpy_s(tooltip_, 128, tooltip, _TRUNCATE);
    }

    // explorer.exe 重启时系统广播此消息，需要重新添加图标
    taskbarCreatedMsg_ = RegisterWindowMessageW(kTaskbarCreatedName);

    return ReAdd();
}

bool TrayIcon::ReAdd() {
    if (!owner_ || !callbackMsg_ || !icon_) return false;

    // 若之前已存在，先确保移除后再添加，避免重复项
    if (added_) {
        NOTIFYICONDATAW existing = {};
        existing.cbSize = sizeof(existing);
        existing.hWnd   = owner_;
        existing.uID    = kIconId;
        Shell_NotifyIconW(NIM_DELETE, &existing);
        added_ = false;
    }

    NOTIFYICONDATAW data = {};
    FillData(data);

    if (!Shell_NotifyIconW(NIM_ADD, &data)) return false;

    // 顺序很重要：先设为 v4，再单独 MODIFY 启用 SHOWTIP。
    // 在 NIM_ADD 阶段带 NIF_SHOWTIP 属于无效组合，会导致图标不显示。
    NOTIFYICONDATAW version = {};
    version.cbSize   = sizeof(version);
    version.hWnd     = owner_;
    version.uID      = kIconId;
    version.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &version);

    NOTIFYICONDATAW showTip = {};
    showTip.cbSize = sizeof(showTip);
    showTip.hWnd   = owner_;
    showTip.uID    = kIconId;
    showTip.uFlags = NIF_SHOWTIP;
    Shell_NotifyIconW(NIM_MODIFY, &showTip);

    added_ = true;
    return true;
}

void TrayIcon::FillData(NOTIFYICONDATAW& data) const {
    data = {};
    data.cbSize           = sizeof(NOTIFYICONDATAW);
    data.hWnd             = owner_;
    data.uID              = kIconId;
    // 注意：NIF_SHOWTIP 依赖 v4 版本，必须等 NIM_SETVERSION 成功后才能使用，
    // 因此这里不能带，否则首次 NIM_ADD 会被判定为参数无效。
    data.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    data.uCallbackMessage = callbackMsg_;
    data.hIcon            = icon_;
    wcsncpy_s(data.szTip, tooltip_, _TRUNCATE);
}

void TrayIcon::Remove() {
    if (!added_ || !owner_) return;

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd   = owner_;
    data.uID    = kIconId;
    Shell_NotifyIconW(NIM_DELETE, &data);
    added_ = false;
}

bool TrayIcon::ReplaceIcon(HICON icon) {
    if (!icon) return false;

    icon_     = icon;
    ownsIcon_ = false;

    if (!added_ || !owner_) return false;

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd   = owner_;
    data.uID    = kIconId;
    data.uFlags = NIF_ICON;
    data.hIcon  = icon_;

    return Shell_NotifyIconW(NIM_MODIFY, &data) != FALSE;
}

void TrayIcon::SetTooltip(const wchar_t* tooltip) {
    if (!tooltip) return;
    wcsncpy_s(tooltip_, 128, tooltip, _TRUNCATE);

    if (!added_ || !owner_) return;

    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd   = owner_;
    data.uID    = kIconId;
    data.uFlags = NIF_TIP;
    wcsncpy_s(data.szTip, tooltip_, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}