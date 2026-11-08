#include "app.h"

#include "autostart.h"
#include "resource.h"

#include <algorithm>

namespace {

constexpr UINT_PTR kRefreshTimerId = 1;

constexpr const wchar_t* kRouterClass = L"MiniTimeRouter";

} // namespace

App* App::s_instance = nullptr;

App::~App() {
    if (s_instance == this) s_instance = nullptr;
    Shutdown();
}

bool App::CreateRouterWindow() {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &App::RouterThunk;
    wc.hInstance     = instance_;
    wc.lpszClassName = kRouterClass;

    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    // 必须是普通窗口而非 HWND_MESSAGE：仅消息窗口收不到 TaskbarCreated 广播，
    // 那样 explorer.exe 重启后托盘图标就无法自动恢复。这里用不可见顶层窗口。
    router_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              kRouterClass, L"", WS_POPUP,
                              0, 0, 0, 0, nullptr, nullptr, instance_, this);
    return router_ != nullptr;
}

bool App::CreateTrayIcon() {
    trayCallback_ = RegisterWindowMessageW(L"MiniTimeTrayCallback");

    appIcon_ = LoadTraySizedIcon();
    if (!appIcon_) {
        appIcon_ = LoadIconW(nullptr, IDI_APPLICATION);
    }

    return trayIcon_.Add(router_, trayCallback_, appIcon_, L"MiniTime");
}

HICON App::LoadTraySizedIcon() const {
    // 托盘图标必须按 SM_CXSMICON 加载（通常 16px，150% DPI 下 24px）。
    // 用 LR_DEFAULTSIZE 会按 SM_CXICON 取 48px，导致图标撑满任务栏而无法正常显示。
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    // 不共享句柄：DPI 变化时需要按新尺寸重新加载
    HICON icon = static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                   cx, cy, LR_DEFAULTCOLOR));
    if (icon) return icon;

    // 回退：直接加载图标资源
    return static_cast<HICON>(
        LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                   cx, cy, 0));
}

void App::ReloadTrayIcon() {
    HICON fresh = LoadTraySizedIcon();
    if (!fresh) return;

    if (appIcon_ && appIcon_ != LoadIconW(nullptr, IDI_APPLICATION)) {
        DestroyIcon(appIcon_);
    }
    appIcon_ = fresh;

    if (!trayIcon_.ReplaceIcon(appIcon_)) {
        trayIcon_.ReAdd();
    }
}

void App::UpdateTrayTooltip() {
    const wchar_t* text = overlay_.IsVisible() ? L"MiniTime — 显示中" : L"MiniTime — 已隐藏";
    trayIcon_.SetTooltip(text);
}

void App::ApplyConfigToPresenter() {
    presenter_.SetStyle(config_.ToTextStyle());
    presenter_.SetLayout(config_.ToLayoutConfig());

    format_ = config_.timeFormat;
    presenter_.SetFormat(format_);
}

void App::SyncRefreshTimer() {
    if (!router_) return;

    const UINT next = timefmt::RefreshIntervalMs(format_);
    if (next == timerMs_ && timerMs_ != 0) return;

    timerMs_ = next;
    SetTimer(router_, kRefreshTimerId, timerMs_, nullptr);
}

void App::RenderNow() {
    if (overlay_.IsVisible()) {
        presenter_.Render(overlay_.Handle());
    }
}

int App::Run(HINSTANCE instance, int showCommand) {
    instance_ = instance;
    s_instance = this;
    (void)showCommand;

    config_ = config::Load();
    LoadGameProfiles();

    if (!overlay_.Create(instance)) return 1;
    if (!CreateRouterWindow()) return 1;

    ApplyConfigToPresenter();
    presenter_.Render(overlay_.Handle());

    timerMs_ = timefmt::RefreshIntervalMs(format_);
    SetTimer(router_, kRefreshTimerId, timerMs_, nullptr);

    hotkeys_.SetWindow(router_);
    RegisterHotkeysFromConfig();

    CreateTrayIcon();
    UpdateTrayTooltip();
    autostart::SyncPathIfEnabled();

    overlay_.SetMessageHook(&App::OverlayMessageThunk, this);

    // 监听前台变化，用于按游戏自动切换位置
    foregroundHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, &App::WinEventThunk, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // 启动时前台可能已经是目标游戏，主动检测一次，
    // 否则要等到下一次切换才会套用配置。
    OnForegroundChanged();

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    Shutdown();
    return 0;
}

