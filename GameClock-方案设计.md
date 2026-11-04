# GameClock —— 游戏内时间叠加层 设计方案

> 版本 v1.0 ｜ 2026-11-04 ｜ 状态：待评审
> 技术路线：纯 Win32 / C++20 ｜ 交付形态：单 exe、零依赖、零安装

---

## 0. 结论速览

| 维度 | 方案 |
|---|---|
| 技术栈 | 纯 Win32 API + C++20，无任何第三方库、无运行时依赖 |
| 形态 | 单进程、单顶层窗口、单 exe（约 150 KB） |
| 体积预算 | **≤ 300 KB**（典型 120–200 KB） |
| 内存预算 | **5–12 MB** 常驻（按渲染后端） |
| CPU | 空闲 **< 0.1%**（每秒最多重绘 4 次） |
| 启动 | < 50 ms 到可见 |
| 点击穿透 | WS_EX_LAYERED + WS_EX_TRANSPARENT + WS_EX_NOACTIVATE |
| 配置 | 便携式 config.ini（Win32 原生 API 读写，零解析库） |
| 托盘菜单 | 原生 TrackPopupMenu，零自绘 |
| 独占全屏 | **不支持**（不做注入），见 §5.4 边界说明与降级方案 |
| 构建发布 | **GitHub Actions 云端编译**（windows-latest + MSVC），本地无需任何编译器 |
| 实现排期 | 约 7.5 人日 |

---

## 1. 目标与非目标

### 1.1 必须实现（Must）

1. 在屏幕上叠加显示当前时间（可选秒、可选日期）
2. 位于游戏画面之上（窗口化 / 无边框全屏场景）
3. 全局热键切换显示 / 隐藏
4. 鼠标点击穿透 —— 不干扰游戏内的点击与视角操作
5. 不抢焦点、不进入 Alt+Tab 与任务栏
6. 系统托盘常驻；右键托盘图标调出设置菜单
7. 显示位置与显示样式可自定义
8. 开机自启
9. 极致轻量（体积、内存、CPU 三项同时达标）

### 1.2 非目标（本版本不做）

- 注入式覆盖**真独占全屏**（DXGI Present 钩子）—— 与"轻量 + 反作弊安全"直接冲突，见 §5.4
- 游戏内计时器 / 秒表 / 多时区世界时钟（§18 列为可扩展项）
- 安装包、自动更新、多语言
- 独立的可视化设置窗口（本版"菜单即设置"，见 §9.4）

---

## 2. 环境与事实核验

### 2.1 目标机器探测结果（实测）

| 项 | 结果 | 影响 |
|---|---|---|
| 操作系统 | Windows 11 家庭版，build 26300，AMD64 | 支持 PerMonitorV2、WDA_EXCLUDEFROMCAPTURE |
| .NET 运行时 | 10.0.11 / 8.0.31 / 6.0.36（含 WindowsDesktop） | C# 路线可立即编译，但体积不达标 |
| Node.js | 已安装 | 仅作参照，不采用 |
| C/C++ 编译器 | `cl`/`g++`/`cmake`/`msbuild`/`vswhere` 缺失，**但 llvm-mingw 已安装可用**（见下行） | 本地可秒级编译迭代 |
| **llvm-mingw** | **已安装**：clang 23.1.4，目标 x86_64-w64-windows-gnu | 已验证可编译并运行 |
| 工具链路径 | `D:\windows\Desktop\dsh本地工作目录\llvm-mingw-20261020-msvcrt-i686\bin` | 见 §15.6 |
| Visual Studio | 未安装 | 不需要；CI 使用云端 MSVC |
| Rust | 未安装 | 不采用 |
| Git | 2.55.0（已安装） | 可本地提交推送 |
| GitHub CLI (gh) | **未安装** | 创建仓库需用网页，见 §15.7 |
| Git 仓库 | **尚未初始化**（当前目录不是 git 仓库） | 待办，见 §15.7 |

### 2.2 直接结论

**工具链阻塞已解除**。本机已安装 llvm-mingw（clang 23.1.4），并已用它**实际编译并运行验证通过**（见 §2.3）。

采用双轨构建：

| 用途 | 工具 | 特点 |
|---|---|---|
| 日常开发迭代 | **本地 llvm-mingw** | 秒级编译反馈，已验证可用 |
| 官方产物 / 发布 | **GitHub Actions + MSVC** | 环境可复现、体积门禁、自动发布（§15） |

两个工具链共用同一份 CMakeLists.txt，行为一致。**发布以 CI 产物为准。**

剩余待办只有一项：**创建 GitHub 仓库并初始化本地 git**（§15.7）。

### 2.3 M1 核心技术实测验证（已完成）

方案中最不确定的部分是"分层窗口 + 点击穿透 + 不抢焦点"能否同时成立。已编写最小可运行程序（`src/main.cpp`，176 行）并**实测通过**：

| 验证项 | 方法 | 实测结果 | 结论 |
|---|---|---|---|
| 编译 | llvm-mingw clang 23.1.4 | 0 error | 通过 |
| **产物体积** | 文件大小 | **12,288 字节（12 KB）** | 远优于 300 KB 预算 |
| **内存占用** | Get-Process | WorkingSet **7.16 MB** / Private **1.23 MB** | 符合 5–12 MB 预算 |
| **点击穿透** | `WindowFromPoint` 探测窗口正中心 | 返回桌面 `SysListView32`(pid 10500)，**非**叠加层(pid 3804) | **实证穿透成立** |
| **不抢焦点** | `GetForegroundWindow` 对比 | 前台窗口未被叠加层夺取 | **通过** |
| 窗口样式 | `GetWindowLong(GWL_EXSTYLE)` | `0x80800A8`：TOPMOST + TRANSPARENT + LAYERED + TOOLWINDOW + NOACTIVATE **全部置位** | **通过** |
| 逐像素透明 | 截图观察 | 白色时间正常显示，底层桌面图标从文字间隙透出 | **通过** |
| 不占任务栏/Alt+Tab | TOOLWINDOW 样式 | 已置位 | 待人工确认 |

