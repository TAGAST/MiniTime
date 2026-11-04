#include "autostart.h"

#include <windows.h>
#include <string>

namespace {

constexpr const wchar_t* kRunKeyPath =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kValueName = L"MiniTime";

std::wstring CurrentExecutablePath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH * 2);
    if (length == 0 || length >= MAX_PATH * 2) return std::wstring();
    return std::wstring(buffer, length);
}

// 写入注册表的值需要带引号，避免路径含空格时被截断
std::wstring QuotedCommand() {
    std::wstring path = CurrentExecutablePath();
    if (path.empty()) return std::wstring();
    return L"\"" + path + L"\" --autostart";
}

std::wstring ReadValue() {
    wchar_t buffer[MAX_PATH * 4] = {};
    DWORD size = sizeof(buffer);
    HKEY key = nullptr;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return std::wstring();
    }

    const LONG result = RegQueryValueExW(key, kValueName, nullptr, nullptr,
                                         reinterpret_cast<LPBYTE>(buffer), &size);
    RegCloseKey(key);

    if (result != ERROR_SUCCESS) return std::wstring();
    return std::wstring(buffer);
}

} // namespace

namespace autostart {

bool IsEnabled() {
    return !ReadValue().empty();
}

bool SetEnabled(bool enabled) {
    HKEY key = nullptr;
    const LONG openResult = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKeyPath, 0, nullptr,
                                            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE,
                                            nullptr, &key, nullptr);
    if (openResult != ERROR_SUCCESS) return false;

    bool ok = false;
    if (enabled) {
        const std::wstring command = QuotedCommand();
        if (!command.empty()) {
            ok = RegSetValueExW(key, kValueName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
        }
    } else {
        const LONG del = RegDeleteValueW(key, kValueName);
        // 值本来就不存在也视为成功
        ok = (del == ERROR_SUCCESS || del == ERROR_FILE_NOT_FOUND);
    }

    RegCloseKey(key);
    return ok;
}

void SyncPathIfEnabled() {
    const std::wstring existing = ReadValue();
    if (existing.empty()) return;

    const std::wstring expected = QuotedCommand();
    if (expected.empty()) return;

    // 程序被移动过则修正路径
    if (existing != expected) {
        SetEnabled(true);
    }
}

} // namespace autostart