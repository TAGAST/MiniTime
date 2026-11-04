# MiniTime

在游戏画面上叠加显示当前时间的轻量工具。

---

## 特性

| 特性 | 说明 |
|---|---|
| **点击穿透** | 鼠标点击、滚轮、拖拽全部落到游戏，零干扰 |
| **不抢焦点** | 点击不会让游戏失焦或最小化 |
| **逐像素透明** | 文字抗锯齿，无黑边框、无背景色块 |
| **始终置顶** | 覆盖在窗口化 / 无边框全屏的游戏画面之上 |
| **不进 Alt+Tab** | 工具窗口样式，不占任务栏 |
| **自定义热键** | 托盘菜单可选预设，或编辑配置文件任意指定 |
| **全局热键** | 默认 `Ctrl+Alt+T` 切换显示，`Ctrl+Alt+E` 调整位置 |
| **系统托盘** | 常驻托盘，右键菜单开关显示、调整样式与位置、退出 |
| **样式自定义** | 整体缩放、字重、预设 5 色，即时生效 |
| **位置自定义** | 九宫格锚点（上/中/下 × 左/中/右） |
| **配置持久化** | 便携式 `config.ini`，重启后保持 |
| **开机自启** | 写 `HKCU\...\Run`，无需管理员权限 |
| **单文件** | 约 240 KB，无运行库依赖，拷走即用 |

### 托盘菜单

```
MiniTime · 显示中          ← 状态行
────────────────
隐藏              Ctrl+Alt+T
────────────────
样式 ▸   大小 ▸    75% / 100% / 125% / 150% / 175%
         字重 ▸    常规 / 中等 / 粗体
         颜色 ▸    白 / 米白 / 亮黄 / 青 / 绿
────────────────
位置 ▸   上左  上中  上右
         中左  居中  中右
         下左  下中  下右
────────────────
开机自启          ☐
────────────────
退出
```

- **热键** ▸ 显示/隐藏、调整位置 —— 各 5 组预设 + 禁用
- 菜单项会根据当前状态变化（显示中文案变为"隐藏"，反之亦然）
- **大小**为整体缩放：字号与内边距同比变化
- 样式与位置修改后**立即生效并写入配置文件**，重启保持
- "开机自启"为勾选项，切换后立即写入注册表
- explorer.exe 重启后，托盘图标会自动恢复

---

## 配置文件

优先保存到 exe 同目录的 `config.ini`（便携模式）；若目录不可写（如 Program Files），
回退到 `%APPDATA%\MiniTime\config.ini`。

```ini
[style]
scale = 100          ; 整体缩放百分比
font_weight = 600    ; 400 常规 / 600 中等 / 700 粗体
text_color = #FFFFFF ; RRGGBB

[position]
anchor = top-right   ; top-left / top-center / top-right
                     ; middle-left / center / middle-right
                     ; bottom-left / bottom-center / bottom-right
                     ; custom（自由拖动后自动切换）
margin_x = 24
margin_y = 20

[hotkey]
toggle = Ctrl+Alt+T  ; none 表示禁用
edit   = Ctrl+Alt+E
```

### 热键写法

`Ctrl` / `Alt` / `Shift` / `Win` 任意组合，加字母、数字或 `F1`-`F24`。
大小写不敏感，修饰键顺序无关。

```ini
toggle = Ctrl+Shift+T
toggle = F9            ; 功能键可以不加修饰
toggle = none          ; 禁用
```

普通字母键**必须带至少一个修饰键**，否则会吞掉游戏内该键的输入。
无效写法会自动回退到默认值，不会导致程序无法启动。

直接编辑该文件也生效，重启程序后读取。

---

## 显示模式支持

| 游戏显示模式 | 是否可见 |
|---|---|
| 窗口化 | 可以 |
| 无边框 / 全屏窗口化 | 可以（推荐） |
| 全屏独占 + 系统"全屏优化"开启 | 多数可以|
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
├─ hotkey.cpp            全局热键注册（多热键管理）
├─ hotkey_key.cpp        热键值对象与字符串互转
├─ tray_icon.cpp         Shell_NotifyIcon 封装
├─ tray_menu.cpp         右键菜单构建与命令返回
├─ autostart.cpp         开机自启注册表读写
├─ config.cpp            INI 配置读写与命令应用
├─ menu_commands.h       菜单命令枚举与锚点定义
└─ renderer_types.h      样式与布局的值对象
```

依赖方向单向，不存在循环引用，也没有集中的"上帝类"。

---

## 系统要求

- Windows 10 1809 及以上 / Windows 11
- x64

---

## 许可

尚未指定许可证。