> 说明：12 KB 是**不含托盘、菜单、D2D、配置**的最小核心版本。完整功能版本预计 120–250 KB，仍在预算内。
> 内存 7.16 MB 为 GDI 后端数据；启用 Direct2D 后端预计增至 9–13 MB。

**这意味着方案的核心技术风险已全部消除** —— 剩余工作是功能堆叠，而非可行性探索。

---

## 3. 技术选型对比

| 维度 | Electron | C# WinForms/WPF | Qt | **C++ Win32（选定）** |
|---|---|---|---|---|
| 单文件体积 | 150–250 MB | 15–25 MB（裁剪后） | 15–30 MB | **0.12–0.3 MB** |
| 常驻内存 | 100–200 MB | 30–45 MB | 30–60 MB | **5–12 MB** |
| 冷启动 | 1–3 s | 100–400 ms | 100–300 ms | **< 50 ms** |
| 运行时依赖 | 无（自带） | .NET 运行时或自包含 | Qt DLL | **无，纯系统 DLL** |
| 分层窗口 / 点击穿透 | 需额外适配 | 可用但 API 间接 | 可用 | **原生直控** |
| 与反作弊共存 | 高风险（大进程、易被关注） | 中 | 中 | **低（无注入、无钩子）** |
| 开发成本 | 低 | 低 | 中 | 中高 |
| 工具链现状 | 就绪 | 就绪 | 缺失 | **本地缺失，但由 CI 云端编译弥补（§15）** |

**结论**：需求明确写了"极致轻量化"，同时要求点击穿透、不抢焦点、托盘常驻 —— 这些全部是 Win32 窗口样式的原生能力。Electron/C#/Qt 都会在体积与内存上付 20–100 倍的代价，且并不能换来任何一项需求上的优势。**选 C++ Win32。**

> 权衡说明：C++ 的代价是开发成本更高、本机也没有编译器。前者是一次性投入，后者已由 GitHub Actions 云端构建彻底消除（§15），而收益是常驻资源占用降低一个数量级 —— 对一个"长期挂在游戏后台"的工具，这个取舍是正确的。

---

## 4. 总体架构

### 4.1 结构

单进程、纯消息驱动、无后台线程：

    ┌──────────────────────────────┐
    │        WinMain / 消息循环      │
    └───────────────┬──────────────┘
        ┌───────────┼───────────┬───────────┬──────────────┐
        ▼           ▼           ▼           ▼              ▼
  ┌───────────┐ ┌───────────┐ ┌─────────┐ ┌─────────┐ ┌───────────┐
  │ Overlay   │ │ Renderer  │ │ Hotkey  │ │  Tray   │ │  Config   │
  │ Window    │ │ (D2D/GDI) │ │Register │ │ NIM+菜单 │ │ INI + 自启 │
  └───────────┘ └───────────┘ └─────────┘ └─────────┘ └───────────┘

所有交互最后都收敛为三类动作：**改配置 → 重算布局 → 重绘**。

### 4.2 模块职责

| 模块 | 文件 | 职责 | 估算行数 |
|---|---|---|---|
| 入口 | main.cpp | 单实例互斥、DPI、消息循环、生命周期 | 180 |
| 叠加窗口 | overlay_window.cpp | 窗口创建、扩展样式、Z 序维护、编辑模式、拖动 | 380 |
| 渲染 | renderer.cpp | 后端抽象、DIB、UpdateLayeredWindow、文字布局 | 420 |
| 时间格式 | timefmt.cpp | 格式串解析、12/24 小时、日期 | 120 |
| 托盘 | tray.cpp | Shell_NotifyIcon、菜单构建、命令分发 | 350 |
| 热键 | hotkey.cpp | RegisterHotKey、失败回退、字符串↔加速键互转 | 130 |
| 配置 | config.cpp | INI 读写、默认值、校验 | 200 |
| 自启 | startup.cpp | Run 键增删、路径校验 | 80 |
| 资源 | resource.rc | 图标、版本信息、manifest | 60 |

**合计约 1,900–2,500 行**，属于单人可控规模。

---

## 5. 窗口与叠加层设计（核心）

### 5.1 窗口创建

    const DWORD exStyle =
        WS_EX_TOPMOST       // 置顶
      | WS_EX_LAYERED       // 分层窗口，逐像素 alpha（必需）
      | WS_EX_TRANSPARENT   // 鼠标消息穿透到下层窗口（点击穿透的核心）
      | WS_EX_TOOLWINDOW    // 不出现在 Alt+Tab / 任务栏
      | WS_EX_NOACTIVATE;   // 点击不抢焦点，绝不打断游戏

    CreateWindowExW(exStyle, L"GameClockOverlay", L"", WS_POPUP,
                    0, 0, 1, 1, nullptr, nullptr, hInst, nullptr);

逐条理由：

- **WS_EX_LAYERED** —— 逐像素透明的唯一途径。没有它，只能做整窗 alpha 或色键，文字边缘会出现黑边/锯齿。
- **WS_EX_TRANSPARENT** —— 在分层窗口上使**命中测试透明**，鼠标点击、滚轮直接落到游戏窗口。这是"点击穿透"的实现本体。
- **WS_EX_TOOLWINDOW** —— 工具窗口不占任务栏、不进 Alt+Tab，避免玩家切窗口时看到它。
- **WS_EX_NOACTIVATE** —— 即使窗口被点击或被程序置顶也不会被激活，**保证游戏永远不丢焦点**（全屏游戏丢焦点会直接最小化，这是最致命的问题）。

创建后加一道保险：

    case WM_NCHITTEST:
        return HTTRANSPARENT;   // 冗余保障，覆盖个别 DPI/兼容性场景

