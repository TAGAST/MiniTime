#include "game_profile.h"

#include <algorithm>
#include <cwctype>

namespace {

std::wstring ToLower(std::wstring s) {
    for (auto& ch : s) ch = static_cast<wchar_t>(std::towlower(ch));
    return s;
}

} // namespace

namespace gameprofile {

std::wstring NormalizeProcessName(const std::wstring& raw) {
    if (raw.empty()) return std::wstring();

    // 只取文件名部分（可能传入完整路径）
    std::wstring name = raw;
    const size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        name = name.substr(slash + 1);
    }

    // 统一小写，便于不区分大小写比较
    return ToLower(name);
}

std::wstring ToDisplayName(const std::wstring& processName) {
    std::wstring name = processName;

    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos && dot > 0) {
        name = name.substr(0, dot);
    }

    if (name.empty()) return processName;
    return name;
}

bool SameProcess(const std::wstring& a, const std::wstring& b) {
    return NormalizeProcessName(a) == NormalizeProcessName(b);
}

ForegroundInfo QueryForeground() {
    ForegroundInfo info;

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return info;

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) return info;

    // 进程名
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process) {
        wchar_t path[MAX_PATH * 2] = {};
        DWORD size = MAX_PATH * 2;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            info.processName = path;
        }
        CloseHandle(process);
    }

    if (info.processName.empty()) return info;

    // 窗口标题
    wchar_t title[512] = {};
    GetWindowTextW(hwnd, title, 512);
    info.windowTitle = title;

    info.valid = true;
    return info;
}

} // namespace gameprofile
