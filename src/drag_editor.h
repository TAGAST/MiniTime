#pragma once

#include <windows.h>

// 编辑模式：允许拖动叠加层定位。
//
// 常态下窗口带 WS_EX_TRANSPARENT（点击穿透），收不到任何鼠标消息，
// 因此无法拖动。本模块负责临时切�换到可交互状态，并在退出时恢复。
class DragEditor {
public:
    DragEditor() = default;

    // 进入编辑模式；返回 false 表示失败
    bool Begin(HWND overlay, HWND host, int timeoutMs);

    // 退出编辑模式。commit=true 表示保留当前位置，false 表示还原
    void End(bool commit);

    bool IsActive() const { return active_; }

    // 供宿主窗口转发消息；返回 true 表示消息已被处理
    bool OnMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // 拖动结束后的最终位置
    POINT Result() const { return { currentX_, currentY_ }; }

    // 是否发生了实际的位移
    bool HasMoved() const { return moved_; }

private:
    void MoveTo(int x, int y);
    void ClampToScreen(int& x, int& y) const;

    HWND  overlay_    = nullptr;
    HWND  host_       = nullptr;
    bool  active_     = false;
    bool  dragging_   = false;
    bool  moved_      = false;

    POINT dragStartCursor_ = { 0, 0 };
    int   startX_ = 0;
    int   startY_ = 0;
    int   origX_  = 0;
    int   origY_  = 0;
    int   currentX_ = 0;
    int   currentY_ = 0;
    int   width_   = 0;
    int   height_  = 0;

    UINT_PTR timerId_ = 0;
    UINT     timeoutMs_ = 60000;
};