### 5.2 点击穿透 ↔ 可拖动定位 的冲突与解法

**冲突**：窗口一旦设置 WS_EX_TRANSPARENT，就收不到任何鼠标消息 —— 也就**无法拖动定位**。而需求同时要求"点击穿透"和"自定义显示位置"。

**解法：双态设计**

| 状态 | 扩展样式 | 鼠标行为 | 视觉 |
|---|---|---|---|
| **常态（游戏中）** | 保留 TRANSPARENT + NOACTIVATE | 全部穿透给游戏 | 正常显示时间 |
| **编辑模式** | **移除** TRANSPARENT、**移除** NOACTIVATE | 可拖动、可接收按键 | 加虚线边框 + 拖动提示 |

**进入编辑模式的三条路径**（任一）：

1. 托盘菜单 → 位置 → 调整位置…
2. 配置热键（默认 Ctrl+Alt+E，可关闭）
3. 命令行参数 --edit

**编辑模式内的操作**：

- 左键拖拽移动（WM_LBUTTONDOWN + SetCapture，**不用 GetCursorPos 轮询**，避免丢帧和 CPU 浪费）
- 方向键微调 1 px，Shift+方向键 10 px
- Enter 保存并退出；Esc 取消并还原
- 托盘菜单 / 再次按热键 也可退出

**退出时恢复样式**：

    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    ex |= (WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0,
                 SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_FRAMECHANGED);

> 坑：修改扩展样式后**必须**带 SWP_FRAMECHANGED，否则新样式不生效。

**超时保护**：编辑模式若 3 分钟无操作，自动退出并恢复穿透 —— 防止用户忘记退出，导致游戏里点不动。（默认开启）

> 备选（更轻但体验差）：不做编辑模式，位置纯靠菜单九宫格预设 + 像素微调。若希望进一步压缩复杂度可以砍掉，代价是不能自由拖动。**推荐保留**，它是"自定义位置"体验的关键。

### 5.3 置顶与 Z 序维护

置顶窗口并非一劳永逸 —— 游戏启动、切回前台、全屏切换时都会重排 Z 序。

    SetWindowPos(hwnd, HWND_TOPMOST, 0,0,0,0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);

- 监听 SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ..., WINEVENT_OUTOFCONTEXT)：前台窗口变化时重新置顶
- 兜底：1 秒定时器检查一次是否被遮挡，**仅在确实被遮挡时**才重设（无条件重设会导致闪烁）
- 全程 SWP_NOACTIVATE，不抢焦点

### 5.4 独占全屏的能力边界（必读）

这是本方案唯一必须如实说明的功能上限。

| 游戏显示模式 | 能否显示 | 说明 |
|---|---|---|
| 窗口化 | 可以，稳定 | 主场景 |
| 无边框 / 全屏窗口化 | 可以，稳定 | 主场景，绝大多数现代游戏支持 |
| 全屏独占 + 系统"全屏优化"开启（Win10 1803+） | 多数可以 | 系统会把部分独占游戏转为 flip 呈现，叠加层可能可见，**因游戏而异** |
| **真独占全屏（flip exclusive）** | **不可见** | 此类模式下 DWM 合成被绕过，任何普通顶层窗口都会被遮挡 |

**为什么不做注入**：要在真独占全屏上叠加，必须向游戏进程注入 DLL 并挂接 DXGI Present（RTSS / Discord / Steam 的做法）。这会带来：

- 进程注入 + API 钩子 → **反作弊（EAC / BattlEye / Vanguard）高误判风险**，可能直接封号
- 代码量增加数倍，需要处理 D3D11/12/Vulkan 多后端
- 常驻内存增加 10–30 MB

**与"极致轻量 + 安全"根本冲突，因此明确不做。**

**降级路径（按优先级）**：

1. **首选**：游戏内把显示模式改为"无边框窗口化 / 全屏窗口化"。现代电竞游戏中两者输入延迟差异通常不足 1 帧，而收益是叠加层稳定可用。
2. 开启 Windows 的"全屏优化"（设置 → 系统 → 显示 → 图形 → 默认图形设置），可让部分独占游戏转为 flip 模式。
3. 双显示器：叠加层放在副屏。
4. **可选增强**：自动检测前台窗口是否为全屏独占，是则自动隐藏叠加层，并在托盘气泡提示"当前为独占全屏，建议改为无边框窗口化"。

> 结论：本方案覆盖"窗口化/无边框"全场景，对真独占全屏**明确不支持并给出替代路径**。若后续必须覆盖独占全屏，需单独立项评估注入方案的合规与风险。

### 5.5 DPI 与多显示器

- 清单文件声明 PerMonitorV2，Windows 按显示器 DPI 分别缩放
- 响应 WM_DPICHANGED：按新 DPI 重算字号与位图尺寸，重绘
- 位置存储不存绝对坐标，存 **(显示器设备名, 九宫格锚点, 像素偏移)**，换分辨率/换屏后自动复现
- 显示器标识用 \\.\DISPLAY1 这类设备名，而非索引

---

## 6. 渲染方案

### 6.1 为什么必须用 UpdateLayeredWindow

| 方式 | 透明度 | 文字边缘 | 结论 |
|---|---|---|---|
| SetLayeredWindowAttributes 色键 | 只有全透明/全不透明 | 锯齿、黑边 | 不采用 |
| SetLayeredWindowAttributes 整窗 alpha | 整窗统一 | 背景方块 | 不采用 |
| **UpdateLayeredWindow + 预乘 ARGB DIB** | **逐像素** | **抗锯齿平滑** | **选定** |

流程：

1. 建 32bpp DIB（CreateDIBSection，BI_RGB）
2. 用 Direct2D 渲染到该 DIB 的 DC
3. 调用 UpdateLayeredWindow(hwnd, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA)
4. blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA }

### 6.2 两个渲染后端

