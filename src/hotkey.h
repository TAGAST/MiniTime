#pragma once

#include "hotkey_key.h"

#include <windows.h>
#include <vector>

// 热键标识：每个标识对应一个功能
enum class HotkeyId : int {
    Toggle = 1,   // 显示 / 隐藏
    Edit   = 2,   // 调整位置
};

// 全局热键注册。
// 负责多个热键的生命周期，不参与渲染与业务逻辑。
class HotkeyRegistry {
public:
    HotkeyRegistry() = default;
    ~HotkeyRegistry();

    HotkeyRegistry(const HotkeyRegistry&) = delete;
    HotkeyRegistry& operator=(const HotkeyRegistry&) = delete;

    void SetWindow(HWND hwnd) { hwnd_ = hwnd; }

    // 注册（或替换）一个热键。返回 false 表示被其它程序占用或参数无效。
    bool Register(HotkeyId id, const HotkeySpec& spec);

    // 注销单个 / 全部
    void Unregister(HotkeyId id);
    void UnregisterAll();

    bool IsRegistered(HotkeyId id) const;

    // 把消息循环中的 wParam 还原成标识
    static bool FromMessageId(WPARAM wParam, HotkeyId& out);

private:
    static int ToNativeId(HotkeyId id) { return static_cast<int>(id); }

    struct Entry {
        HotkeyId id = HotkeyId::Toggle;
        bool     registered = false;
    };

    HWND hwnd_ = nullptr;
    std::vector<Entry> entries_;
};
