#pragma once

#include <windows.h>
#include <string>

// 时间显示格式（值语义，可自由拷贝）
struct TimeFormat {
    bool showSeconds = true;
    bool showDate    = false;
};

// 纯函数：不持有状态，不依赖窗口/渲染
namespace timefmt {

std::wstring Format(const SYSTEMTIME& st, const TimeFormat& fmt);

// 依据格式决定重绘间隔：显示秒时需高频刷新
UINT RefreshIntervalMs(const TimeFormat& fmt);

} // namespace timefmt