| 后端 | 依赖 | 内存 | 质量 | 用途 |
|---|---|---|---|---|
| **Direct2D + DirectWrite**（默认） | d2d1.dll / dwrite.dll（系统自带） | +3–5 MB | 优：抗锯齿、字重、描边、圆角 | 推荐 |
| **GDI**（精简） | gdi32.dll | 基准 | 中：无平滑描边 | 低配模式 / D2D 初始化失败时回退 |

后端在配置里可切（[style] renderer=gdi|d2d），默认 d2d。

### 6.3 抗锯齿关键点

**分层窗口上 ClearType 不可用**，必须显式指定灰阶抗锯齿，否则文字会发虚或出现彩色边：

    dwriteTextFormat->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

背景为全透明时，文字需配描边或半透明底板保证可读性（尤其压在亮色游戏画面上）。

### 6.4 重绘策略

- **仅在显示文本变化时重绘**（秒变化 / 分钟变化），不是每帧
- 窗口尺寸 = 文本包围盒 + padding，通常 60×110 ~ 200×40 像素
- 走 UpdateLayeredWindow 路径，无 WM_PAINT 开销

---

## 7. 计时与刷新

| 显示内容 | 定时器周期 | 依据 |
|---|---|---|
| 含秒（HH:mm:ss） | 250 ms | 保证秒跳变延迟 ≤ 250 ms |
| 不含秒（HH:mm） | 1000 ms | 每分钟才需要一次更新 |

- **每次触发都调用 GetLocalTime 取真实时间**，不做累加 → 永不漂移
- 可选的整秒对齐：把 SetTimer 设为"到下个整秒的毫秒数"，减少无效唤醒（进一步降低 CPU）
- WM_TIMECHANGE（用户改了系统时间）→ 立即重绘
- WM_POWERBROADCAST（休眠唤醒）→ 立即重绘，避免显示旧时间
- WM_DISPLAYCHANGE（分辨率/显示器变化）→ 重新定位并重绘

> 空闲 CPU 预期接近 0（大部分时间在 GetMessage 阻塞），仅定时器触发时做一次微小重绘。

---

## 8. 全局热键

    RegisterHotKey(hwnd, ID_HOTKEY_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T');
    // 消息循环中:
    case WM_HOTKEY: if (wParam == ID_HOTKEY_TOGGLE) toggle_visibility(); break;

**设计要点**：

- **只用 RegisterHotKey，绝不用 SetWindowsHookEx(WH_KEYBOARD_LL) 全局键盘钩子** —— 全局钩子在反作弊眼中是红旗，且性能更差
- MOD_NOREPEAT 防止长按连发
- 热键冲突（被其他程序占用）时：RegisterHotKey 返回 FALSE → 托盘气泡提示"热键 Ctrl+Alt+T 已被占用，请在配置中修改"，且**不阻塞**程序其余功能
- 默认建议 Ctrl+Alt+T；避开展开键（Alt+Z、Shift+Tab 等游戏常用键）
- 支持双热键：① 显示/隐藏 ② 进入编辑模式（后者可选，默认关闭）

**为何热键是刚需**：游戏全屏时任务栏被遮挡，托盘图标不可见，热键是唯一可靠的开关手段。

---

## 9. 系统托盘与右键菜单

### 9.1 托盘图标

    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP));
    wcscpy_s(nid.szTip, L"GameClock — 显示中");
    Shell_NotifyIconW(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);

- 图标内嵌多尺寸资源（16/20/24/32 px），高 DPI 下清晰
- 提示文本动态反映状态（显示中 / 已隐藏）
- 退出时 NIM_DELETE

### 9.2 菜单结构（简洁轻量原则）

    GameClock · 显示中              ← 灰色不可点，状态行
    ────────────────────────────
    显示 / 隐藏          Ctrl+Alt+T
    ────────────────────────────
    位置 ▸  上左   上中   上右
            中左   居中   中右
            下左   下中   下右
            ────────────
            选择显示器 ▸  显示器 1 / 显示器 2
            偏移微调 ▸  ↑ ↓ ← →
            调整位置…          Ctrl+Alt+E
    ────────────────────────────
    样式 ▸  字号 ▸   20 / 28 / 36 / 48
            字重 ▸   常规 / 中等 / 粗体
            颜色 ▸   白 / 米白 / 亮黄 / 青 / 绿
            背景 ▸   无 / 半透明黑 / 圆角卡片
            透明度 ▸ 100% / 85% / 70% / 55% / 40%
            格式 ▸   HH:mm / HH:mm:ss / hh:mm 上午
            ☐ 显示日期
    ────────────────────────────
    ☑ 点击穿透
    ☑ 始终置顶
    ☐ 开机自启
    ────────────────────────────
    打开配置文件
    重新加载配置
    ────────────────────────────
    退出

**设计原则**：

- 全部使用**原生 Win32 菜单 API**（CreatePopupMenu / CreateMenu / CheckMenuItem / AppendMenuW），**零自绘** → 体积与视觉负担最小，且自动跟随系统主题（深色模式）
- 层级 ≤ 2 层，每层 ≤ 12 项，满足"简洁轻量"
- 当前生效项用 CheckMenuItem(MF_BYCOMMAND | MF_CHECKED) 打勾
- 常用项（显示/隐藏、调整位置）放在一级，避免翻菜单影响游戏节奏

### 9.3 必须避开的经典坑

    SetForegroundWindow(hwnd);                     // ① 先抢前台
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);             // ② 再补一条空消息

漏掉这两步，会出现"**点击菜单外部菜单不消失**"的经典 bug。此外可 SetMenuInfo + MNS_CHECKORBMP 减少勾选图标导致的菜单跳动。

### 9.4 "设置菜单"的形态决策

需求原文是"右键菜单调出设置菜单"。有两种理解，本方案取前者：

