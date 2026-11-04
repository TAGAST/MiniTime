# MiniTime

在游戏画面上叠加显示当前时间的轻量工具。单文件、零依赖、点击穿透。

---

## 特性

| 特性 | 说明 |
|---|---|
| **点击穿透** | 鼠标点击、滚轮、拖拽全部落到游戏，零干扰 |
| **不抢焦点** | 点击不会让游戏失焦或最小化 |
| **逐像素透明** | 文字抗锯齿，无黑边框、无背景色块 |
| **始终置顶** | 覆盖在窗口化 / 无边框全屏的游戏画面之上 |
| **不进 Alt+Tab** | 工具窗口样式，不占任务栏 |
| **全局热键** | `Ctrl+Alt+T` 切换显示 / 隐藏 |
| **单文件** | 约 200 KB，无运行库依赖，拷走即用 |

---

## 快速开始

1. 从 [Releases](../../releases) 下载，或从 [Actions](../../actions) 取最新构建产物
2. 双击 `MiniTime.exe` 运行
3. 时间默认显示在屏幕**右上角**
4. 按 `Ctrl+Alt+T` 隐藏 / 显示

退出程序：在任务管理器中结束 `MiniTime.exe`。

> **注意**：当前版本**还没有托盘图标**，因此没有菜单入口，也没有"退出"按钮。
> 隐藏后只能再按 `Ctrl+Alt+T` 唤回。托盘与设置菜单在后续版本规划中。

---

## 显示模式支持

| 游戏显示模式 | 是否可见 |
|---|---|
| 窗口化 | 可以 |
| 无边框 / 全屏窗口化 | 可以（推荐） |
| 全屏独占 + 系统"全屏优化"开启 | 多数可以，因游戏而异 |
| **真独占全屏** | **不可见** |

真独占全屏下 DWM 合成被绕过，任何普通置顶窗口都会被遮挡。要在该模式下叠加，必须向游戏进程注入 DLL 并挂钩 DXGI Present —— 这会带来**反作弊误判风险**，与"轻量 + 安全"目标冲突，因此**本项目明确不做**。

**建议**：游戏内改为"无边框窗口化"。现代游戏中与独占全屏的输入延迟差异通常不足一帧。

---

## 构建

### 从源码构建（本地）

需要 [llvm-mingw](https://github.com/mstorsjo/llvm-mingw)（免安装，解压即用）。

1. 修改 `build.bat` 中的 `MINGW` 路径指向你的 llvm-mingw 目录
2. 运行：

```bat
build.bat
```

产物为 `build\MiniTime.exe`。

### CMake 构建

仓库提供 `CMakeLists.txt`，MSVC 与 llvm-mingw 均可：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 云端构建

推送代码或提交 PR 会触发 GitHub Actions：

- 自动编译（MSVC / windows-latest）
- **体积门禁**：产物超过 300 KB 直接构建失败
- 产物可在 Actions 页面的 Artifacts 中下载
- 推送 `v*` tag 自动创建 Release

---

## 项目结构

按单一职责划分，每个模块只做一件事：

```
src/
├─ main.cpp              进程入口，仅做初始化与委托
├─ app.cpp/.h            组合根：接线 + 消息分发
├─ clock_presenter.cpp   编排：取时 → 格式化 → 测量 → 绘制
├─ timefmt.cpp/.h        纯函数：时间 → 字符串
├─ layered_surface.cpp   分层窗口像素缓冲与提交
├─ gdi_text_painter.cpp  字体与文字绘制
├─ overlay_window.cpp    HWND 创建与窗口样式
├─ hotkey.cpp            全局热键注册
└─ renderer_types.h      样式与布局的值对象
```

依赖方向单向，不存在循环引用，也没有集中的"上帝类"。

---

## 实现要点

**为什么点击穿透和可拖动定位不能同时成立**

设置了 `WS_EX_TRANSPARENT` 的窗口收不到任何鼠标消息，因此无法拖动。当前版本取"穿透"，位置固定为右上角；自由拖动需要配合"编辑模式"（临时移除穿透样式）实现，属后续规划。

**为什么透明必须用 `UpdateLayeredWindow`**

`SetLayeredWindowAttributes` 只能做色键或整窗 alpha，文字边缘会出现锯齿和黑边。逐像素透明必须走 32bpp 预乘 ARGB 的 `UpdateLayeredWindow` 路径。

**为什么 GDI 绘制后要重算 alpha**

GDI 在 DIB 上绘制时不写 alpha 通道。本项目以纯白文字绘制于黑底，颜色值本身即为覆盖率，因此直接用亮度回填 alpha —— 结果天然是预乘格式。

**为什么不用全局键盘钩子**

`RegisterHotKey` 是系统登记的热键机制；`SetWindowsHookEx(WH_KEYBOARD_LL)` 是全局钩子，在反作弊眼中是红旗。本项目只用前者。

---

## 系统要求

- Windows 10 1809 及以上 / Windows 11
- x64
- 无需 .NET、无需 VC++ 运行库

---

## 许可

尚未指定许可证。
