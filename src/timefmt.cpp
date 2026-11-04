#include "timefmt.h"

#include <cstdio>

namespace timefmt {

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
    if (remain > 0) {
        if (fmt.showSeconds) {
            swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d:%02d",
                     static_cast<int>(st.wHour),
                     static_cast<int>(st.wMinute),
                     static_cast<int>(st.wSecond));
        } else {
            swprintf(buf + used, static_cast<size_t>(remain), L"%02d:%02d",
                     static_cast<int>(st.wHour),
                     static_cast<int>(st.wMinute));
        }
    }

    return std::wstring(buf);
}

UINT RefreshIntervalMs(const TimeFormat& fmt) {
    // 250ms 保证秒跳变延迟不超过 1/4 秒；不显示秒时每分钟一次即可
    return fmt.showSeconds ? 250u : 1000u;
}

} // namespace timefmt
