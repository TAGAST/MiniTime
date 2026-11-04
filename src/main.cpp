#include "app.h"

#include <windows.h>

// 进程入口：只做初始化与委托，不含任何业务逻辑。
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 单实例：多开会导致托盘图标互相覆盖、热键重复注册、配置并发写入。
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"MiniTime.SingleInstance");
    if (!mutex) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // 已有实例在运行：通知它显示出来，然后本进程直接退出
        HWND existing = FindWindowW(L"MiniTimeRouter", nullptr);
        if (existing) {
            PostMessageW(existing, WM_APP + 1, 0, 0);
        }
        CloseHandle(mutex);
        return 0;
    }

    App app;
    const int result = app.Run(instance, showCommand);

    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return result;
}