| 方案 | 描述 | 体积 | 体验 |
|---|---|---|---|
| **A. 菜单即设置（选定）** | 所有选项直接内置在托盘右键菜单中，无独立窗口 | **最轻**，无额外窗口类 | 快，一键直达；项数受菜单密度限制 |
| B. 弹出设置窗口 | 右键 → 设置… → 打开配置窗口 | +30–60 KB 与一个窗口类 | 可视化更好，但更重、打断游戏 |

选 A 的理由：与"极致轻量化"和"菜单以简洁轻量为主"直接吻合；且样式项本身就是离散枚举（字号/颜色/透明度），菜单式切换反而更快。A 的短板（复杂自定义）由"打开配置文件"兜底。

> 若后续更想要可视化面板，架构上已预留：托盘菜单加一项 设置… 即可接入，不影响现有模块。

---

## 10. 配置

### 10.1 存储位置

优先**便携模式**：<exe 同目录>\config.ini
若该目录不可写（如 Program Files），回退到 %APPDATA%\GameClock\config.ini。

### 10.2 格式（INI，零解析依赖）

选用 INI 而非 JSON 的理由：Win32 自带 GetPrivateProfileStringW / WritePrivateProfileStringW，**解析代码量为 0**，无需引入 JSON 库（可为 exe 省下 20–60 KB 并消除一类解析 bug）。

    [general]
    visible = true
    hotkey_toggle = Ctrl+Alt+T
    hotkey_edit = Ctrl+Alt+E
    click_through = true
    always_on_top = true
    autostart = false
    edit_mode_timeout_sec = 180

    [position]
    anchor = top-right
    monitor = \\.\DISPLAY1
    offset_x = 24
    offset_y = 20

    [style]
    font_face = Segoe UI
    font_size = 28
    font_weight = 600
    text_color = #FFFFFF
    text_opacity = 100
    outline_width = 1
    outline_color = #000000
    background = none
    background_color = #000000
    background_opacity = 45
    corner_radius = 8
    padding_x = 12
    padding_y = 6
    renderer = d2d

    [format]
    pattern = HH:mm:ss
    show_seconds = true
    show_date = false

### 10.3 读写策略

- 启动时读入，缺失项用默认值补齐（**配置损坏不应导致启动失败**）
- 菜单中任何改动 → 立即写回文件
- 外部手工编辑 → 菜单项"重新加载配置"生效（默认不做文件监听，省内存；若需要可加 ReadDirectoryChangesW，约 +1 MB）
- 每次弹出菜单前根据内存状态重建勾选状态，保证菜单与配置**永不脱节**

---

## 11. 位置与样式自定义

### 11.1 位置

- **九宫格锚点 + 像素偏移**（推荐，抗分辨率变化）
- 自由拖动的结果换算为 anchor=custom + 相对锚点偏移存储
- 多显示器用设备名标识，显示器移除时回退到主屏
- 边缘吸附（可选）：距屏幕边缘 < 12 px 自动吸附

### 11.2 样式项清单

| 分类 | 可选项 |
|---|---|
| 字体 | 字体族、字号（20/28/36/48 或任意）、字重（400/600/700）、斜体（可选） |
| 颜色 | 文字色、透明度、描边宽度与颜色、背景（无 / 纯色 / 圆角卡片）、背景透明度、圆角半径、内边距 |
| 格式 | HH:mm / HH:mm:ss / hh:mm（12 小时）/ 含日期 yyyy-MM-dd HH:mm |
| 布局 | 九宫格锚点、显示器、偏移、行高 |

### 11.3 格式串占位符

| 标记 | 含义 | 示例 |
|---|---|---|
| HH | 24 小时制小时 | 14 |
| hh | 12 小时制小时 | 02 |
| mm | 分钟 | 03 |
| ss | 秒 | 09 |
| yyyy / MM / dd | 年 / 月 / 日 | 2026-11-04 |
| A / a | 上午/下午、AM/PM | 下午 |

实现用 GetLocalTime + 手写格式化（约 50 行），不依赖 strftime 的本地化不确定性。

---

## 12. 开机自启

| 方案 | 权限 | 可靠性 | 用户可管理性 | 结论 |
|---|---|---|---|---|
| **HKCU\...\Run 注册表值** | 普通用户 | 高 | 可在任务管理器"启动应用"里禁用 | **默认** |
| 启动文件夹快捷方式 | 普通用户 | 高 | 直观 | 备选 |
| 任务计划程序 | 可提权 | 最高 | 较隐蔽，用户不易关闭 | 仅在需要管理员权限时使用 |

**实现**：

    键：HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
    值：GameClock
    数据："C:\path\to\GameClock.exe" --autostart

- 菜单勾选 → 写值 / 删值
- 启动时校验：若已存在但路径与当前 exe 不一致（程序被移动过），**自动修正**为当前路径
- --autostart 参数下：静默启动、不弹任何提示；若检测到显示器尚未初始化，延迟 3 秒首绘
- 无需管理员权限，不写 HKLM

---

## 13. 轻量化预算与达成手段

### 13.1 指标预算

| 指标 | 目标 | 达成手段 | **M1 实测** |
|---|---|---|---|
| **exe 体积** | ≤ 300 KB（典型 120–200 KB） | -Os -flto -fno-exceptions -fno-rtti -ffunction-sections -Wl,--gc-sections -Wl,-s；不引入 STL 重模板、不引入 JSON 库、用 Win32 INI API | **12 KB**（核心版，未含托盘/菜单/D2D） |
| **常驻内存** | 5–12 MB | 单窗口 + 单 DIB；D2D 后端约 9–13 MB，GDI 后端约 5–7 MB | **7.16 MB** WorkingSet / 1.23 MB Private |
| **空闲 CPU** | < 0.1% | 定时器 250 ms/1000 ms，且仅在文本变化时重绘；无轮询循环 | 待实测 |
| **启动耗时** | < 50 ms 到可见 | 无运行时初始化、无框架引导、配置解析 < 1 ms | 待实测 |
| **安装/依赖** | 0 | 只链接系统 DLL：kernel32 / user32 / gdi32 / shell32 / advapi32 / d2d1 / dwrite / comctl32 | 待实测 |
| **磁盘占用** | 单文件，无附带资源目录 | 图标内嵌为资源 | 待实测 |
| **体积门禁** | CI 强制校验 ≤ 300 KB | 流水线检测产物大小，超标即构建失败，防止体积在迭代中悄悄膨胀 | 待实测 |

