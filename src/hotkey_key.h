#pragma once

#include <windows.h>
#include <string>

// 一个全局热键的值表示：修饰键 + 虚拟键码。
// 与人类可读字符串互转，例如 "Ctrl+Alt+T"。
// 纯数据 + 纯函数，不涉及注册与窗口。
struct HotkeySpec {
    // 注意：默认构造必须表示"未设置/禁用"，不能给成某个具体热键，
    // 否则 HotkeySpec{} 会被误当成有效的 Ctrl+Alt+T。
    UINT modifiers = 0;
    UINT virtualKey = 0;

    bool IsEmpty() const { return virtualKey == 0; }

    static HotkeySpec Make(UINT mods, UINT vk) {
        HotkeySpec spec;
        spec.modifiers = mods;
        spec.virtualKey = vk;
        return spec;
    }

    // 与字符串互转
    std::wstring ToString() const;
};

namespace hotkey {

// 解析 "Ctrl+Alt+T" 这类写法；失败返回 virtualKey == 0 的无效值。
// 大小写不敏感，修饰键顺序无关。
HotkeySpec Parse(const std::wstring& text);

// 默认值
HotkeySpec DefaultToggle();
HotkeySpec DefaultEdit();

// 键名规范化：单个字符转大写，F1..F24 保持大写
std::wstring NormalizeText(const std::wstring& text);

} // namespace hotkey