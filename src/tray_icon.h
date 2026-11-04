#pragma once

#include <windows.h>
#include <shellapi.h>

// 封装托盘图标的添加/更新/移除。
// 不关心点击后要做什么，只负责图标本身的存在与状态文本。
class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // 回调消息由外部注册的自定义消息驱动
    bool Add(HWND owner, UINT callbackMessage, HICON icon, const wchar_t* tooltip);
    void Remove();

    // 重新添加（如 explorer.exe 重启后）
    bool ReAdd();

    void SetTooltip(const wchar_t* tooltip);

    bool IsAdded() const { return added_; }
    HICON Icon() const { return icon_; }

    // 用于 TaskbarCreated 广播消息
    UINT TaskbarCreatedMessage() const { return taskbarCreatedMsg_; }

private:
    void FillData(NOTIFYICONDATAW& data) const;

    HWND   owner_        = nullptr;
    UINT   callbackMsg_  = 0;
    HICON  icon_         = nullptr;
    bool   added_        = false;
    bool   ownsIcon_     = false;
    wchar_t tooltip_[128] = {};
    UINT   taskbarCreatedMsg_ = 0;
};