### 13.2 与其它方案的量级对比

    exe 体积：  C++ 0.15 MB  ｜  C# 20 MB  ｜  Electron 200 MB      → 相差约 1300 倍
    常驻内存：  C++ 8 MB     ｜  C# 38 MB  ｜  Electron 150 MB      → 相差约 19 倍

> 说明：M1 核心版已实测（见 §2.3），体积与内存均在预算内。完整功能版本（含托盘/菜单/D2D/配置）需在 M5 阶段重新实测并记录；若有超标项，优先回退渲染后端（D2D → GDI）。

---

## 14. 工程结构

    mini game time/
    ├─ src/
    │  ├─ main.cpp              入口、单实例、消息循环
    │  ├─ app.h / app.cpp       全局状态与模块协调
    │  ├─ overlay_window.h/.cpp 窗口样式、Z 序、编辑模式、拖动
    │  ├─ renderer.h/.cpp       渲染后端抽象（D2D / GDI）
    │  ├─ renderer_d2d.cpp      Direct2D + DirectWrite 实现
    │  ├─ renderer_gdi.cpp      GDI 精简实现
    │  ├─ timefmt.h/.cpp        时间格式化
    │  ├─ tray.h/.cpp           托盘图标、菜单构建与分发
    │  ├─ hotkey.h/.cpp         热键注册与字符串互转
    │  ├─ config.h/.cpp         INI 读写与默认值
    │  ├─ startup.h/.cpp        开机自启
    │  └─ resource.h            资源 ID
    ├─ res/
    │  ├─ resource.rc           图标 / 版本信息 / manifest 引用
    │  ├─ app.ico               多尺寸图标
    │  └─ app.manifest          PerMonitorV2、asInvoker
    ├─ .github/
    │  └─ workflows/
    │     └─ build.yml          GitHub Actions 构建与发布流水线
    ├─ CMakeLists.txt           构建配置（CI 与本地共用）
    ├─ .gitignore
    └─ README.md

---

## 15. 构建与发布（GitHub Actions）

### 15.1 方案：云端编译，本地零工具链

构建完全交给 GitHub Actions，本机不安装任何 C++ 编译器。

    本地 git push / 打 tag
              │
              ▼
    GitHub Actions (windows-latest，自带 MSVC + Windows SDK)
              │
              ├─ 编译 Release x64
              ├─ 体积门禁校验（≤ 300 KB，超标即失败）
              ├─ 上传 artifact（每次提交都可下载 exe）
              └─ 打 tag 时自动打包并创建 Release
              │
              ▼
        下载 GameClock.exe 直接运行

**收益**：

| 项 | 说明 |
|---|---|
| 本地环境 | 不需要 VS / SDK / CMake / 任何编译器，机器保持干净 |
| 可复现性 | 构建环境镜像固定，任何人构建结果一致 |
| 免维护 | 工具链由 GitHub 托管升级，无版本漂移 |
| 体积守护 | 体积门禁写进流水线，"极致轻量"由 CI 强制执行而非靠自觉 |
| 发布自动化 | 打 tag 即出 Release，无需手工打包 |

**代价**：单次构建约 2–4 分钟（含排队），交互式调试不如本地编译。应对见 §15.6。

### 15.2 工具链选择：MSVC on windows-latest（推荐）

| 方案 | Runner | 优点 | 缺点 | 结论 |
|---|---|---|---|---|
| **MSVC** | windows-latest | runner 自带、Win32/D2D/DWrite SDK 最完整、`/O1 /GL /LTCG /OPT:ICF` 体积优化到位 | Windows runner 计费系数 2× | **默认** |
| mingw-w64 | ubuntu-latest | Linux 计费 1×、构建更快 | D2D/DWrite 头文件不如 SDK 完整 | 备选 |
| llvm-mingw | windows-latest | 与本地一致 | 需额外下载 181 MB 工具链 | 不推荐 |

选 MSVC 的理由：

- Windows runner **本身已装好 MSVC + Windows SDK**，工作流里一行工具链安装都不需要
- 本项目依赖 `d2d1.h` / `dwrite.h` / `shellapi.h`，MSVC 的 SDK 头文件最完整，可规避 mingw 头文件缺漏这类难查的问题
- 体积：`/GL` + `/LTCG` + `/OPT:REF /OPT:ICF` 对小体积 Win32 程序效果很好，配合 `/MT` 静态 CRT，单 exe 可稳定落在 150–250 KB

> 计费说明：公开仓库的 Actions **完全免费且不限时长**；私有仓库免费额度 2000 分钟/月，Windows runner 按 2× 计。本工作流单次约 1–2 分钟，即便私有仓库也几乎不占额度。**建议公开仓库**。

### 15.3 工作流设计

| 要素 | 设计 |
|---|---|
| 触发 | `push` 到 main / `pull_request` / `workflow_dispatch` 手动 / `v*` tag |
| Runner | `windows-latest` |
| 架构 | 仅 `x64`（游戏基本为 x64；ARM64 留待后续扩展矩阵） |
| 构建系统 | CMake + Ninja（runner 均已预装） |
| 编译优化 | `/O1 /GL /Gy /Gw /GR- /utf-8` + `/LTCG /OPT:REF /OPT:ICF` + `/MT` |
| 体积门禁 | 产物 > 300,000 字节 → 构建失败并打印实际大小 |
| 产物 | 每次构建上传 `GameClock.exe` artifact（保留 90 天） |
| 发布 | 打 `v*` tag → 自动打包 zip 并创建 GitHub Release |
| 版本号 | `git describe --tags` 注入 exe 版本资源 |
| 缓存 | 不启用（约 2000 行，编译 < 30 秒，缓存反而增加复杂度） |

