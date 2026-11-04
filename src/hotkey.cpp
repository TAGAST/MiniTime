#include "hotkey.h"

namespace {

constexpr int kHotkeyId = 1;

} // namespace

HotkeyRegistry::~HotkeyRegistry() { UnregisterAll(); }

bool HotkeyRegistry::Register(HWND hwnd, UINT modifiers, UINT virtualKey) {
    if (!hwnd) return false;

    UnregisterAll();
    hwnd_ = hwnd;
    id_   = kHotkeyId;

    // MOD_NOREPEAT 防止长按连发
    registered_ = RegisterHotKey(hwnd, id_, modifiers | MOD_NOREPEAT, virtualKey) != FALSE;
    return registered_;
}

void HotkeyRegistry::UnregisterAll() {
    if (registered_ && hwnd_) {
        UnregisterHotKey(hwnd_, id_);
    }
    registered_ = false;
}
