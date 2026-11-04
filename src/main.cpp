#include "app.h"

#include <windows.h>

// 进程入口：只做初始化与委托，不含任何业务逻辑。
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    App app;
    return app.Run(instance, showCommand);
}
