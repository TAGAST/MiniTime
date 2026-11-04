#include "hotkey.h"

HotkeyRegistry::~HotkeyRegistry() { UnregisterAll(); }

bool HotkeyRegistry::Register(HotkeyId id, const HotkeySpec& spec) {
    if (!hwnd_) return false;

    // 同一标识重复注册：先注销旧的
    Unregister(id);

    // 空热键表示禁用
    if (spec.IsEmpty()) return true;

    // MOD_NOREPEAT 防止长按连发
    const BOOL ok = RegisterHotKey(hwnd_, ToNativeId(id),
                                   spec.modifiers | MOD_NOREPEAT, spec.virtualKey);

    const bool conflicted = !ok;

    bool found = false;
    for (auto& entry : entries_) {
        if (entry.id == id) {
            entry.registered = (ok != FALSE);
            entry.conflicted = conflicted;
            found = true;
            break;
        }
    }
    if (!found) {
        entries_.push_back(Entry{ id, ok != FALSE, conflicted });
    }

    return ok != FALSE;
}

bool HotkeyRegistry::Conflicted(HotkeyId id) const {
    for (const auto& entry : entries_) {
        if (entry.id == id) return entry.conflicted;
    }
    return false;
}

bool HotkeyRegistry::IsAvailable(const HotkeySpec& spec) {
    if (spec.IsEmpty()) return true;

    // 用临时窗口探测，探测后立即释放，不影响本进程已注册的热键
    HWND probe = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
                                 nullptr, nullptr, nullptr, nullptr);
    if (!probe) return true;   // 探测失败时不要误判为冲突

    constexpr int kProbeId = 0x4D54;   // 任意不冲突的局部 ID
    const BOOL ok = RegisterHotKey(probe, kProbeId,
                                   spec.modifiers | MOD_NOREPEAT, spec.virtualKey);
    if (ok) {
        UnregisterHotKey(probe, kProbeId);
    }
    DestroyWindow(probe);

    return ok != FALSE;
}

void HotkeyRegistry::Unregister(HotkeyId id) {
    if (!hwnd_) return;

    bool wasRegistered = false;
    for (auto& entry : entries_) {
        if (entry.id == id) {
            wasRegistered = entry.registered;
            entry.registered = false;
            break;
        }
    }

    if (wasRegistered) {
        UnregisterHotKey(hwnd_, ToNativeId(id));
    }
}

void HotkeyRegistry::UnregisterAll() {
    if (!hwnd_) {
        entries_.clear();
        return;
    }

    for (auto& entry : entries_) {
        if (entry.registered) {
            UnregisterHotKey(hwnd_, ToNativeId(entry.id));
            entry.registered = false;
        }
    }
    entries_.clear();
}

bool HotkeyRegistry::IsRegistered(HotkeyId id) const {
    for (const auto& entry : entries_) {
        if (entry.id == id) return entry.registered;
    }
    return false;
}

bool HotkeyRegistry::FromMessageId(WPARAM wParam, HotkeyId& out) {
    const int value = static_cast<int>(wParam);
    if (value == static_cast<int>(HotkeyId::Toggle) ||
        value == static_cast<int>(HotkeyId::Edit)) {
        out = static_cast<HotkeyId>(value);
        return true;
    }
    return false;
}