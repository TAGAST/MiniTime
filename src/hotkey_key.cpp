#include "hotkey_key.h"

#include <cwctype>
#include <vector>

namespace {

constexpr UINT kModifierMask = MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN;

std::wstring ToLower(std::wstring s) {
    for (auto& ch : s) ch = static_cast<wchar_t>(std::towlower(ch));
    return s;
}

std::vector<std::wstring> Split(const std::wstring& text, wchar_t sep) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == sep) {
            parts.push_back(current);
            current.clear();
        } else if (!std::iswspace(ch)) {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

// 解析主键名（字母 / 数字 / F1..F24 / 常见符号）
UINT ParseVirtualKey(const std::wstring& name) {
    if (name.empty()) return 0;

    const std::wstring lower = ToLower(name);

    // F1..F24
    if (lower.size() >= 2 && lower[0] == L'f') {
        const int n = _wtoi(name.c_str() + 1);
        if (n >= 1 && n <= 24) return static_cast<UINT>(VK_F1 + n - 1);
        return 0;
    }

    // 单字符：字母 / 数字
    if (name.size() == 1) {
        const wchar_t ch = name[0];
        if (std::iswalnum(ch)) {
            return static_cast<UINT>(std::towupper(ch));
        }
        return 0;
    }

    // 少数常用命名键
    if (lower == L"space")  return VK_SPACE;
    if (lower == L"tab")    return VK_TAB;
    if (lower == L"pause")  return VK_PAUSE;
    if (lower == L"home")   return VK_HOME;
    if (lower == L"end")    return VK_END;
    if (lower == L"insert") return VK_INSERT;

    return 0;
}

std::wstring VirtualKeyToName(UINT vk) {
    if (vk >= VK_F1 && vk <= VK_F24) {
        wchar_t buf[8] = {};
        swprintf(buf, 8, L"F%u", vk - VK_F1 + 1);
        return buf;
    }

    if (vk == VK_SPACE)  return L"Space";
    if (vk == VK_TAB)    return L"Tab";
    if (vk == VK_PAUSE)  return L"Pause";
    if (vk == VK_HOME)   return L"Home";
    if (vk == VK_END)    return L"End";
    if (vk == VK_INSERT) return L"Insert";

    if (vk >= L'0' && vk <= L'9') return std::wstring(1, static_cast<wchar_t>(vk));
    if (vk >= L'A' && vk <= L'Z') return std::wstring(1, static_cast<wchar_t>(vk));

    wchar_t buf[8] = {};
    swprintf(buf, 8, L"0x%02X", vk);
    return buf;
}

} // namespace

std::wstring HotkeySpec::ToString() const {
    if (IsEmpty()) return L"none";

    std::wstring result;
    if (modifiers & MOD_CONTROL) result += L"Ctrl+";
    if (modifiers & MOD_ALT)     result += L"Alt+";
    if (modifiers & MOD_SHIFT)   result += L"Shift+";
    if (modifiers & MOD_WIN)     result += L"Win+";

    result += VirtualKeyToName(virtualKey);
    return result;
}

namespace hotkey {

std::wstring NormalizeText(const std::wstring& text) {
    const HotkeySpec spec = Parse(text);
    return spec.ToString();
}

HotkeySpec Parse(const std::wstring& text) {
    HotkeySpec spec;  // 默认即"未设置"

    if (text.empty()) return spec;

    const std::wstring trimmed = ToLower(text);
    if (trimmed == L"none" || trimmed == L"禁用" || trimmed == L"off") {
        return spec;  // 明确禁用
    }

    const std::vector<std::wstring> parts = Split(text, L'+');
    if (parts.empty()) return spec;

    UINT modifiers = 0;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        const std::wstring token = ToLower(parts[i]);
        if (token == L"ctrl" || token == L"control") modifiers |= MOD_CONTROL;
        else if (token == L"alt")   modifiers |= MOD_ALT;
        else if (token == L"shift") modifiers |= MOD_SHIFT;
        else if (token == L"win" || token == L"winkey") modifiers |= MOD_WIN;
        else return spec;  // 未知修饰键
    }

    const UINT vk = ParseVirtualKey(parts.back());
    if (vk == 0) return spec;

    // 功能键允许无修饰（F1..F24），普通字母需至少一个修饰键，
    // 否则会吞掉游戏内所有该键的输入。
    const bool isFunctionKey = (vk >= VK_F1 && vk <= VK_F24);
    if (!isFunctionKey && modifiers == 0) return spec;

    spec.modifiers = modifiers & kModifierMask;
    spec.virtualKey = vk;
    return spec;
}

HotkeySpec DefaultToggle() {
    return HotkeySpec::Make(MOD_CONTROL | MOD_ALT, 'T');
}

HotkeySpec DefaultEdit() {
    return HotkeySpec::Make(MOD_CONTROL | MOD_ALT, 'E');
}

} // namespace hotkey