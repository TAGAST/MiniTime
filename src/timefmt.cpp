#include "timefmt.h"

#include <cstdio>

namespace timefmt {

namespace {

// 把 0..23 转成 12 小时制的 1..12
int To12Hour(int hour24) {
    int h = hour24 % 12;
    if (h == 0) h = 12;
    return h;
}

const wchar_t* Meridiem(int hour24) {
    return hour24 < 12 ? L"AM" : L"PM";
}

} // namespace

std::wstring Format(const SYSTEMTIME& st, const TimeFormat& fmt) {
    wchar_t buf[64] = {};
    int used = 0;

    if (fmt.showDate) {
        const int n = swprintf(buf, 64, L"%04d-%02d-%02d  ",
                               static_cast<int>(st.wYear),
                               static_cast<int>(st.wMonth),
                               static_cast<int>(st.wDay));
        if (n > 0) used = n;
    }

    const int remain = 64 - used;
    if (remain <= 0) return std::wstring(buf);

    const int hour24 = static_cast<int>(st.wHour);
    const int minute = static_cast<int>(st.wMinute);
    const int second = static_cast<int>(st.wSecond);

    if (fmt.use24Hour) {
        if (fmt.showSeconds) {
            swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d:%02d",
                     hour24, minute, second);
        } else {
            swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d",
                     hour24, minute);
        }
        return std::wstring(buf);
    }

    // 12 小时制：小时补零，AM/PM 后置
    const int hour12 = To12Hour(hour24);
    if (fmt.showSeconds) {
        swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d:%02d %ls",
                 hour12, minute, second, Meridiem(hour24));
    } else {
        swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d %ls",
                 hour12, minute, Meridiem(hour24));
    }

    return std::wstring(buf);
}

UINT RefreshIntervalMs(const TimeFormat& fmt) {
    // 250ms 保证秒跳变延迟不超过 1/4 秒；不显示秒时每分钟一次即可
    return fmt.showSeconds ? 250u : 1000u;
}

} // namespace timefmt
