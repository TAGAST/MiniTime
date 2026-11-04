#pragma once

#include <windows.h>

// 全局热键注册。
// 不持有窗口以外的东西，不参与渲染。
class HotkeyRegistry {
public:
    HotkeyRegistry() = default;
    ~HotkeyRegistry();

    HotkeyRegistry(const HotkeyRegistry&) = delete;
    HotkeyRegistry& operator=(const HotkeyRegistry&) = delete;

    // 返回 false 表示被其它程序占用
    bool Register(HWND hwnd, UINT modifiers, UINT virtualKey);
    void UnregisterAll();

    bool IsRegistered() const { return registered_; }

private:
    HWND hwnd_       = nullptr;
    int  id_         = 0;
    bool registered_ = false;
};