LRESULT CALLBACK App::RouterThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    App* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->Route(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::Route(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // explorer.exe 重启后恢复托盘图标
    if (msg != 0 && msg == trayIcon_.TaskbarCreatedMessage()) {
        trayIcon_.ReAdd();
        UpdateTrayTooltip();
        return 0;
    }

    if (msg == trayCallback_) {
        const UINT event = LOWORD(lParam);
        if (event == WM_LBUTTONUP || event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            ShowTrayMenu();
        }
        return 0;
    }

    switch (msg) {
    case WM_TIMER:
        if (wParam == kRefreshTimerId) { OnTick(); return 0; }
        break;

    case WM_HOTKEY: {
        HotkeyId id = HotkeyId::Toggle;
        if (HotkeyRegistry::FromMessageId(wParam, id)) {
            if (id == HotkeyId::Edit) {
                EnterEditMode();
            } else {
                OnToggleVisibility();
            }
        }
        return 0;
    }

    case WM_APP + 1:
        // 第二个实例启动时请求：把叠加层重新显示出来
        EnsureVisibleAndTopmost();
        return 0;

    case WM_DISPLAYCHANGE:
        RenderNow();
        return 0;

    case WM_DPICHANGED:
        // DPI 变化后托盘图标需要按新尺寸重新加载
        ReloadTrayIcon();
        RenderNow();
        return 0;

    case WM_TIMECHANGE:
        RenderNow();
        return 0;

    case WM_POWERBROADCAST:
        if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            RenderNow();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void App::ShowTrayMenu() {
    POINT pt = {};
    GetCursorPos(&pt);

    TrayMenuState state = {};
    state.visible   = overlay_.IsVisible();
    state.autostart = autostart::IsEnabled();
    state.config    = config_;
    state.toggleConflicted = hotkeys_.IsConflicted(HotkeyId::Toggle);
    state.editConflicted   = hotkeys_.IsConflicted(HotkeyId::Edit);
    state.profiles         = profiles_;
    state.foreground       = gameprofile::QueryForeground();
    state.activeProfileProcess = activeProfile_;

    // 记录下来：菜单关闭后前台会变化，保存/删除必须用这一刻的窗口
    menuForeground_ = state.foreground;

    const MenuCommand command = trayMenu_.Show(router_, pt, state);
    HandleMenuCommand(command);
}

void App::HandleMenuCommand(MenuCommand command) {
    if (command == MenuCommand::None) return;

    switch (command) {
    case MenuCommand::ToggleShow:
        OnToggleVisibility();
        return;

    case MenuCommand::Autostart: {
        const bool next = !autostart::IsEnabled();
        autostart::SetEnabled(next);
        return;
    }

    case MenuCommand::Exit:
        DestroyWindow(router_);
        return;

    case MenuCommand::EnterEditMode:
        EnterEditMode();
        return;

    case MenuCommand::SaveGamePosition:
        SaveCurrentPositionToGame(menuForeground_);
        return;

    case MenuCommand::DeleteGamePosition:
        DeleteCurrentGameProfile(menuForeground_);
        return;

    default:
        break;
    }

    // 已保存游戏的条目
    {
        const int value = static_cast<int>(command);
        if (value >= static_cast<int>(MenuCommand::ProfileBase) &&
            value <= static_cast<int>(MenuCommand::ProfileBaseEnd)) {
            const size_t index = static_cast<size_t>(
                value - static_cast<int>(MenuCommand::ProfileBase));
            SelectProfile(index);
            return;
        }
    }

    // 样式与位置类命令：修改配置 -> 立即生效 -> 持久化
    if (config::ApplyCommand(config_, command)) {
        ApplyConfigToPresenter();
        RenderNow();
        config::Save(config_);

        // 热键类命令需要重新注册才能生效
        if (IsHotkeyCommand(command)) {
            RegisterHotkeysFromConfig();
        }

        // 时间格式变化会影响刷新频率（显示秒时更快）
        if (IsFormatCommand(command)) {
            SyncRefreshTimer();
        }
    }
}

void App::OnDragMovedThunk(void* context) {
    auto* self = static_cast<App*>(context);
    if (!self) return;

    // 位置已由 DragEditor 设置，这里只重绘内容
    self->presenter_.Render(self->overlay_.Handle());
}

bool App::OverlayMessageThunk(void* context, HWND hwnd, UINT msg,
                              WPARAM wParam, LPARAM lParam) {
    auto* self = static_cast<App*>(context);
    if (!self) return false;
    return self->dragEditor_.OnMessage(hwnd, msg, wParam, lParam);
}

void App::EnterEditMode() {
    if (dragEditor_.IsActive()) {
        ExitEditMode(true);
        return;
    }

    // 1 分钟无操作自动退出，避免忘记退出后游戏无法操作
    constexpr int kEditTimeoutMs = 60 * 1000;
    if (!dragEditor_.Begin(overlay_.Handle(), router_, kEditTimeoutMs)) {
        return;
    }

    // 编辑期间由拖动接管位置，渲染层不得覆写
    presenter_.SetLayoutSuspended(true);

    // 半透明底板 + 边框：便于识别拖动区域，并让整块区域可接收鼠标
    presenter_.SetEditAppearance(true);
    presenter_.SetEditHint(L"移动中，按回车固定");

    // 位置变化时立即重绘，避免内容滞后于光标
    dragEditor_.SetMovedCallback(&App::OnDragMovedThunk, this);

    // 回车 / ESC / 超时会由 DragEditor 内部结束会话，必须在此恢复外观
    dragEditor_.SetFinishedCallback(&App::OnEditFinishedThunk, this);

    overlay_.SetClickThrough(false);
    overlay_.SetFocusable(true);

    // 应用编辑外观后需要重新计算窗口尺寸
    presenter_.InvalidateGeometry();
    RenderNow();

    UpdateTrayTooltip();
}

void App::ExitEditMode(bool commit) {
    // 会话可能已由 DragEditor 内部结束（回车/ESC/超时），此处仍需完成清理，
    // 因此不能以 IsActive() 作为唯一前提。
    const bool wasActive = dragEditor_.IsActive();

    // 以窗口实际位置为准，避免内部记录与真实几何不一致
    const RECT actual = overlay_.Bounds();
    const POINT pos = { actual.left, actual.top };
    const bool moved = dragEditor_.HasMoved();

    if (wasActive) {
        dragEditor_.End(commit);   // 内部会回调 OnEditFinishedThunk
        return;                    // 清理交给回调完成，避免重复执行
    }

    dragEditor_.SetMovedCallback(nullptr, nullptr);
    dragEditor_.SetFinishedCallback(nullptr, nullptr);

    RestoreAfterEdit(commit, pos, moved);
}

void App::RestoreAfterEdit(bool commit, POINT pos, bool moved) {
    // 恢复点击穿透与不可获得焦点的行为
    overlay_.SetClickThrough(true);
    overlay_.SetFocusable(false);
    presenter_.SetLayoutSuspended(false);
    presenter_.SetEditAppearance(false);

    if (commit && moved) {
        config_.anchor  = Anchor::Custom;
        config_.customX = pos.x;
        config_.customY = pos.y;
        config::Save(config_);
    }

    // 位置变化后需让渲染层重新同步窗口几何
    presenter_.InvalidateGeometry();
    ApplyConfigToPresenter();
    RenderNow();
    UpdateTrayTooltip();
}

void App::OnEditFinishedThunk(void* context, bool commit) {
    auto* self = static_cast<App*>(context);
    if (!self) return;

    const POINT pos = self->ComputeCommittedPosition();
    const bool moved = self->dragEditor_.HasMoved();

    self->dragEditor_.SetMovedCallback(nullptr, nullptr);
    self->dragEditor_.SetFinishedCallback(nullptr, nullptr);

    self->RestoreAfterEdit(commit, pos, moved);
}

POINT App::ComputeCommittedPosition() const {
    // 编辑模式与常态的窗口尺寸、文字位置都不同，直接保存左上角会让文字跳动。
    // 正确的基准是"主文字"：让保存前后的文字屏幕位置完全一致。
    const RECT editRect = overlay_.Bounds();

    const POINT normalOffset = presenter_.NormalTextOffset();

    // 编辑模式下的窗口左上角
    const int editTextX = editRect.left;
    const int editTextY = editRect.top;

    // 文字在编辑底板中的位置
    POINT editOffset = presenter_.MainTextOffset();

    // 编辑模式文字水平居中，居中量必须以"当前真实窗口宽度"为基准：
    // lastWidth_ 可能还是上一帧的值（例如刚进入编辑模式的过渡帧），
    // 直接用会导致水平方向偏移。
    if (presenter_.EditAppearance()) {
        const int editWidth = editRect.right - editRect.left;
        const int textWidth = presenter_.MainTextWidth();
        if (textWidth > 0 && editWidth > textWidth) {
            editOffset.x = (editWidth - textWidth) / 2;
        }
    }

    // 反推常态窗口左上角：使文字落在同一屏幕坐标
    POINT pos = { 0, 0 };
    pos.x = editTextX + editOffset.x - normalOffset.x;
    pos.y = editTextY + editOffset.y - normalOffset.y;

    return pos;
}

bool App::IsFormatCommand(MenuCommand command) {
    const int value = static_cast<int>(command);
    return value >= 3001 && value <= 3004;
}

bool App::IsHotkeyCommand(MenuCommand command) {
    const int value = static_cast<int>(command);
    return value >= 2001 && value <= 2106;
}

void App::RegisterHotkeysFromConfig() {
    // 两个热键不允许绑定成同一个键
    if (!config_.hotkeyToggle.IsEmpty() && !config_.hotkeyEdit.IsEmpty() &&
        config_.hotkeyToggle.modifiers == config_.hotkeyEdit.modifiers &&
        config_.hotkeyToggle.virtualKey == config_.hotkeyEdit.virtualKey) {
        // 冲突时保留切换热键，禁用调整热键，避免两者都失效
        config_.hotkeyEdit = HotkeySpec{};
        config::Save(config_);
    }

    hotkeys_.Register(HotkeyId::Toggle, config_.hotkeyToggle);
    hotkeys_.Register(HotkeyId::Edit, config_.hotkeyEdit);
}

void App::LoadGameProfiles() {
    profiles_ = config::LoadProfiles();
}

void App::ApplyActiveLayout() {
    // 有生效的游戏配置就用它，否则用全局配置
    if (!activeProfile_.empty()) {
        for (const auto& profile : profiles_) {
            if (gameprofile::SameProcess(profile.processName, activeProfile_)) {
                LayoutConfig layout = config_.ToLayoutConfig();
                layout.anchor  = profile.anchor;
                layout.customX = profile.customX;
                layout.customY = profile.customY;
                layout.marginX = profile.marginX;
                layout.marginY = profile.marginY;

                presenter_.SetLayout(layout);
                presenter_.InvalidateGeometry();
                RenderNow();
                return;
            }
        }
    }

    ApplyConfigToPresenter();
    presenter_.InvalidateGeometry();
    RenderNow();
}

void App::ApplyProfileFor(const std::wstring& processName) {
    // 查找匹配的游戏配置
    std::wstring matched;
    for (const auto& profile : profiles_) {
        if (gameprofile::SameProcess(profile.processName, processName)) {
            matched = profile.processName;
            break;
        }
    }

    if (matched == activeProfile_) return;   // 无变化

    activeProfile_ = matched;
    ApplyActiveLayout();
}

void App::OnForegroundChanged() {
    const ForegroundInfo info = gameprofile::QueryForeground();

    if (!info.valid) return;

    // 自己的窗口不算
    if (gameprofile::SameProcess(info.processName, L"MiniTime.exe")) return;

    ApplyProfileFor(info.processName);
}

void App::WinEventThunk(HWINEVENTHOOK, DWORD event, HWND, LONG, LONG, DWORD, DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND) return;

    // 回调运行在外部事件上下文，通过全局单例转发
    if (App::s_instance) App::s_instance->OnForegroundChanged();
}

void App::SaveCurrentPositionToGame(const ForegroundInfo& info) {
    if (!info.valid) return;

    // 记录当前叠加层左上角。此时不在编辑模式，窗口尺寸即常态尺寸，
    // 该坐标可直接作为 customX/customY 使用。
    const RECT actual = overlay_.Bounds();
    const int x = actual.left;
    const int y = actual.top;

    // 已存在则更新
    for (auto& profile : profiles_) {
        if (gameprofile::SameProcess(profile.processName, info.processName)) {
            profile.anchor  = Anchor::Custom;
            profile.customX = x;
            profile.customY = y;
            config::SaveProfiles(profiles_);
            LoadGameProfiles();
            return;
        }
    }

    GameProfile profile;
    profile.processName = gameprofile::NormalizeProcessName(info.processName);
    profile.displayName = gameprofile::ToDisplayName(profile.processName);
    profile.anchor  = Anchor::Custom;
    profile.customX = x;
    profile.customY = y;
    profiles_.push_back(profile);

    config::SaveProfiles(profiles_);
    LoadGameProfiles();
}

void App::DeleteCurrentGameProfile(const ForegroundInfo& info) {
    if (!info.valid) return;

    const auto before = profiles_.size();
    profiles_.erase(
        std::remove_if(profiles_.begin(), profiles_.end(),
                       [&](const GameProfile& p) {
                           return gameprofile::SameProcess(p.processName, info.processName);
                       }),
        profiles_.end());

    if (profiles_.size() != before) {
        config::SaveProfiles(profiles_);
        LoadGameProfiles();

        if (gameprofile::SameProcess(activeProfile_, info.processName)) {
            activeProfile_.clear();
            ApplyActiveLayout();
        }
    }
}

void App::SelectProfile(size_t index) {
    if (index >= profiles_.size()) return;

    // 立即套用并记录为生效配置
    activeProfile_ = profiles_[index].processName;
    ApplyActiveLayout();
}

void App::OnTick() {
    RenderNow();
}

void App::EnsureVisibleAndTopmost() {
    overlay_.SetVisible(true);
    overlay_.RaiseTopmost();
    ApplyConfigToPresenter();
    RenderNow();
    UpdateTrayTooltip();
}

void App::OnToggleVisibility() {
    overlay_.ToggleVisibility();
    if (overlay_.IsVisible()) {
        overlay_.RaiseTopmost();
        RenderNow();
    }
    UpdateTrayTooltip();
}

void App::Shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;

    if (router_) {
        KillTimer(router_, kRefreshTimerId);
    }
    hotkeys_.UnregisterAll();
    trayIcon_.Remove();

    if (foregroundHook_) {
        UnhookWinEvent(foregroundHook_);
        foregroundHook_ = nullptr;
    }

    // 必须先销毁叠加层：分层窗口的像素由 DWM 持有，
    // 若进程退出时窗口仍在，桌面会残留最后一次绘制的内容。
    overlay_.Destroy();

    if (router_) {
        DestroyWindow(router_);
        router_ = nullptr;
    }
}