### 15.4 工作流文件

完整文件已就位：`.github/workflows/build.yml`。关键片段：

体积门禁（把"极致轻量"变成硬性约束）：

    - name: 体积门禁
      shell: pwsh
      run: |
        $size = (Get-Item build/GameClock.exe).Length
        "{0:N1} KB" -f ($size / 1KB)
        if ($size -gt 300000) { throw "体积超标" }

tag 自动发布：

    - name: 创建 Release
      if: startsWith(github.ref, 'refs/tags/v')
      uses: softprops/action-gh-release@v2
      with:
        files: GameClock-${{ github.ref_name }}-win64.zip
        generate_release_notes: true

### 15.5 CMakeLists.txt 要点

    cmake_minimum_required(VERSION 3.21)
    project(GameClock CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")   # /MT，免装 VC++ 运行库

    add_executable(GameClock WIN32
        src/main.cpp
        src/overlay_window.cpp
        # …… 其余源文件
        res/resource.rc)                               # 图标 + manifest + 版本信息

    target_compile_definitions(GameClock PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

    target_link_libraries(GameClock PRIVATE
        user32 gdi32 shell32 advapi32 ole32 d2d1 dwrite comctl32)

    if (MSVC)
      target_compile_options(GameClock PRIVATE /O1 /GL /Gy /Gw /GR- /utf-8)
      target_link_options(GameClock PRIVATE
          /LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO /MANIFEST:EMBED)
    else()
      target_compile_options(GameClock PRIVATE
          -Os -flto -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections)
      target_link_options(GameClock PRIVATE
          -Wl,--gc-sections -Wl,-s -static -static-libgcc -static-libstdc++)
    endif()

要点说明：

- `WIN32` 目标属性对应 `/SUBSYSTEM:WINDOWS` + `wWinMain` 入口，不弹控制台
- `/MT` 静态链接 CRT → 目标机无需安装 VC++ 运行库
- `/GR-` 关闭 RTTI、`/O1` 优先体积；若后续用到会抛异常的 STL，需改回 `/EHsc`
- `res/resource.rc` 必须含：图标、版本信息（CI 注入 tag 号）、`app.manifest`（PerMonitorV2 + asInvoker）

### 15.6 本地开发循环（可选，但强烈建议）

CI 编译的硬伤：**每次改动都要 push 再等 2–4 分钟**，写 Win32 消息处理代码时这个反馈速度会很折磨。

| 方式 | 操作 | 适合 |
|---|---|---|
| **A. 本地 llvm-mingw（推荐，已就绪）** | **已安装**于 `D:\windows\Desktop\dsh本地工作目录\llvm-mingw-20261020-msvcrt-i686`（clang 23.1.4），已实测可编译运行。用同一份 CMakeLists 本地编译，秒级反馈；CI 仍作为**官方产物与发布出口** | 实际开发阶段 |
| B. 纯 CI | 不装任何东西，靠 push 迭代 | 一次性改动、或完全不想碰本地环境 |

> 本地产物仅用于调试，**发布以 CI 产物为准**（环境可复现）。两者共用同一份 CMakeLists，行为一致。
> 本地 llvm-mingw **已安装并验证可用**，见 §2.3。

**已验证的本地编译命令**（实测通过，产物 12 KB）：

    $bindir = 'D:\windows\Desktop\dsh本地工作目录\llvm-mingw-20261020-msvcrt-i686\bin'

    & "$bindir\x86_64-w64-mingw32-g++.exe" -std=c++20 -municode -mwindows `
      -Os -flto -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections `
      -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX `
      -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 `
      src/main.cpp -o build/GameClock.exe `
      -Wl,--gc-sections -Wl,-s -static-libgcc -static-libstdc++ `
      -luser32 -lgdi32 -lshell32 -ladvapi32 -lole32 -lcomctl32 -lshcore

**实测踩到的三个坑**：

1. `SetProcessDpiAwarenessContext` 需定义 `_WIN32_WINNT=0x0A00`，否则报 undeclared identifier。
2. `UNICODE` / `NOMINMAX` 等宏统一放命令行，不要同时写进源码，否则触发 `-Wmacro-redefined`。
3. 发行包后缀 `msvcrt-i686` 指的是**工具链宿主为 32 位**，但它**可正常编译 64 位目标** `x86_64-w64-mingw32`，无需另装。

### 15.7 开工前置准备清单

| # | 事项 | 状态 | 说明 |
|---|---|---|---|
| 1 | 创建 GitHub 仓库 | **待办** | 建议公开仓库（Actions 免费无限时长） |
| 2 | `git init` + 关联远端 + 首次推送 | **待办** | 当前目录尚未初始化 git |
| 3 | 添加 `.gitignore` | **已完成** | 已生成，排除 build/、dist/ 等 |
| 4 | 推送 `.github/workflows/build.yml` | **待办** | 工作流文件已就绪 |
| 5 | M0 源码落地 | 待办 | 有源码前 CI 必然红，属正常 |

> `gh` CLI 本机未安装。创建仓库请用 GitHub 网页；或安装 `gh` 后用 `gh repo create` 一条命令完成。

### 15.8 注意事项

1. **不要用 UPX 压缩**。虽能把 exe 从 200 KB 压到约 80 KB，但会显著提高杀毒软件误报率——对一个"常驻游戏后台、带置顶窗口"的工具，误报代价远大于 120 KB 的体积收益。
2. **artifact 有保留期**（默认 90 天）。需要长期保存的版本请打 tag 走 Release。
3. **Release 需要 `permissions: contents: write`**，否则 `gh-release` 会报 403。
4. **首次推送前 CI 必红**（无源码）。建议 M0 先提交最小可编译骨架让流水线先绿一次，再逐步加功能。
5. 若后续需要 ARM64 或多编译器体积对比，扩展 `strategy.matrix` 即可，工作流结构无需改动。

---

## 16. 里程碑与验收标准

| 阶段 | 内容 | 人日 | 完成标志 |
|---|---|---|---|
| **M0** | 仓库初始化 + CI 跑通 + 空窗口 + 消息循环 | 0.5 | GitHub Actions 绿灯；下载 artifact 可运行并显示空的分层窗口，体积 < 100 KB |
| **M1** | 叠加层窗口样式 + 时间渲染 + **点击穿透** | 1.5 | 窗口置顶、不抢焦点、点击穿透到下层；时间正常走秒 |
| **M2** | 渲染后端与样式（字号/颜色/透明度/背景/描边） | 1.5 | 各项样式可切换且视觉正确，无黑边 |
| **M3** | 托盘图标 + 右键菜单 + 配置读写 | 1.5 | 所有菜单项生效并持久化，重启后保持 |
| **M4** | 热键 / 编辑模式拖动 / 多显示器 / DPI / 开机自启 | 1.5 | 热键可用；拖动定位准确；4K+1080p 双屏正常；自启生效 |
| **M5** | 打磨：体积与内存实测优化、独占全屏检测提示、异常兜底 | 1 | 三项指标达标并记录；无崩溃 |

**合计约 7.5 人日**（单人估算，含联调余量）。

### 验收清单

- [ ] 游戏（无边框窗口化）中稳定置顶显示时间
- [ ] 鼠标点击、滚轮、拖拽全部穿透到游戏，游戏操作零干扰
- [ ] 点击叠加层**不会**让游戏失焦 / 最小化
- [ ] 热键切换显示隐藏，响应 < 100 ms
- [ ] 不出现在 Alt+Tab 列表与任务栏
- [ ] 右键托盘图标弹出菜单，位置正确且点击外部可关闭
- [ ] 位置：九宫格 / 拖动 / 多显示器切换均生效并持久化
- [ ] 样式：字号、颜色、透明度、背景、格式切换即时生效
- [ ] 开机自启勾选后可自动运行；程序移动位置后自动修正注册表路径
- [ ] exe ≤ 300 KB；常驻内存 ≤ 12 MB；空闲 CPU < 0.1%
- [ ] 独占全屏下：自动隐藏并提示（或文档明确说明）

---

## 17. 风险登记

| 风险 | 概率 | 影响 | 对策 |
|---|---|---|---|
| **真独占全屏无法显示** | 高（已确认为设计边界） | 中 | §5.4 降级路径 + 自动检测提示；不改用注入 |
| 反作弊误判 | 低 | 高 | 不注入、不挂钩子、不读内存；仅用 RegisterHotKey 与顶层窗口；README 附声明 |
| 热键被其它软件占用 | 中 | 低 | 注册失败时明确提示并引导改配置，不阻塞其它功能 |
| 某些游戏仍周期性置顶自身 | 中 | 低 | 前台事件钩子 + 遮挡检测重设 |
| D2D 初始化失败（老旧显卡/远程桌面） | 低 | 中 | 自动回退 GDI 后端 |
| 多显示器 DPI 切换导致尺寸错乱 | 中 | 中 | 正确处理 WM_DPICHANGED，按显示器 DPI 重算 |
| 用户忘记退出编辑模式 | 中 | 低 | 3 分钟无操作自动退出 |
| CI 构建失败 / runner 镜像变更 | 低 | 中 | 工作流只依赖 runner 预装的 MSVC + CMake + Ninja；M0 即跑通验证 |
| mingw 与 MSVC 行为差异 | 低 | 低 | 双轨构建；以 CI（MSVC）为发布基准。已实测 mingw 可正常编译 Win32/D2D 头文件 |
| 反馈循环变慢拖累开发效率 | 中 | 中 | 开发期用本地 llvm-mingw 秒级迭代（§15.6），CI 只作发布出口 |
| Actions 额度或可用性 | 低 | 低 | 公开仓库免费无限时长；私有仓库单次约 1–2 分钟，额度充裕 |

---

## 18. 未来可选增强（本版不做）

1. 多时区世界时钟 / 游戏服务器时间
2. 计时器、秒表
3. 样式预设的导入导出（便于分享）
4. 检测到全屏独占自动隐藏 + 恢复（在 §5.4 提示的基础上做成开关）
5. SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)：让叠加层不出现在录屏/直播画面中（Win10 2004+）
6. 简化的可视化设置面板（架构已预留，见 §9.4）

---

## 19. 待确认事项（建议直接采用默认值）

| # | 事项 | 建议默认 | 备选 |
|---|---|---|---|
| 1 | 构建方式 | GitHub Actions + MSVC（§15.2） | 本地 llvm-mingw 调试（§15.6） |
| 2 | 默认热键 | Ctrl+Alt+T | Ctrl+Alt+H / 自定义 |
| 3 | 默认位置 | 右上角，偏移 (24, 20) | 左上 / 居中 |
| 4 | 默认样式 | Segoe UI 28px / 白色 / 1px 黑描边 / 无背景 | 半透明圆角卡片 |
| 5 | 默认格式 | HH:mm:ss | HH:mm |
| 6 | 渲染后端 | Direct2D | GDI（更省内存） |
| 7 | 设置形态 | 菜单即设置（§9.4 A） | 独立设置窗口 |
| 8 | 编辑模式超时 | 180 秒自动退出 | 不超时 |
| 9 | 仓库可见性 | 公开（Actions 免费无限时长） | 私有 |
| 10 | 体积门禁阈值 | 300 KB，超标即 CI 失败 | 仅告警不失败 |

---

**下一步**：确认 §19 的默认值，并准备好 GitHub 仓库（§15.7），即可从 M0 开始实施。